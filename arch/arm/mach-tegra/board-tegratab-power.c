/*
 * arch/arm/mach-tegra/board-tegratab-power.c
 *
 * Copyright (C) 2012-2013 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/i2c.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/palmas.h>
#include <linux/power/bq2419x-charger.h>
#include <linux/max17048_battery.h>
#include <linux/power/power_supply_extcon.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/regulator/userspace-consumer.h>
#include <linux/edp.h>
#include <linux/edpdev.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/platform_data/ina230.h>

#include <asm/mach-types.h>
#include <linux/power/sbs-battery.h>

#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/hardware.h>
#include <mach/edp.h>
#include <mach/gpio-tegra.h>

#include "cpu-tegra.h"
#include "pm.h"
#include "tegra-board-id.h"
#include "board-pmu-defines.h"
#include "board.h"
#include "gpio-names.h"
#include "board-common.h"
#include "board-tegratab.h"
#include "tegra_cl_dvfs.h"
#include "devices.h"
#include "tegra11_soctherm.h"
#include "tegra3_tsensor.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

/* BQ2419X VBUS regulator */
static struct regulator_consumer_supply tegratab_bq2419x_vbus_supply[] = {
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-otg"),
};

static struct regulator_consumer_supply tegratab_bq2419x_batt_supply[] = {
	REGULATOR_SUPPLY("usb_bat_chg", "tegra-udc.0"),
};

static struct bq2419x_vbus_platform_data tegratab_bq2419x_vbus_pdata = {
	.gpio_otg_iusb = TEGRA_GPIO_PI4,
	.num_consumer_supplies = ARRAY_SIZE(tegratab_bq2419x_vbus_supply),
	.consumer_supplies = tegratab_bq2419x_vbus_supply,
};

struct bq2419x_charger_platform_data tegratab_bq2419x_charger_pdata = {
	.update_status = max17048_battery_status,
	.battery_check = max17048_check_battery,
	.soc_check = max17048_check_soc,
	.vcell_check = max17048_check_vcell,
	.current_check = ina230_get_current,
	.max_charge_current_mA = 3000,
	.charging_term_current_mA = 100,
	.consumer_supplies = tegratab_bq2419x_batt_supply,
	.num_consumer_supplies = ARRAY_SIZE(tegratab_bq2419x_batt_supply),
	.wdt_timeout = 40,
	.chg_restart_time = 1800,	/* 30 min */
	.chg_complete_soc = 100,
};

#ifndef CONFIG_OF
struct max17048_battery_model tegratab_max17048_mdata = {
	.rcomp		= 105,
	.soccheck_A	= 233,
	.soccheck_B	= 235,
	.bits		= 19,
	.alert_threshold = 0x01,	/* 1% SOC */
	.one_percent_alerts = 0x40,
	.alert_on_reset = 0x00,		/* not use */
	.rcomp_seg	= 0x0080,
	.hibernate	= 0x3080,
	.vreset		= 0x3c96,
	.valert		= 0xA2FF,	/*VALRT.MIN 3.24V, VALRT.MAX 5.1V*/
	.ocvtest	= 55824,
	.t_co_hot	= -1125,	/* -1.125 */
	.t_co_cold	= -7625,	/* -7.625 */
	.data_tbl = {
		0xAA, 0x30, 0xB7, 0x80, 0xB8, 0xD0, 0xBA, 0xC0,
		0xBB, 0xC0, 0xBC, 0x30, 0xBC, 0xB0, 0xBD, 0x60,
		0xBE, 0x10, 0xBF, 0x40, 0xC0, 0x30, 0xC3, 0x80,
		0xC5, 0x00, 0xC7, 0x60, 0xCB, 0x50, 0xD0, 0x10,
		0x03, 0x00, 0x30, 0xC0, 0x1C, 0x40, 0x2C, 0x00,
		0x70, 0xC0, 0x51, 0x40, 0x43, 0xE0, 0x43, 0xC0,
		0x29, 0xC0, 0x2F, 0xC0, 0x17, 0xE0, 0x12, 0xE0,
		0x1C, 0x20, 0x13, 0x80, 0x0E, 0x00, 0x0E, 0x00,
	},
};

struct max17048_platform_data tegratab_max17048_pdata = {
	.model_data = &tegratab_max17048_mdata,
	.read_batt_id = 1;
	.set_current_threshold = ina230_set_current_threshold,
	.current_normal = TEGRATAB_BATTERY_MAX_CURRENT,
	.current_threshold_num = 2,
	.current_threshold_soc = {4, 8},
	.current_threshold = {1800, 2500},
	.sysedp_throttle = sysedp_lite_throttle,
	.sysedp_throttle_num = 2,
	.sysedp_throttle_soc = {4, 8},
	.sysedp_throttle_power = {10000, 10500},
};

static struct i2c_board_info __initdata tegratab_max17048_boardinfo[] = {
	{
		I2C_BOARD_INFO("max17048", 0x36),
		.platform_data	= &tegratab_max17048_pdata,
	},
};
#endif

struct bq2419x_platform_data tegratab_bq2419x_pdata = {
	.vbus_pdata = &tegratab_bq2419x_vbus_pdata,
	.bcharger_pdata = &tegratab_bq2419x_charger_pdata,
};

static struct i2c_board_info __initdata tegratab_bq2419x_boardinfo[] = {
	{
		I2C_BOARD_INFO("bq2419x", 0x6b),
		.platform_data = &tegratab_bq2419x_pdata,
	},
};

static struct power_supply_extcon_plat_data psy_extcon_pdata = {
	.extcon_name = "tegra-udc",
};

static struct platform_device psy_extcon_device = {
	.name = "power-supply-extcon",
	.id = -1,
	.dev = {
		.platform_data = &psy_extcon_pdata,
	},
};

/************************ Tegratab based regulator ****************/
static struct regulator_consumer_supply palmas_smps123_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply palmas_smps45_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.3"),
};

static struct regulator_consumer_supply palmas_smps6_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_hv", NULL),
	REGULATOR_SUPPLY("avdd_lcd", NULL),
};

static struct regulator_consumer_supply palmas_smps7_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr", NULL),
};

static struct regulator_consumer_supply palmas_smps8_supply[] = {
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("vdd_emmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("vlogic", "0-0069"),
	REGULATOR_SUPPLY("vid", "0-000d"),
	REGULATOR_SUPPLY("vddio", "0-0078"),
	REGULATOR_SUPPLY("vdd", "0-004c"),
};

static struct regulator_consumer_supply palmas_smps9_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("vddio_hv", "tegradc.1"),
	REGULATOR_SUPPLY("pwrdet_hv", NULL),
};

static struct regulator_consumer_supply palmas_smps10_supply[] = {
};

static struct regulator_consumer_supply palmas_ldo1_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "vi"),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu", NULL),
	REGULATOR_SUPPLY("avdd_plla_p_c", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
	REGULATOR_SUPPLY("avdd_plle", NULL),
};

static struct regulator_consumer_supply palmas_ldo2_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.2"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
};

static struct regulator_consumer_supply palmas_ldo3_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
};

static struct regulator_consumer_supply palmas_ldo4_supply[] = {
	REGULATOR_SUPPLY("avdd", "spi0.0"),
};

static struct regulator_consumer_supply palmas_ldo5_supply[] = {
#ifdef CONFIG_USE_OF
	REGULATOR_SUPPLY("ext_vcm_vdd", "2-0010"),
#else
	REGULATOR_SUPPLY("vdd_af_cam1", NULL),
#endif
	REGULATOR_SUPPLY("vdd", "2-000c"),
	REGULATOR_SUPPLY("vana", "2-0048"),
	REGULATOR_SUPPLY("vana", "2-0021"),
};

static struct regulator_consumer_supply palmas_ldo6_supply[] = {
	REGULATOR_SUPPLY("vdd", "0-0069"),
	REGULATOR_SUPPLY("vdd", "0-0010"),
	REGULATOR_SUPPLY("vdd", "0-000d"),
	REGULATOR_SUPPLY("vdd", "0-0078"),
};

static struct regulator_consumer_supply palmas_ldo7_supply[] = {
	REGULATOR_SUPPLY("avdd", "2-0010"),
};
static struct regulator_consumer_supply palmas_ldo8_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};
static struct regulator_consumer_supply palmas_ldo9_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};
static struct regulator_consumer_supply palmas_ldoln_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
};

static struct regulator_consumer_supply palmas_ldousb_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-ehci.2"),

};

static struct regulator_consumer_supply palmas_regen1_supply[] = {
};

static struct regulator_consumer_supply palmas_regen2_supply[] = {
};

PALMAS_REGS_PDATA(smps123, 900,  1350, NULL, 0, 0, 0, 0,
	0, PALMAS_EXT_CONTROL_ENABLE1, 0, 3, 0);
PALMAS_REGS_PDATA(smps45, 900,  1400, NULL, 0, 0, 0, 0,
	0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(smps6, 3160,  3160, NULL, 0, 0, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps7, 1380,  1380, NULL, 0, 0, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps8, 1800,  1800, NULL, 1, 1, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps9, 2900,  2900, NULL, 0, 0, 1, NORMAL,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(smps10, 5000,  5000, NULL, 0, 0, 0, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo1, 1050,  1050, palmas_rails(smps7), 1, 0, 1, 0,
	0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(ldo2, 1200,  1200, palmas_rails(smps7), 0, 1, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo3, 1800,  1800, NULL, 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo4, 3000,  3000, NULL, 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo5, 2800,  2800, palmas_rails(smps9), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo6, 2850,  2850, palmas_rails(smps9), 0, 1, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo7, 2700,  2700, palmas_rails(smps9), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo8, 950,  950, NULL, 1, 1, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldo9, 1800,  2900, palmas_rails(smps9), 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldoln, 3300,   3300, NULL, 0, 0, 1, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(ldousb, 3300,  3300, NULL, 0, 0, 1, 0,
	0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REGS_PDATA(regen1, 4200,  4200, NULL, 0, 0, 0, 0,
	0, 0, 0, 0, 0);
PALMAS_REGS_PDATA(regen2, 4200,  4200, palmas_rails(smps8), 0, 0, 0, 0,
	0, 0, 0, 0, 0);

#define PALMAS_REG_PDATA(_sname) (&reg_idata_##_sname)
static struct regulator_init_data *tegratab_reg_data[PALMAS_NUM_REGS] = {
	NULL,
	PALMAS_REG_PDATA(smps123),
	NULL,
	PALMAS_REG_PDATA(smps45),
	NULL,
	PALMAS_REG_PDATA(smps6),
	PALMAS_REG_PDATA(smps7),
	PALMAS_REG_PDATA(smps8),
	PALMAS_REG_PDATA(smps9),
	PALMAS_REG_PDATA(smps10),
	PALMAS_REG_PDATA(ldo1),
	PALMAS_REG_PDATA(ldo2),
	PALMAS_REG_PDATA(ldo3),
	PALMAS_REG_PDATA(ldo4),
	PALMAS_REG_PDATA(ldo5),
	PALMAS_REG_PDATA(ldo6),
	PALMAS_REG_PDATA(ldo7),
	PALMAS_REG_PDATA(ldo8),
	PALMAS_REG_PDATA(ldo9),
	PALMAS_REG_PDATA(ldoln),
	PALMAS_REG_PDATA(ldousb),
	PALMAS_REG_PDATA(regen1),
	PALMAS_REG_PDATA(regen2),
	NULL,
	NULL,
	NULL,
};

#define PALMAS_REG_INIT_DATA(_sname) (&reg_init_data_##_sname)
static struct palmas_reg_init *tegratab_reg_init[PALMAS_NUM_REGS] = {
	NULL,
	PALMAS_REG_INIT_DATA(smps123),
	NULL,
	PALMAS_REG_INIT_DATA(smps45),
	NULL,
	PALMAS_REG_INIT_DATA(smps6),
	PALMAS_REG_INIT_DATA(smps7),
	PALMAS_REG_INIT_DATA(smps8),
	PALMAS_REG_INIT_DATA(smps9),
	PALMAS_REG_INIT_DATA(smps10),
	PALMAS_REG_INIT_DATA(ldo1),
	PALMAS_REG_INIT_DATA(ldo2),
	PALMAS_REG_INIT_DATA(ldo3),
	PALMAS_REG_INIT_DATA(ldo4),
	PALMAS_REG_INIT_DATA(ldo5),
	PALMAS_REG_INIT_DATA(ldo6),
	PALMAS_REG_INIT_DATA(ldo7),
	PALMAS_REG_INIT_DATA(ldo8),
	PALMAS_REG_INIT_DATA(ldo9),
	PALMAS_REG_INIT_DATA(ldoln),
	PALMAS_REG_INIT_DATA(ldousb),
	PALMAS_REG_INIT_DATA(regen1),
	PALMAS_REG_INIT_DATA(regen2),
	NULL,
	NULL,
	NULL,
};

static struct palmas_pmic_platform_data pmic_platform = {
	.enable_ldo8_tracking = true,
	.disabe_ldo8_tracking_suspend = true,
	.disable_smps10_boost_suspend = true,
};


#define PALMAS_GPADC_IIO_MAP(_ch, _dev_name, _name)		\
	{							\
		.adc_channel_label = _ch,			\
		.consumer_dev_name = _dev_name,			\
		.consumer_channel = _name,			\
	}

static struct iio_map palmas_adc_iio_maps[] = {
	PALMAS_GPADC_IIO_MAP("IN0", "0-0036", "batt_id"),
	PALMAS_GPADC_IIO_MAP("IN1", "generic-adc-thermal.0", "thermistor"),
	PALMAS_GPADC_IIO_MAP("IN3", "generic-adc-thermal.1", "tdiode"),
	PALMAS_GPADC_IIO_MAP(NULL, NULL, NULL),
};

static struct palmas_gpadc_platform_data palmas_adc_pdata = {
	.ch0_current_uA = 5,
	/* If ch3_dual_current is true, it will measure ch3 input signal with
	 * ch3_current and the next current of ch3_current.
	 * So this system will use 400uA and 800uA for ch3 measurement. */
	.ch3_current_uA = 400,	/* 0uA, 10uA, 400uA, 800uA */
	.ch3_dual_current = true,
	.extended_delay = true,
	.iio_maps = palmas_adc_iio_maps,
};

static struct palmas_pinctrl_config palmas_pincfg[] = {
	PALMAS_PINMUX(POWERGOOD, POWERGOOD, DEFAULT, DEFAULT),
	PALMAS_PINMUX(VAC, VAC, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO0, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO1, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO2, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO3, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO4, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO5, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO6, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO7, GPIO, DEFAULT, DEFAULT),
};

static struct palmas_pinctrl_platform_data palmas_pinctrl_pdata = {
	.pincfg = palmas_pincfg,
	.num_pinctrl = ARRAY_SIZE(palmas_pincfg),
	.dvfs1_enable = true,
	.dvfs2_enable = false,
};

static struct palmas_extcon_platform_data palmas_extcon_pdata = {
	.connection_name = "palmas-extcon",
	.enable_vbus_detection = true,
	.enable_id_pin_detection = true,
};

static struct palmas_platform_data palmas_pdata = {
	.gpio_base = PALMAS_TEGRA_GPIO_BASE,
	.irq_base = PALMAS_TEGRA_IRQ_BASE,
	.pmic_pdata = &pmic_platform,
	.adc_pdata = &palmas_adc_pdata,
	.use_power_off = true,
	.pinctrl_pdata = &palmas_pinctrl_pdata,
	.extcon_pdata = &palmas_extcon_pdata,
	.long_press_delay = PALMAS_LONG_PRESS_KEY_TIME_8SECONDS,
	.poweron_lpk = PALMAS_SWOFF_COLDRST_PWRON_LPK_RESTART,
};

struct palmas_clk32k_init_data tegratab_palmas_clk32k_idata[] = {
	{
		.clk32k_id = PALMAS_CLOCK32KG,
		.enable = true,
	}
};

static struct i2c_board_info palma_device[] = {
	{
		I2C_BOARD_INFO("tps65913", 0x58),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &palmas_pdata,
	},
};

static struct regulator_consumer_supply fixed_reg_dvdd_lcd_1v8_supply[] = {
	REGULATOR_SUPPLY("dvdd_lcd", NULL),
};

static struct regulator_consumer_supply fixed_reg_vdd_lcd_bl_en_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_bl_en", NULL),
};

/* EN_1V8_TS From TEGRA_GPIO_PH4 */
static struct regulator_consumer_supply fixed_reg_dvdd_ts_supply[] = {
	REGULATOR_SUPPLY("dvdd", "spi0.0"),
};

/* ENABLE 5v0 for HDMI */
static struct regulator_consumer_supply fixed_reg_vdd_hdmi_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.1"),
};

static struct regulator_consumer_supply fixed_reg_vddio_sd_slot_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.2"),
};

static struct regulator_consumer_supply fixed_reg_vd_cam_1v8_supply[] = {
	REGULATOR_SUPPLY("dovdd", "2-0010"),
	REGULATOR_SUPPLY("vif2", "2-0048"),
	REGULATOR_SUPPLY("vif2", "2-0021"),
	REGULATOR_SUPPLY("vddio_cam", "vi"),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_lcd_1v8_supply[] = {
	REGULATOR_SUPPLY("dvdd_lcd", NULL),
	REGULATOR_SUPPLY("dvdd", "spi0.0"),
};

/* Macro for defining fixed regulator sub device data */
#define FIXED_SUPPLY(_name) "fixed_reg_"#_name
#define FIXED_REG(_id, _var, _name, _in_supply, _always_on, _boot_on,	\
	_gpio_nr, _open_drain, _active_high, _boot_state, _millivolts)	\
	static struct regulator_init_data ri_data_##_var =		\
	{								\
		.supply_regulator = _in_supply,				\
		.num_consumer_supplies =				\
			ARRAY_SIZE(fixed_reg_##_name##_supply),		\
		.consumer_supplies = fixed_reg_##_name##_supply,	\
		.constraints = {					\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					REGULATOR_CHANGE_STATUS |	\
					REGULATOR_CHANGE_VOLTAGE),	\
			.always_on = _always_on,			\
			.boot_on = _boot_on,				\
		},							\
	};								\
	static struct fixed_voltage_config fixed_reg_##_var##_pdata =	\
	{								\
		.supply_name = FIXED_SUPPLY(_name),			\
		.microvolts = _millivolts * 1000,			\
		.gpio = _gpio_nr,					\
		.gpio_is_open_drain = _open_drain,			\
		.enable_high = _active_high,				\
		.enabled_at_boot = _boot_state,				\
		.init_data = &ri_data_##_var,				\
	};								\
	static struct platform_device fixed_reg_##_var##_dev = {	\
		.name = "reg-fixed-voltage",				\
		.id = _id,						\
		.dev = {						\
			.platform_data = &fixed_reg_##_var##_pdata,	\
		},							\
	}

/*
 * Creating the fixed regulator device table
 */

FIXED_REG(1,	dvdd_lcd_1v8,	dvdd_lcd_1v8,
	palmas_rails(smps8),	0,	1,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO4,	false,	true,	1,	1800);

FIXED_REG(2,	vdd_lcd_bl_en,	vdd_lcd_bl_en,
	NULL,	0,	1,
	TEGRA_GPIO_PH2,	false,	true,	1,	3700);

FIXED_REG(3,	dvdd_ts,	dvdd_ts,
	palmas_rails(smps8),	0,	0,
	TEGRA_GPIO_PH4,	false,	false,	1,	1800);

FIXED_REG(4,	vdd_hdmi_5v0,	vdd_hdmi_5v0,
	palmas_rails(smps10),	0,	0,
	TEGRA_GPIO_PK6,	true,	true,	0,	5000);

FIXED_REG(5,	vddio_sd_slot,	vddio_sd_slot,
	palmas_rails(smps9),	0,	0,
	TEGRA_GPIO_PK1,	false,	true,	0,	2900);

FIXED_REG(6,	vd_cam_1v8,	vd_cam_1v8,
	palmas_rails(smps8),	0,	0,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO6,	false,	true,	0,	1800);

FIXED_REG(7,	en_lcd_1v8,	en_lcd_1v8,
	palmas_rails(smps8),	0,	1,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO4,	false,	true,	1,	1800);

/*
 * Creating the fixed regulator device tables
 */
#define ADD_FIXED_REG(_name)	(&fixed_reg_##_name##_dev)

#define TEGRATAB_COMMON_FIXED_REG		\
	ADD_FIXED_REG(vdd_lcd_bl_en),		\
	ADD_FIXED_REG(vdd_hdmi_5v0),		\
	ADD_FIXED_REG(vddio_sd_slot),		\
	ADD_FIXED_REG(vd_cam_1v8),

#define E1569_FIXED_REG				\
	ADD_FIXED_REG(dvdd_lcd_1v8),		\
	ADD_FIXED_REG(dvdd_ts),

#define P1640_FIXED_REG				\
	ADD_FIXED_REG(en_lcd_1v8),

/* Gpio switch regulator platform data for Tegratab E1569 */
static struct platform_device *fixed_reg_devs_e1569[] = {
	TEGRATAB_COMMON_FIXED_REG
	E1569_FIXED_REG
};

/* Gpio switch regulator platform data for Tegratab P1640 */
static struct platform_device *fixed_reg_devs_p1640[] = {
	TEGRATAB_COMMON_FIXED_REG
	P1640_FIXED_REG
};

int __init tegratab_palmas_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	int i;
	struct board_info board_info;

	/* TPS65913: Normal state of INT request line is LOW.
	 * configure the power management controller to trigger PMU
	 * interrupts when HIGH.
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl | PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);
	for (i = 0; i < PALMAS_NUM_REGS ; i++) {
		pmic_platform.reg_data[i] = tegratab_reg_data[i];
		pmic_platform.reg_init[i] = tegratab_reg_init[i];
	}

	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_P1640 &&
				board_info.fab >= BOARD_FAB_A01) {
		palmas_pdata.clk32k_init_data = tegratab_palmas_clk32k_idata;
		palmas_pdata.clk32k_init_data_size =
				ARRAY_SIZE(tegratab_palmas_clk32k_idata);
	}
#ifdef CONFIG_ANDROID
	if (get_androidboot_mode() == BOOTMODE_CHARGER)
		palmas_pdata.long_press_delay =
				PALMAS_LONG_PRESS_KEY_TIME_12SECONDS;
#endif
	i2c_register_board_info(4, palma_device,
			ARRAY_SIZE(palma_device));
	return 0;
}

static int ac_online(void)
{
	return 1;
}

static struct resource tegratab_pda_resources[] = {
	[0] = {
		.name	= "ac",
	},
};

static struct pda_power_pdata tegratab_pda_data = {
	.is_ac_online	= ac_online,
};

static struct platform_device tegratab_pda_power_device = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= tegratab_pda_resources,
	.num_resources	= ARRAY_SIZE(tegratab_pda_resources),
	.dev	= {
		.platform_data	= &tegratab_pda_data,
	},
};

static struct tegra_suspend_platform_data tegratab_suspend_data = {
	.cpu_timer	= 300,
	.cpu_off_timer	= 300,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x157e,
	.core_off_timer = 2000,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.cpu_lp2_min_residency = 1000,
	.min_residency_crail = 20000,
#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
	.lp1_lowvolt_support = false,
	.i2c_base_addr = 0,
	.pmuslave_addr = 0,
	.core_reg_addr = 0,
	.lp1_core_volt_low = 0,
	.lp1_core_volt_high = 0,
#endif
};
#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param tegratab_cl_dvfs_param = {
	.sample_rate = 11500,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};
#endif

/* palmas: fixed 10mV steps from 600mV to 1400mV, with offset 0x10 */
#define PMU_CPU_VDD_MAP_SIZE ((1400000 - 600000) / 10000 + 1)
static struct voltage_reg_map pmu_cpu_vdd_map[PMU_CPU_VDD_MAP_SIZE];
static inline void fill_reg_map(void)
{
	int i;
	for (i = 0; i < PMU_CPU_VDD_MAP_SIZE; i++) {
		pmu_cpu_vdd_map[i].reg_value = i + 0x10;
		pmu_cpu_vdd_map[i].reg_uV = 600000 + 10000 * i;
	}
}

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct tegra_cl_dvfs_platform_data tegratab_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0xb0,
		.reg = 0x23,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,
	.pmu_undershoot_gb = 100,

	.cfg_param = &tegratab_cl_dvfs_param,
};

static int __init tegratab_cl_dvfs_init(void)
{
	fill_reg_map();
	if (tegra_revision < TEGRA_REVISION_A02)
		tegratab_cl_dvfs_data.flags =
			TEGRA_CL_DVFS_FLAGS_I2C_WAIT_QUIET;
	tegra_cl_dvfs_device.dev.platform_data = &tegratab_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

static int __init tegratab_fixed_regulator_init(void)
{
	struct board_info board_info;
	int ret;

	if (!machine_is_tegratab())
		return 0;

	tegra_get_board_info(&board_info);

	if (board_info.board_id == BOARD_P1640)
		ret = platform_add_devices(fixed_reg_devs_p1640,
					   ARRAY_SIZE(fixed_reg_devs_p1640));
	else
		ret = platform_add_devices(fixed_reg_devs_e1569,
					   ARRAY_SIZE(fixed_reg_devs_e1569));

	return ret;
}
subsys_initcall_sync(tegratab_fixed_regulator_init);


int __init tegratab_regulator_init(void)
{

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	tegratab_cl_dvfs_init();
#endif
	tegratab_palmas_regulator_init();
#ifndef CONFIG_OF
	tegratab_max17048_boardinfo[0].irq = gpio_to_irq(TEGRA_GPIO_PQ5);
	i2c_register_board_info(0, tegratab_max17048_boardinfo, 1);
#endif
	/* Disable charger when adapter is power source. */
	if (get_power_supply_type() != POWER_SUPPLY_TYPE_BATTERY)
		tegratab_bq2419x_pdata.bcharger_pdata = NULL;

	tegratab_bq2419x_boardinfo[0].irq = gpio_to_irq(TEGRA_GPIO_PJ0);
	i2c_register_board_info(0, tegratab_bq2419x_boardinfo, 1);

	regulator_has_full_constraints();
	platform_device_register(&psy_extcon_device);
	platform_device_register(&tegratab_pda_power_device);

	return 0;
}

int __init tegratab_power_off_init(void)
{
	/* Use PMU reset only when battery is exist. */
	if (get_power_supply_type() == POWER_SUPPLY_TYPE_BATTERY)
		pm_power_off = palmas_reset;

	return 0;
}

int __init tegratab_suspend_init(void)
{
	tegra_init_suspend(&tegratab_suspend_data);
	return 0;
}

int __init tegratab_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA)
		regulator_mA = 9000;

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_cpu_edp_limits(regulator_mA);

	regulator_mA = get_maximum_core_current_supported();
	if (!regulator_mA)
		regulator_mA = 4000;

	pr_info("%s: core regulator %d mA\n", __func__, regulator_mA);
	tegra_init_core_edp_limits(regulator_mA);

	return 0;
}

static struct thermal_zone_params tegratab_soctherm_therm_cpu_tzp = {
	.governor_name = "pid_thermal_gov",
};

static struct tegra_tsensor_pmu_data tpdata_palmas = {
	.reset_tegra = 1,
	.pmu_16bit_ops = 0,
	.controller_type = 0,
	.pmu_i2c_addr = 0x58,
	.i2c_controller_id = 4,
	.poweroff_reg_addr = 0xa0,
	.poweroff_reg_data = 0x0,
};

static struct soctherm_platform_data tegratab_soctherm_data = {
	.oc_irq_base = TEGRA_SOC_OC_IRQ_BASE,
	.num_oc_irqs = TEGRA_SOC_OC_NUM_IRQ,
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 90000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 100000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &tegratab_soctherm_therm_cpu_tzp,
		},
		[THERM_GPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 90000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 100000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &tegratab_soctherm_therm_cpu_tzp,
		},
		[THERM_PLL] = {
			.zone_enable = true,
		},
	},
	.throttle = {
		[THROTTLE_HEAVY] = {
			.priority = 100,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = true,
					.depth = 80,
				},
				[THROTTLE_DEV_GPU] = {
					.enable = true,
					.depth = 80,
				},
			},
		},
		[THROTTLE_OC4] = {
			.throt_mode = BRIEF,
			.polarity = 1,
			.intr = true,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = true,
					.depth = 50,
				},
				[THROTTLE_DEV_GPU] = {
					.enable = true,
					.depth = 50,
				},
			},
		},
	},
	.tshut_pmu_trip_data = &tpdata_palmas,
};

int __init tegratab_soctherm_init(void)
{
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	/*
	 * P1640 has oc4 from ina230. E1569 has oc4 from pmic powergood
	 * Disable oc4 throttle for E1569
	 */
	if (board_info.board_id == BOARD_E1569) {
		tegratab_soctherm_data.throttle[THROTTLE_OC4]
			.devs[THROTTLE_DEV_CPU].enable = false;
		tegratab_soctherm_data.throttle[THROTTLE_OC4]
			.devs[THROTTLE_DEV_GPU].enable = false;
	}

	tegra_platform_edp_init(tegratab_soctherm_data.therm[THERM_CPU].trips,
			&tegratab_soctherm_data.therm[THERM_CPU].num_trips,
			6000); /* edp temperature margin */
	tegra_add_tj_trips(tegratab_soctherm_data.therm[THERM_CPU].trips,
			&tegratab_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_add_vc_trips(tegratab_soctherm_data.therm[THERM_CPU].trips,
			&tegratab_soctherm_data.therm[THERM_CPU].num_trips);

	if (board_info.board_id != BOARD_E1569 &&
			(board_info.board_id == BOARD_P1640 &&
			(board_info.fab != BOARD_FAB_A00 &&
			board_info.fab != BOARD_FAB_A01))) {
		tegra_add_cdev_trips(
			tegratab_soctherm_data.therm[THERM_CPU].trips,
			&tegratab_soctherm_data.therm[THERM_CPU].num_trips);
	}

	return tegra11_soctherm_init(&tegratab_soctherm_data);
}

static struct edp_manager tegratab_sysedp_manager = {
	.name = "battery",
	.max = 16800
};

void __init tegratab_sysedp_init(void)
{
	struct edp_governor *g;
	int r;

	if (!IS_ENABLED(CONFIG_EDP_FRAMEWORK))
		return;

	r = edp_register_manager(&tegratab_sysedp_manager);
	WARN_ON(r);
	if (r)
		return;

	/* start with priority governor */
	g = edp_get_governor("priority");
	WARN_ON(!g);
	if (!g)
		return;

	r = edp_set_governor(&tegratab_sysedp_manager, g);
	WARN_ON(r);
}

static unsigned int tegratab_psydepl_states[] = {
	16800, 16200, 15900, 15600, 15300, 15000, 14700, 14400, 14100, 13800,
	13500, 13200, 12900, 12600, 12300, 12000, 11700, 11400, 11100, 10800,
	10500, 10200,  9900,  9600,  9300,  9000,  8700,  8300,  7900,  7500,
	 7100,  6700,  6300,  5900,  5500,  5100,  4700,  4300,  3900,  3500,
	 3000,  2500,  2000,  1500,  1000,   500,     0
};

static struct psy_depletion_ibat_lut tegratab_ibat_lut[] = {
	{  60,  500 }, /* Temporary, due to psy_depletion client */
	{  50, 4500 },
	{   0, 4500 },
	{ -10,    0 }
};

static struct psy_depletion_rbat_lut tegratab_rbat_lut[] = {
	{ 100,   60000 },
	{  30,   60000 },
	{  20,   80000 },
	{  15,   90000 },
	{  10,  120000 },
	{   0,  120000 }
};

static struct psy_depletion_platform_data tegratab_psydepl_pdata = {
	.power_supply = "battery",
	.states = tegratab_psydepl_states,
	.num_states = ARRAY_SIZE(tegratab_psydepl_states),
	.e0_index = ARRAY_SIZE(tegratab_psydepl_states) - 1,
	.r_const = 40100,
	.vsys_min = 3200000,
	.vcharge = 4200000,
	.ibat_nom = 4500,
	.ibat_lut = tegratab_ibat_lut,
	.rbat_lut = tegratab_rbat_lut
};

static struct platform_device tegratab_psydepl_device = {
	.name = "psy_depletion",
	.id = -1,
	.dev = { .platform_data = &tegratab_psydepl_pdata }
};

void __init tegratab_sysedp_psydepl_init(void)
{
	int r;

	r = platform_device_register(&tegratab_psydepl_device);
	WARN_ON(r);
}

#define NO_CAP	(ULONG_MAX)
static struct tegra_sysedp_corecap tegratab_sysedp_corecap[] = {
	{   9100, {   3943,    240,    624 }, {   2706,    420,    792 } },
	{  10000, {   4565,    240,    792 }, {   3398,    528,    792 } },
	{  10500, {   5065,    240,    792 }, {   3898,    528,    792 } },
	{  11000, {   5565,    240,    792 }, {   4398,    528,    792 } },
	{  12000, {   6565,    240,    792 }, {   4277,    600,    792 } },
	{  13000, {   7565,    240,    792 }, {   5277,    600,    792 } },
	{  14000, {   8565,    240,    792 }, {   6277,    600,    792 } },
	{  15000, {   9565,    384,    792 }, {   7277,    600,    792 } },
	{  16000, {  10565,    468,    792 }, {   8277,    600,    792 } },
	{ NO_CAP, { NO_CAP, NO_CAP, NO_CAP }, { NO_CAP, NO_CAP, NO_CAP } },
};

static struct tegra_sysedp_platform_data tegratab_sysedp_platdata = {
	.corecap = tegratab_sysedp_corecap,
	.corecap_size = ARRAY_SIZE(tegratab_sysedp_corecap),
	.init_req_watts = 15000
};

static struct platform_device tegratab_sysedp_device = {
	.name = "tegra_sysedp_lite",
	.id = -1,
	.dev = { .platform_data = &tegratab_sysedp_platdata }
};

void __init tegratab_sysedp_core_init(void)
{
	int r;

	tegratab_sysedp_platdata.cpufreq_lim = tegra_get_system_edp_entries(
			&tegratab_sysedp_platdata.cpufreq_lim_size);
	if (!tegratab_sysedp_platdata.cpufreq_lim) {
		WARN_ON(1);
		return;
	}

	r = platform_device_register(&tegratab_sysedp_device);
	WARN_ON(r);
}
