/*
 * Copyright 2004 ATI Technologies Inc., Markham, Ontario
 * Copyright 2007-8 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 */
#include "drmP.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

/* old legacy ATI BIOS routines */

/* COMBIOS table offsets */
enum radeon_combios_table_offset
{
	/* absolute offset tables */
	COMBIOS_ASIC_INIT_1_TABLE,
	COMBIOS_BIOS_SUPPORT_TABLE,
	COMBIOS_DAC_PROGRAMMING_TABLE,
	COMBIOS_MAX_COLOR_DEPTH_TABLE,
	COMBIOS_CRTC_INFO_TABLE,
	COMBIOS_PLL_INFO_TABLE,
	COMBIOS_TV_INFO_TABLE,
	COMBIOS_DFP_INFO_TABLE,
	COMBIOS_HW_CONFIG_INFO_TABLE,
	COMBIOS_MULTIMEDIA_INFO_TABLE,
	COMBIOS_TV_STD_PATCH_TABLE,
	COMBIOS_LCD_INFO_TABLE,
	COMBIOS_MOBILE_INFO_TABLE,
	COMBIOS_PLL_INIT_TABLE,
	COMBIOS_MEM_CONFIG_TABLE,
	COMBIOS_SAVE_MASK_TABLE,
	COMBIOS_HARDCODED_EDID_TABLE,
	COMBIOS_ASIC_INIT_2_TABLE,
	COMBIOS_CONNECTOR_INFO_TABLE,
	COMBIOS_DYN_CLK_1_TABLE,
	COMBIOS_RESERVED_MEM_TABLE,
	COMBIOS_EXT_TMDS_INFO_TABLE,
	COMBIOS_MEM_CLK_INFO_TABLE,
	COMBIOS_EXT_DAC_INFO_TABLE,
	COMBIOS_MISC_INFO_TABLE,
	COMBIOS_CRT_INFO_TABLE,
	COMBIOS_INTEGRATED_SYSTEM_INFO_TABLE,
	COMBIOS_COMPONENT_VIDEO_INFO_TABLE,
	COMBIOS_FAN_SPEED_INFO_TABLE,
	COMBIOS_OVERDRIVE_INFO_TABLE,
	COMBIOS_OEM_INFO_TABLE,
	COMBIOS_DYN_CLK_2_TABLE,
	COMBIOS_POWER_CONNECTOR_INFO_TABLE,
	COMBIOS_I2C_INFO_TABLE,
	/* relative offset tables */
	COMBIOS_ASIC_INIT_3_TABLE,	/* offset from misc info */
	COMBIOS_ASIC_INIT_4_TABLE,	/* offset from misc info */
	COMBIOS_ASIC_INIT_5_TABLE,	/* offset from misc info */
	COMBIOS_RAM_RESET_TABLE,	/* offset from mem config */
	COMBIOS_POWERPLAY_TABLE,	/* offset from mobile info */
	COMBIOS_GPIO_INFO_TABLE,	/* offset from mobile info */
	COMBIOS_LCD_DDC_INFO_TABLE,	/* offset from mobile info */
	COMBIOS_TMDS_POWER_TABLE,	/* offset from mobile info */
	COMBIOS_TMDS_POWER_ON_TABLE,	/* offset from tmds power */
	COMBIOS_TMDS_POWER_OFF_TABLE,	/* offset from tmds power */
};

enum radeon_combios_ddc
{
    DDC_NONE_DETECTED,
    DDC_MONID,
    DDC_DVI,
    DDC_VGA,
    DDC_CRT2,
    DDC_LCD,
    DDC_GPIO,
};

enum radeon_combios_connector
{
    CONNECTOR_NONE_LEGACY,
    CONNECTOR_PROPRIETARY_LEGACY,
    CONNECTOR_CRT_LEGACY,
    CONNECTOR_DVI_I_LEGACY,
    CONNECTOR_DVI_D_LEGACY,
    CONNECTOR_CTV_LEGACY,
    CONNECTOR_STV_LEGACY,
    CONNECTOR_UNSUPPORTED_LEGACY
};

static uint16_t combios_get_table_offset(struct drm_device *dev, enum radeon_combios_table_offset table)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int rev;
	uint16_t offset = 0, check_offset;

	switch (table) {
	/* absolute offset tables */
	case COMBIOS_ASIC_INIT_1_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0xc);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_BIOS_SUPPORT_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x14);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_DAC_PROGRAMMING_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x2a);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_MAX_COLOR_DEPTH_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x2c);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_CRTC_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x2e);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_PLL_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x30);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_TV_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x32);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_DFP_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x34);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_HW_CONFIG_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x36);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_MULTIMEDIA_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x38);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_TV_STD_PATCH_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x3e);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_LCD_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x40);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_MOBILE_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x42);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_PLL_INIT_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x46);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_MEM_CONFIG_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x48);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_SAVE_MASK_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x4a);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_HARDCODED_EDID_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x4c);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_ASIC_INIT_2_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x4e);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_CONNECTOR_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x50);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_DYN_CLK_1_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x52);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_RESERVED_MEM_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x54);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_EXT_TMDS_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x58);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_MEM_CLK_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x5a);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_EXT_DAC_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x5c);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_MISC_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x5e);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_CRT_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x60);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_INTEGRATED_SYSTEM_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x62);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_COMPONENT_VIDEO_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x64);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_FAN_SPEED_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x66);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_OVERDRIVE_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x68);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_OEM_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x6a);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_DYN_CLK_2_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x6c);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_POWER_CONNECTOR_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x6e);
		if (check_offset)
			offset = check_offset;
		break;
	case COMBIOS_I2C_INFO_TABLE:
		check_offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x70);
		if (check_offset)
			offset = check_offset;
		break;
	/* relative offset tables */
	case COMBIOS_ASIC_INIT_3_TABLE:	/* offset from misc info */
		check_offset = combios_get_table_offset(dev, COMBIOS_MISC_INFO_TABLE);
		if (check_offset) {
			rev = radeon_bios8(dev_priv, check_offset);
			if (rev > 0) {
				check_offset = radeon_bios16(dev_priv, check_offset + 0x3);
				if (check_offset)
					offset = check_offset;
			}
		}
		break;
	case COMBIOS_ASIC_INIT_4_TABLE:	/* offset from misc info */
		check_offset = combios_get_table_offset(dev, COMBIOS_MISC_INFO_TABLE);
		if (check_offset) {
			rev = radeon_bios8(dev_priv, check_offset);
			if (rev > 0) {
				check_offset = radeon_bios16(dev_priv, check_offset + 0x5);
				if (check_offset)
					offset = check_offset;
			}
		}
		break;
	case COMBIOS_ASIC_INIT_5_TABLE:	/* offset from misc info */
		check_offset = combios_get_table_offset(dev, COMBIOS_MISC_INFO_TABLE);
		if (check_offset) {
			rev = radeon_bios8(dev_priv, check_offset);
			if (rev == 2) {
				check_offset = radeon_bios16(dev_priv, check_offset + 0x9);
				if (check_offset)
					offset = check_offset;
			}
		}
		break;
	case COMBIOS_RAM_RESET_TABLE:	/* offset from mem config */
		check_offset = combios_get_table_offset(dev, COMBIOS_MEM_CONFIG_TABLE);
		if (check_offset) {
			while (radeon_bios8(dev_priv, check_offset++));
			check_offset += 2;
			if (check_offset)
				offset = check_offset;
		}
		break;
	case COMBIOS_POWERPLAY_TABLE:	/* offset from mobile info */
		check_offset = combios_get_table_offset(dev, COMBIOS_MOBILE_INFO_TABLE);
		if (check_offset) {
			check_offset = radeon_bios16(dev_priv, check_offset + 0x11);
			if (check_offset)
				offset = check_offset;
		}
		break;
	case COMBIOS_GPIO_INFO_TABLE:	/* offset from mobile info */
		check_offset = combios_get_table_offset(dev, COMBIOS_MOBILE_INFO_TABLE);
		if (check_offset) {
			check_offset = radeon_bios16(dev_priv, check_offset + 0x13);
			if (check_offset)
				offset = check_offset;
		}
		break;
	case COMBIOS_LCD_DDC_INFO_TABLE:	/* offset from mobile info */
		check_offset = combios_get_table_offset(dev, COMBIOS_MOBILE_INFO_TABLE);
		if (check_offset) {
			check_offset = radeon_bios16(dev_priv, check_offset + 0x15);
			if (check_offset)
				offset = check_offset;
		}
		break;
	case COMBIOS_TMDS_POWER_TABLE:	        /* offset from mobile info */
		check_offset = combios_get_table_offset(dev, COMBIOS_MOBILE_INFO_TABLE);
		if (check_offset) {
			check_offset = radeon_bios16(dev_priv, check_offset + 0x17);
			if (check_offset)
				offset = check_offset;
		}
		break;
	case COMBIOS_TMDS_POWER_ON_TABLE:	/* offset from tmds power */
		check_offset = combios_get_table_offset(dev, COMBIOS_TMDS_POWER_TABLE);
		if (check_offset) {
			check_offset = radeon_bios16(dev_priv, check_offset + 0x2);
			if (check_offset)
				offset = check_offset;
		}
		break;
	case COMBIOS_TMDS_POWER_OFF_TABLE:	/* offset from tmds power */
		check_offset = combios_get_table_offset(dev, COMBIOS_TMDS_POWER_TABLE);
		if (check_offset) {
			check_offset = radeon_bios16(dev_priv, check_offset + 0x4);
			if (check_offset)
				offset = check_offset;
		}
		break;
	default:
		break;
	}

	return offset;

}

struct radeon_i2c_bus_rec combios_setup_i2c_bus(int ddc_line)
{
	struct radeon_i2c_bus_rec i2c;

	i2c.mask_clk_mask = RADEON_GPIO_EN_1;
	i2c.mask_data_mask = RADEON_GPIO_EN_0;
	i2c.a_clk_mask = RADEON_GPIO_A_1;
	i2c.a_data_mask = RADEON_GPIO_A_0;
	i2c.put_clk_mask = RADEON_GPIO_EN_1;
	i2c.put_data_mask = RADEON_GPIO_EN_0;
	i2c.get_clk_mask = RADEON_GPIO_Y_1;
	i2c.get_data_mask = RADEON_GPIO_Y_0;
	if ((ddc_line == RADEON_LCD_GPIO_MASK) ||
	    (ddc_line == RADEON_MDGPIO_EN_REG)) {
		i2c.mask_clk_reg = ddc_line;
		i2c.mask_data_reg = ddc_line;
		i2c.a_clk_reg = ddc_line;
		i2c.a_data_reg = ddc_line;
		i2c.put_clk_reg = ddc_line;
		i2c.put_data_reg = ddc_line;
		i2c.get_clk_reg = ddc_line + 4;
		i2c.get_data_reg = ddc_line + 4;
	} else {
		i2c.mask_clk_reg = ddc_line;
		i2c.mask_data_reg = ddc_line;
		i2c.a_clk_reg = ddc_line;
		i2c.a_data_reg = ddc_line;
		i2c.put_clk_reg = ddc_line;
		i2c.put_data_reg = ddc_line;
		i2c.get_clk_reg = ddc_line;
		i2c.get_data_reg = ddc_line;
	}

	if (ddc_line)
		i2c.valid = true;
	else
		i2c.valid = false;

	return i2c;
}

bool radeon_combios_get_clock_info(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	uint16_t pll_info;
	struct radeon_pll *pll = &mode_info->pll;
	int8_t rev;

	pll_info = combios_get_table_offset(dev, COMBIOS_PLL_INFO_TABLE);
	if (pll_info) {
		rev = radeon_bios8(dev_priv, pll_info);

		pll->reference_freq = radeon_bios16(dev_priv, pll_info + 0xe);
		pll->reference_div = radeon_bios16(dev_priv, pll_info + 0x10);
		pll->pll_out_min = radeon_bios32(dev_priv, pll_info + 0x12);
		pll->pll_out_max = radeon_bios32(dev_priv, pll_info + 0x16);

		if (rev > 9) {
			pll->pll_in_min = radeon_bios32(dev_priv, pll_info + 0x36);
			pll->pll_in_max = radeon_bios32(dev_priv, pll_info + 0x3a);
		} else {
			pll->pll_in_min = 40;
			pll->pll_in_max = 500;
		}

		pll->xclk = radeon_bios16(dev_priv, pll_info + 0x08);

		// sclk/mclk use fixed point
		//sclk = radeon_bios16(pll_info + 8) / 100.0;
		//mclk = radeon_bios16(pll_info + 10) / 100.0;
		//if (sclk == 0) sclk = 200;
		//if (mclk == 0) mclk = 200;

		return true;
	}
	return false;
}

bool radeon_combios_get_primary_dac_info(struct radeon_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint16_t dac_info;
	uint8_t rev, bg, dac;

	/* check CRT table */
	dac_info = combios_get_table_offset(dev, COMBIOS_CRT_INFO_TABLE);
	if (dac_info) {
		rev = radeon_bios8(dev_priv, dac_info) & 0x3;
		if (rev < 2) {
			bg = radeon_bios8(dev_priv, dac_info + 0x2) & 0xf;
			dac = (radeon_bios8(dev_priv, dac_info + 0x2) >> 4) & 0xf;
			encoder->ps2_pdac_adj = (bg << 8) | (dac);

			return true;
		} else {
			bg = radeon_bios8(dev_priv, dac_info + 0x2) & 0xf;
			dac = radeon_bios8(dev_priv, dac_info + 0x3) & 0xf;
			encoder->ps2_pdac_adj = (bg << 8) | (dac);

			return true;
		}

	}

	return false;
}

bool radeon_combios_get_tv_dac_info(struct radeon_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint16_t dac_info;
	uint8_t rev, bg, dac;

	/* first check TV table */
	dac_info = combios_get_table_offset(dev, COMBIOS_TV_INFO_TABLE);
	if (dac_info) {
		rev = radeon_bios8(dev_priv, dac_info + 0x3);
		if (rev > 4) {
			bg = radeon_bios8(dev_priv, dac_info + 0xc) & 0xf;
			dac = radeon_bios8(dev_priv, dac_info + 0xd) & 0xf;
			encoder->ps2_tvdac_adj = (bg << 16) | (dac << 20);

			bg = radeon_bios8(dev_priv, dac_info + 0xe) & 0xf;
			dac = radeon_bios8(dev_priv, dac_info + 0xf) & 0xf;
			encoder->pal_tvdac_adj = (bg << 16) | (dac << 20);

			bg = radeon_bios8(dev_priv, dac_info + 0x10) & 0xf;
			dac = radeon_bios8(dev_priv, dac_info + 0x11) & 0xf;
			encoder->ntsc_tvdac_adj = (bg << 16) | (dac << 20);

			return true;
		} else if (rev > 1) {
			bg = radeon_bios8(dev_priv, dac_info + 0xc) & 0xf;
			dac = (radeon_bios8(dev_priv, dac_info + 0xc) >> 4) & 0xf;
			encoder->ps2_tvdac_adj = (bg << 16) | (dac << 20);

			bg = radeon_bios8(dev_priv, dac_info + 0xd) & 0xf;
			dac = (radeon_bios8(dev_priv, dac_info + 0xd) >> 4) & 0xf;
			encoder->pal_tvdac_adj = (bg << 16) | (dac << 20);

			bg = radeon_bios8(dev_priv, dac_info + 0xe) & 0xf;
			dac = (radeon_bios8(dev_priv, dac_info + 0xe) >> 4) & 0xf;
			encoder->ntsc_tvdac_adj = (bg << 16) | (dac << 20);

			return true;
		}
	}

	/* then check CRT table */
	dac_info = combios_get_table_offset(dev, COMBIOS_CRT_INFO_TABLE);
	if (dac_info) {
		rev = radeon_bios8(dev_priv, dac_info) & 0x3;
		if (rev < 2) {
			bg = radeon_bios8(dev_priv, dac_info + 0x3) & 0xf;
			dac = (radeon_bios8(dev_priv, dac_info + 0x3) >> 4) & 0xf;
			encoder->ps2_tvdac_adj = (bg << 16) | (dac << 20);
			encoder->pal_tvdac_adj = encoder->ps2_tvdac_adj;
			encoder->ntsc_tvdac_adj = encoder->ps2_tvdac_adj;

			return true;
		} else {
			bg = radeon_bios8(dev_priv, dac_info + 0x4) & 0xf;
			dac = radeon_bios8(dev_priv, dac_info + 0x5) & 0xf;
			encoder->ps2_tvdac_adj = (bg << 16) | (dac << 20);
			encoder->pal_tvdac_adj = encoder->ps2_tvdac_adj;
			encoder->ntsc_tvdac_adj = encoder->ps2_tvdac_adj;

			return true;
		}

	}

	return false;
}

bool radeon_combios_get_tv_info(struct radeon_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint16_t tv_info;

	tv_info = combios_get_table_offset(dev, COMBIOS_TV_INFO_TABLE);
	if (tv_info) {
		if (radeon_bios8(dev_priv, tv_info + 6) == 'T') {
			switch (radeon_bios8(dev_priv, tv_info + 7) & 0xf) {
			case 1:
				encoder->tv_std = TV_STD_NTSC;
				DRM_INFO("Default TV standard: NTSC\n");
				break;
			case 2:
				encoder->tv_std = TV_STD_PAL;
				DRM_INFO("Default TV standard: PAL\n");
				break;
			case 3:
				encoder->tv_std = TV_STD_PAL_M;
				DRM_INFO("Default TV standard: PAL-M\n");
				break;
			case 4:
				encoder->tv_std = TV_STD_PAL_60;
				DRM_INFO("Default TV standard: PAL-60\n");
				break;
			case 5:
				encoder->tv_std = TV_STD_NTSC_J;
				DRM_INFO("Default TV standard: NTSC-J\n");
				break;
			case 6:
				encoder->tv_std = TV_STD_SCART_PAL;
				DRM_INFO("Default TV standard: SCART-PAL\n");
				break;
			default:
				encoder->tv_std = TV_STD_NTSC;
				DRM_INFO("Unknown TV standard; defaulting to NTSC\n");
				break;
			}

			switch ((radeon_bios8(dev_priv, tv_info + 9) >> 2) & 0x3) {
			case 0:
				DRM_INFO("29.498928713 MHz TV ref clk\n");
				break;
			case 1:
				DRM_INFO("28.636360000 MHz TV ref clk\n");
				break;
			case 2:
				DRM_INFO("14.318180000 MHz TV ref clk\n");
				break;
			case 3:
				DRM_INFO("27.000000000 MHz TV ref clk\n");
				break;
			default:
				break;
			}
			return true;
		}
	}
	return false;
}

bool radeon_combios_get_lvds_info(struct radeon_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint16_t lcd_info;
	uint32_t panel_setup;
	char stmp[30];
	int tmp, i;

	lcd_info = combios_get_table_offset(dev, COMBIOS_LCD_INFO_TABLE);

	if (lcd_info) {
		for (i = 0; i < 24; i++)
			stmp[i] = radeon_bios8(dev_priv, lcd_info + i + 1);
		stmp[24] = 0;

		DRM_INFO("Panel ID String: %s\n", stmp);

		encoder->panel_xres = radeon_bios16(dev_priv, lcd_info + 25);
		encoder->panel_yres = radeon_bios16(dev_priv, lcd_info + 27);

		DRM_INFO("Panel Size %dx%d\n", encoder->panel_xres, encoder->panel_yres);

		encoder->panel_vcc_delay = radeon_bios16(dev_priv, lcd_info + 44);
		if (encoder->panel_vcc_delay > 2000 || encoder->panel_vcc_delay < 0)
			encoder->panel_vcc_delay = 2000;

		encoder->panel_pwr_delay = radeon_bios16(dev_priv, lcd_info + 0x24);
		encoder->panel_digon_delay = radeon_bios16(dev_priv, lcd_info + 0x38) & 0xf;
		encoder->panel_blon_delay = (radeon_bios16(dev_priv, lcd_info + 0x38) >> 4) & 0xf;

		encoder->panel_ref_divider = radeon_bios16(dev_priv, lcd_info + 46);
		encoder->panel_post_divider = radeon_bios8(dev_priv, lcd_info + 48);
		encoder->panel_fb_divider = radeon_bios16(dev_priv, lcd_info + 49);
		if ((encoder->panel_ref_divider != 0) &&
		    (encoder->panel_fb_divider > 3))
			encoder->use_bios_dividers = true;

		panel_setup = radeon_bios32(dev_priv, lcd_info + 0x39);
		encoder->lvds_gen_cntl = 0xff00;
		if (panel_setup & 0x1)
			encoder->lvds_gen_cntl |= RADEON_LVDS_PANEL_FORMAT;

		if ((panel_setup >> 4) & 0x1)
			encoder->lvds_gen_cntl |= RADEON_LVDS_PANEL_TYPE;

		switch ((panel_setup >> 8) & 0x7) {
		case 0:
			encoder->lvds_gen_cntl |= RADEON_LVDS_NO_FM;
			break;
		case 1:
			encoder->lvds_gen_cntl |= RADEON_LVDS_2_GREY;
			break;
		case 2:
			encoder->lvds_gen_cntl |= RADEON_LVDS_4_GREY;
			break;
		default:
			break;
		}

		if ((panel_setup >> 16) & 0x1)
			encoder->lvds_gen_cntl |= RADEON_LVDS_FP_POL_LOW;

		if ((panel_setup >> 17) & 0x1)
			encoder->lvds_gen_cntl |= RADEON_LVDS_LP_POL_LOW;

		if ((panel_setup >> 18) & 0x1)
			encoder->lvds_gen_cntl |= RADEON_LVDS_DTM_POL_LOW;

		if ((panel_setup >> 23) & 0x1)
			encoder->lvds_gen_cntl |= RADEON_LVDS_BL_CLK_SEL;

		encoder->lvds_gen_cntl |= (panel_setup & 0xf0000000);


		for (i = 0; i < 32; i++) {
			tmp = radeon_bios16(dev_priv, lcd_info + 64 + i * 2);
			if (tmp == 0) break;

			if ((radeon_bios16(dev_priv, tmp) == encoder->panel_xres) &&
			    (radeon_bios16(dev_priv, tmp + 2) == encoder->panel_yres)) {
				encoder->hblank = (radeon_bios16(dev_priv, tmp + 17) -
						   radeon_bios16(dev_priv, tmp + 19)) * 8;
				encoder->hoverplus = (radeon_bios16(dev_priv, tmp + 21) -
						      radeon_bios16(dev_priv, tmp + 19) - 1) * 8;
				encoder->hsync_width = radeon_bios8(dev_priv, tmp + 23) * 8;

				encoder->vblank = (radeon_bios16(dev_priv, tmp + 24) -
						   radeon_bios16(dev_priv, tmp + 26));
				encoder->voverplus = ((radeon_bios16(dev_priv, tmp + 28) & 0x7fff) -
						      radeon_bios16(dev_priv, tmp + 26));
				encoder->vsync_width = ((radeon_bios16(dev_priv, tmp + 28) & 0xf800) >> 11);
				encoder->dotclock = radeon_bios16(dev_priv, tmp + 9) * 10;
				encoder->flags = 0;
			}
		}
		return true;
	}
	DRM_INFO("No panel info found in BIOS\n");
	return false;

}

bool radeon_combios_get_tmds_info(struct radeon_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint16_t tmds_info;
	int i, n;
	uint8_t ver;

	tmds_info = combios_get_table_offset(dev, COMBIOS_DFP_INFO_TABLE);

	if (tmds_info) {
		ver = radeon_bios8(dev_priv, tmds_info);
		DRM_INFO("DFP table revision: %d\n", ver);
		if (ver == 3) {
			n = radeon_bios8(dev_priv, tmds_info + 5) + 1;
			if (n > 4)
				n = 4;
			for (i = 0; i < n; i++) {
				encoder->tmds_pll[i].value = radeon_bios32(dev_priv, tmds_info + i * 10 + 0x08);
				encoder->tmds_pll[i].freq = radeon_bios16(dev_priv, tmds_info + i * 10 + 0x10);
			}
			return true;
		} else if (ver == 4) {
			int stride = 0;
			n = radeon_bios8(dev_priv, tmds_info + 5) + 1;
			if (n > 4)
				n = 4;
			for (i = 0; i < n; i++) {
				encoder->tmds_pll[i].value = radeon_bios32(dev_priv, tmds_info + stride + 0x08);
				encoder->tmds_pll[i].freq = radeon_bios16(dev_priv, tmds_info + stride + 0x10);
				if (i == 0)
					stride += 10;
				else
					stride += 6;
			}
			return true;
		}
	}

	DRM_INFO("No TMDS info found in BIOS\n");
	return false;
}

void radeon_combios_get_ext_tmds_info(struct radeon_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint16_t ext_tmds_info;
	uint8_t ver;

	ext_tmds_info = combios_get_table_offset(dev, COMBIOS_EXT_TMDS_INFO_TABLE);
	if (ext_tmds_info) {
		ver = radeon_bios8(dev_priv, ext_tmds_info);
		DRM_INFO("External TMDS Table revision: %d\n", ver);
		// TODO
	}
}

static void radeon_apply_legacy_quirks(struct drm_device *dev, int bios_index)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;

	/* XPRESS DDC quirks */
	if ((dev_priv->chip_family == CHIP_RS400 ||
	     dev_priv->chip_family == CHIP_RS480) &&
	    mode_info->bios_connector[bios_index].ddc_i2c.mask_clk_reg == RADEON_GPIO_CRT2_DDC) {
		mode_info->bios_connector[bios_index].ddc_i2c = combios_setup_i2c_bus(RADEON_GPIO_MONID);
	} else if ((dev_priv->chip_family == CHIP_RS400 ||
		    dev_priv->chip_family == CHIP_RS480) &&
		   mode_info->bios_connector[bios_index].ddc_i2c.mask_clk_reg == RADEON_GPIO_MONID) {
		mode_info->bios_connector[bios_index].ddc_i2c.valid = true;
		mode_info->bios_connector[bios_index].ddc_i2c.mask_clk_mask = (0x20 << 8);
		mode_info->bios_connector[bios_index].ddc_i2c.mask_data_mask = 0x80;
		mode_info->bios_connector[bios_index].ddc_i2c.a_clk_mask = (0x20 << 8);
		mode_info->bios_connector[bios_index].ddc_i2c.a_data_mask = 0x80;
		mode_info->bios_connector[bios_index].ddc_i2c.put_clk_mask = (0x20 << 8);
		mode_info->bios_connector[bios_index].ddc_i2c.put_data_mask = 0x80;
		mode_info->bios_connector[bios_index].ddc_i2c.get_clk_mask = (0x20 << 8);
		mode_info->bios_connector[bios_index].ddc_i2c.get_data_mask = 0x80;
		mode_info->bios_connector[bios_index].ddc_i2c.mask_clk_reg = RADEON_GPIOPAD_MASK;
		mode_info->bios_connector[bios_index].ddc_i2c.mask_data_reg = RADEON_GPIOPAD_MASK;
		mode_info->bios_connector[bios_index].ddc_i2c.a_clk_reg = RADEON_GPIOPAD_A;
		mode_info->bios_connector[bios_index].ddc_i2c.a_data_reg = RADEON_GPIOPAD_A;
		mode_info->bios_connector[bios_index].ddc_i2c.put_clk_reg = RADEON_GPIOPAD_EN;
		mode_info->bios_connector[bios_index].ddc_i2c.put_data_reg = RADEON_GPIOPAD_EN;
		mode_info->bios_connector[bios_index].ddc_i2c.get_clk_reg = RADEON_LCD_GPIO_Y_REG;
		mode_info->bios_connector[bios_index].ddc_i2c.get_data_reg = RADEON_LCD_GPIO_Y_REG;
	}

	/* Certain IBM chipset RN50s have a BIOS reporting two VGAs,
	   one with VGA DDC and one with CRT2 DDC. - kill the CRT2 DDC one */
	if (dev->pdev->device == 0x515e &&
	    dev->pdev->subsystem_vendor == 0x1014) {
		if (mode_info->bios_connector[bios_index].connector_type == CONNECTOR_VGA &&
		    mode_info->bios_connector[bios_index].ddc_i2c.mask_clk_reg == RADEON_GPIO_CRT2_DDC) {
			mode_info->bios_connector[bios_index].valid = false;
		}
	}

	/* Some RV100 cards with 2 VGA ports show up with DVI+VGA */
	if (dev->pdev->device == 0x5159 &&
	    dev->pdev->subsystem_vendor == 0x1002 &&
	    dev->pdev->subsystem_device == 0x013a) {
		if (mode_info->bios_connector[bios_index].connector_type == CONNECTOR_DVI_I)
			mode_info->bios_connector[bios_index].connector_type = CONNECTOR_VGA;

	}

	/* X300 card with extra non-existent DVI port */
	if (dev->pdev->device == 0x5B60 &&
	    dev->pdev->subsystem_vendor == 0x17af &&
	    dev->pdev->subsystem_device == 0x201e &&
	    bios_index == 2) {
		if (mode_info->bios_connector[bios_index].connector_type == CONNECTOR_DVI_I)
			mode_info->bios_connector[bios_index].valid = false;
	}

}

bool radeon_get_legacy_connector_info_from_bios(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	uint32_t conn_info, entry;
	uint16_t tmp;
	enum radeon_combios_ddc ddc_type;
	enum radeon_combios_connector connector_type;
	int i;

	DRM_DEBUG("\n");
	conn_info = combios_get_table_offset(dev, COMBIOS_CONNECTOR_INFO_TABLE);
	if (conn_info) {
		for (i = 0; i < 4; i++) {
			entry = conn_info + 2 + i * 2;

			if (!radeon_bios16(dev_priv, entry))
				    break;

			mode_info->bios_connector[i].valid = true;

			tmp = radeon_bios16(dev_priv, entry);

			connector_type = (tmp >> 12) & 0xf;
			mode_info->bios_connector[i].connector_type = connector_type;

			switch(connector_type) {
			case CONNECTOR_PROPRIETARY_LEGACY:
				mode_info->bios_connector[i].connector_type = CONNECTOR_DVI_D;
				break;
			case CONNECTOR_CRT_LEGACY:
				mode_info->bios_connector[i].connector_type = CONNECTOR_VGA;
				break;
			case CONNECTOR_DVI_I_LEGACY:
				mode_info->bios_connector[i].connector_type = CONNECTOR_DVI_I;
				break;
			case CONNECTOR_DVI_D_LEGACY:
				mode_info->bios_connector[i].connector_type = CONNECTOR_DVI_D;
				break;
			case CONNECTOR_CTV_LEGACY:
				mode_info->bios_connector[i].connector_type = CONNECTOR_CTV;
				break;
			case CONNECTOR_STV_LEGACY:
				mode_info->bios_connector[i].connector_type = CONNECTOR_STV;
				break;
			default:
				DRM_ERROR("Unknown connector type: %d\n", connector_type);
				mode_info->bios_connector[i].valid = false;
				break;
			}

			mode_info->bios_connector[i].ddc_i2c.valid = false;

			ddc_type = (tmp >> 8) & 0xf;
			switch (ddc_type) {
			case DDC_MONID:
				mode_info->bios_connector[i].ddc_i2c = combios_setup_i2c_bus(RADEON_GPIO_MONID);
				break;
			case DDC_DVI:
				mode_info->bios_connector[i].ddc_i2c = combios_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
				break;
			case DDC_VGA:
				mode_info->bios_connector[i].ddc_i2c = combios_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
				break;
			case DDC_CRT2:
				mode_info->bios_connector[i].ddc_i2c = combios_setup_i2c_bus(RADEON_GPIO_CRT2_DDC);
				break;
			default:
				break;
			}

			if (tmp & 0x1)
				mode_info->bios_connector[i].dac_type = DAC_TVDAC;
			else
				mode_info->bios_connector[i].dac_type = DAC_PRIMARY;

			if ((dev_priv->chip_family == CHIP_RS300) ||
			    (dev_priv->chip_family == CHIP_RS400) ||
			    (dev_priv->chip_family == CHIP_RS480))
				mode_info->bios_connector[i].dac_type = DAC_TVDAC;

			if ((tmp >> 4) & 0x1)
				mode_info->bios_connector[i].tmds_type = TMDS_EXT;
			else
				mode_info->bios_connector[i].tmds_type = TMDS_INT;

			radeon_apply_legacy_quirks(dev, i);
		}
	} else {
		uint16_t tmds_info = combios_get_table_offset(dev, COMBIOS_DFP_INFO_TABLE);
		if (tmds_info) {
			DRM_DEBUG("Found DFP table, assuming DVI connector\n");

			mode_info->bios_connector[0].valid = true;
			mode_info->bios_connector[0].connector_type = CONNECTOR_DVI_I;
			mode_info->bios_connector[0].dac_type = DAC_PRIMARY;
			mode_info->bios_connector[0].tmds_type = TMDS_INT;
			mode_info->bios_connector[0].ddc_i2c = combios_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
		} else {
			DRM_DEBUG("No connector info found\n");
			return false;
		}
	}

	if (dev_priv->flags & RADEON_IS_MOBILITY ||
	    dev_priv->chip_family == CHIP_RS400 ||
	    dev_priv->chip_family == CHIP_RS480) {
		uint16_t lcd_info = combios_get_table_offset(dev, COMBIOS_LCD_INFO_TABLE);
		if (lcd_info) {
			uint16_t lcd_ddc_info = combios_get_table_offset(dev, COMBIOS_LCD_DDC_INFO_TABLE);

			mode_info->bios_connector[4].valid = true;
			mode_info->bios_connector[4].connector_type = CONNECTOR_LVDS;
			mode_info->bios_connector[4].dac_type = DAC_NONE;
			mode_info->bios_connector[4].tmds_type = TMDS_NONE;
			mode_info->bios_connector[4].ddc_i2c.valid = false;

			if (lcd_ddc_info) {
				ddc_type = radeon_bios8(dev_priv, lcd_ddc_info + 2);
				switch(ddc_type) {
				case DDC_MONID:
					mode_info->bios_connector[4].ddc_i2c =
						combios_setup_i2c_bus(RADEON_GPIO_MONID);
					break;
				case DDC_DVI:
					mode_info->bios_connector[4].ddc_i2c =
						combios_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
					break;
				case DDC_VGA:
					mode_info->bios_connector[4].ddc_i2c =
						combios_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
					break;
				case DDC_CRT2:
					mode_info->bios_connector[4].ddc_i2c =
						combios_setup_i2c_bus(RADEON_GPIO_CRT2_DDC);
					break;
				case DDC_LCD:
					mode_info->bios_connector[4].ddc_i2c =
						combios_setup_i2c_bus(RADEON_LCD_GPIO_MASK);
					mode_info->bios_connector[4].ddc_i2c.mask_clk_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 3);
					mode_info->bios_connector[4].ddc_i2c.mask_data_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 7);
					mode_info->bios_connector[4].ddc_i2c.a_clk_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 3);
					mode_info->bios_connector[4].ddc_i2c.a_data_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 7);
					mode_info->bios_connector[4].ddc_i2c.put_clk_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 3);
					mode_info->bios_connector[4].ddc_i2c.put_data_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 7);
					mode_info->bios_connector[4].ddc_i2c.get_clk_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 3);
					mode_info->bios_connector[4].ddc_i2c.get_data_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 7);
					break;
				case DDC_GPIO:
					mode_info->bios_connector[4].ddc_i2c =
						combios_setup_i2c_bus(RADEON_MDGPIO_EN_REG);
					mode_info->bios_connector[4].ddc_i2c.mask_clk_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 3);
					mode_info->bios_connector[4].ddc_i2c.mask_data_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 7);
					mode_info->bios_connector[4].ddc_i2c.a_clk_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 3);
					mode_info->bios_connector[4].ddc_i2c.a_data_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 7);
					mode_info->bios_connector[4].ddc_i2c.put_clk_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 3);
					mode_info->bios_connector[4].ddc_i2c.put_data_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 7);
					mode_info->bios_connector[4].ddc_i2c.get_clk_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 3);
					mode_info->bios_connector[4].ddc_i2c.get_data_mask =
						radeon_bios32(dev_priv, lcd_ddc_info + 7);
					break;
				default:
					break;
				}
				DRM_DEBUG("LCD DDC Info Table found!\n");
			}
		} else
			mode_info->bios_connector[4].ddc_i2c.valid = false;
	}

	/* check TV table */
	if (dev_priv->chip_family != CHIP_R100 &&
	    dev_priv->chip_family != CHIP_R200) {
		uint32_t tv_info = combios_get_table_offset(dev, COMBIOS_TV_INFO_TABLE);
		if (tv_info) {
			if (radeon_bios8(dev_priv, tv_info + 6) == 'T') {
				mode_info->bios_connector[5].valid = true;
				mode_info->bios_connector[5].connector_type = CONNECTOR_DIN;
				mode_info->bios_connector[5].dac_type = DAC_TVDAC;
				mode_info->bios_connector[5].tmds_type = TMDS_NONE;
				mode_info->bios_connector[5].ddc_i2c.valid = false;
			}
		}
	}


	DRM_DEBUG("BIOS Connector table\n");
	for (i = 0; i < ATOM_MAX_SUPPORTED_DEVICE; i++) {
		if (!mode_info->bios_connector[i].valid)
			continue;

		DRM_DEBUG("Port %d: ddc_type 0x%x, dac_type %d, tmds_type %d, connector type %d, hpd_mask %d\n",
			  i, mode_info->bios_connector[i].ddc_i2c.mask_clk_reg,
			  mode_info->bios_connector[i].dac_type,
			  mode_info->bios_connector[i].tmds_type,
			  mode_info->bios_connector[i].connector_type,
			  mode_info->bios_connector[i].hpd_mask);
	}

	return true;
}

static void combios_parse_mmio_table(struct drm_device *dev, uint16_t offset)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	if (offset) {
		while (radeon_bios16(dev_priv, offset)) {
			uint16_t cmd  = ((radeon_bios16(dev_priv, offset) & 0xe000) >> 13);
			uint32_t addr = (radeon_bios16(dev_priv, offset) & 0x1fff);
			uint32_t val, and_mask, or_mask;
			uint32_t tmp;

			offset += 2;
			switch (cmd) {
			case 0:
				val = radeon_bios32(dev_priv, offset);
				offset += 4;
				RADEON_WRITE(addr, val);
				break;
			case 1:
				val = radeon_bios32(dev_priv, offset);
				offset += 4;
				RADEON_WRITE(addr, val);
				break;
			case 2:
				and_mask = radeon_bios32(dev_priv, offset);
				offset += 4;
				or_mask = radeon_bios32(dev_priv, offset);
				offset += 4;
				tmp = RADEON_READ(addr);
				tmp &= and_mask;
				tmp |= or_mask;
				RADEON_WRITE(addr, tmp);
				break;
			case 3:
				and_mask = radeon_bios32(dev_priv, offset);
				offset += 4;
				or_mask = radeon_bios32(dev_priv, offset);
				offset += 4;
				tmp = RADEON_READ(addr);
				tmp &= and_mask;
				tmp |= or_mask;
				RADEON_WRITE(addr, tmp);
				break;
			case 4:
				val = radeon_bios16(dev_priv, offset);
				offset += 2;
				udelay(val);
				break;
			case 5:
				val = radeon_bios16(dev_priv, offset);
				offset += 2;
				switch (addr) {
				case 8:
					while (val--) {
						if (!(RADEON_READ_PLL(dev_priv, RADEON_CLK_PWRMGT_CNTL) &
						      RADEON_MC_BUSY))
							break;
					}
					break;
				case 9:
					while (val--) {
						if ((RADEON_READ(RADEON_MC_STATUS) &
						      RADEON_MC_IDLE))
							break;
					}
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}
	}
}

static void combios_parse_pll_table(struct drm_device *dev, uint16_t offset)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	if (offset) {
		while (radeon_bios8(dev_priv, offset)) {
			uint8_t cmd  = ((radeon_bios8(dev_priv, offset) & 0xc0) >> 6);
			uint8_t addr = (radeon_bios8(dev_priv, offset) & 0x3f);
			uint32_t val, shift, tmp;
			uint32_t and_mask, or_mask;

			offset++;
			switch (cmd) {
			case 0:
				val = radeon_bios32(dev_priv, offset);
				offset += 4;
				RADEON_WRITE_PLL(dev_priv, addr, val);
				break;
			case 1:
				shift = radeon_bios8(dev_priv, offset) * 8;
				offset++;
				and_mask = radeon_bios8(dev_priv, offset) << shift;
				and_mask |= ~(0xff << shift);
				offset++;
				or_mask = radeon_bios8(dev_priv, offset) << shift;
				offset++;
				tmp = RADEON_READ_PLL(dev_priv, addr);
				tmp &= and_mask;
				tmp |= or_mask;
				RADEON_WRITE_PLL(dev_priv, addr, tmp);
				break;
			case 2:
			case 3:
				tmp = 1000;
				switch (addr) {
				case 1:
					udelay(150);
					break;
				case 2:
					udelay(1000);
					break;
				case 3:
					while (tmp--) {
						if (!(RADEON_READ_PLL(dev_priv, RADEON_CLK_PWRMGT_CNTL) &
						      RADEON_MC_BUSY))
							break;
					}
					break;
				case 4:
					while (tmp--) {
						if (RADEON_READ_PLL(dev_priv, RADEON_CLK_PWRMGT_CNTL) &
						    RADEON_DLL_READY)
							break;
					}
					break;
				case 5:
					tmp = RADEON_READ_PLL(dev_priv, RADEON_CLK_PWRMGT_CNTL);
					if (tmp & RADEON_CG_NO1_DEBUG_0) {
#if 0
						uint32_t mclk_cntl = RADEON_READ_PLL(RADEON_MCLK_CNTL);
						mclk_cntl &= 0xffff0000;
						//mclk_cntl |= 0x00001111; /* ??? */
						RADEON_WRITE_PLL(dev_priv, RADEON_MCLK_CNTL, mclk_cntl);
						udelay(10000);
#endif
						RADEON_WRITE_PLL(dev_priv, RADEON_CLK_PWRMGT_CNTL,
								 tmp & ~RADEON_CG_NO1_DEBUG_0);
						udelay(10000);
					}
					break;
				default:
					break;
				}
				break;
			default:
				break;
			}
		}
	}
}

static void combios_parse_ram_reset_table(struct drm_device *dev, uint16_t offset)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t tmp;

	if (offset) {
		uint8_t val = radeon_bios8(dev_priv, offset);
		while (val != 0xff) {
			offset++;

			if (val == 0x0f) {
				uint32_t channel_complete_mask;

				if (radeon_is_r300(dev_priv))
					channel_complete_mask = R300_MEM_PWRUP_COMPLETE;
				else
					channel_complete_mask = RADEON_MEM_PWRUP_COMPLETE;
				tmp = 20000;
				while (tmp--) {
					if ((RADEON_READ(RADEON_MEM_STR_CNTL) &
					     channel_complete_mask) ==
					    channel_complete_mask)
						break;
				}
			} else {
				uint32_t or_mask = radeon_bios16(dev_priv, offset);
				offset += 2;

				tmp = RADEON_READ(RADEON_MEM_SDRAM_MODE_REG);
				tmp &= RADEON_SDRAM_MODE_MASK;
				tmp |= or_mask;
				RADEON_WRITE(RADEON_MEM_SDRAM_MODE_REG, tmp);

				or_mask = val << 24;
				tmp = RADEON_READ(RADEON_MEM_SDRAM_MODE_REG);
				tmp &= RADEON_B3MEM_RESET_MASK;
				tmp |= or_mask;
				RADEON_WRITE(RADEON_MEM_SDRAM_MODE_REG, tmp);
			}
			val = radeon_bios8(dev_priv, offset);
		}
	}
}

void radeon_combios_dyn_clk_setup(struct drm_device *dev, int enable)
{
	uint16_t dyn_clk_info = combios_get_table_offset(dev, COMBIOS_DYN_CLK_1_TABLE);

	if (dyn_clk_info)
		combios_parse_pll_table(dev, dyn_clk_info);
}

void radeon_combios_asic_init(struct drm_device *dev)
{
	uint16_t table;

	/* ASIC INIT 1 */
	table = combios_get_table_offset(dev, COMBIOS_ASIC_INIT_1_TABLE);
	if (table)
		combios_parse_mmio_table(dev, table);

	/* PLL INIT */
	table = combios_get_table_offset(dev, COMBIOS_PLL_INIT_TABLE);
	if (table)
		combios_parse_pll_table(dev, table);

	/* ASIC INIT 2 */
	table = combios_get_table_offset(dev, COMBIOS_ASIC_INIT_2_TABLE);
	if (table)
		combios_parse_mmio_table(dev, table);

	/* ASIC INIT 4 */
	table = combios_get_table_offset(dev, COMBIOS_ASIC_INIT_4_TABLE);
	if (table)
		combios_parse_mmio_table(dev, table);

	/* RAM RESET */
	table = combios_get_table_offset(dev, COMBIOS_RAM_RESET_TABLE);
	if (table)
		combios_parse_ram_reset_table(dev, table);

	/* ASIC INIT 3 */
	table = combios_get_table_offset(dev, COMBIOS_ASIC_INIT_3_TABLE);
	if (table)
		combios_parse_mmio_table(dev, table);

	/* DYN CLK 1 */
	table = combios_get_table_offset(dev, COMBIOS_DYN_CLK_1_TABLE);
	if (table)
		combios_parse_pll_table(dev, table);

	/* ASIC INIT 5 */
	table = combios_get_table_offset(dev, COMBIOS_ASIC_INIT_5_TABLE);
	if (table)
		combios_parse_mmio_table(dev, table);

}

void radeon_combios_initialize_bios_scratch_regs(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t bios_0_scratch, bios_6_scratch, bios_7_scratch;

	bios_0_scratch = RADEON_READ(RADEON_BIOS_0_SCRATCH);
	bios_6_scratch = RADEON_READ(RADEON_BIOS_6_SCRATCH);
	//bios_7_scratch = RADEON_READ(RADEON_BIOS_7_SCRATCH);

	/* let the bios control the backlight */
	bios_0_scratch &= ~RADEON_DRIVER_BRIGHTNESS_EN;

	/* tell the bios not to handle mode switching */
	bios_6_scratch |= (RADEON_DISPLAY_SWITCHING_DIS |
				 RADEON_ACC_MODE_CHANGE);

	/* tell the bios a driver is loaded */
	//bios_7_scratch |= RADEON_DRV_LOADED;

	RADEON_WRITE(RADEON_BIOS_0_SCRATCH, bios_0_scratch);
	RADEON_WRITE(RADEON_BIOS_6_SCRATCH, bios_6_scratch);
	//RADEON_WRITE(RADEON_BIOS_7_SCRATCH, bios_7_scratch);
}

void
radeon_combios_output_lock(struct drm_encoder *encoder, bool lock)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t bios_6_scratch;

	bios_6_scratch = RADEON_READ(RADEON_BIOS_6_SCRATCH);

	if (lock)
		bios_6_scratch |= RADEON_DRIVER_CRITICAL;
	else
		bios_6_scratch &= ~RADEON_DRIVER_CRITICAL;

	RADEON_WRITE(RADEON_BIOS_6_SCRATCH, bios_6_scratch);
}
