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

struct radeon_i2c_bus_rec combios_setup_i2c_bus(int ddc_line)
{
	struct radeon_i2c_bus_rec i2c;

	i2c.mask_clk_mask = RADEON_GPIO_EN_1 | RADEON_GPIO_Y_1;
	i2c.mask_data_mask =  RADEON_GPIO_EN_0 | RADEON_GPIO_Y_0;
	i2c.put_clk_mask = RADEON_GPIO_EN_1;
	i2c.put_data_mask = RADEON_GPIO_EN_0;
	i2c.get_clk_mask = RADEON_GPIO_Y_1;
	i2c.get_data_mask = RADEON_GPIO_Y_0;
	if ((ddc_line == RADEON_LCD_GPIO_MASK) ||
	    (ddc_line == RADEON_MDGPIO_EN_REG)) {
		i2c.mask_clk_reg = ddc_line;
		i2c.mask_data_reg = ddc_line;
		i2c.put_clk_reg = ddc_line;
		i2c.put_data_reg = ddc_line;
		i2c.get_clk_reg = ddc_line + 4;
		i2c.get_data_reg = ddc_line + 4;
	} else {
		i2c.mask_clk_reg = ddc_line;
		i2c.mask_data_reg = ddc_line;
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
	uint16_t pll_info_block;
	struct radeon_pll *pll = &mode_info->pll;
	int rev;

	pll_info_block = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x30);
	rev = radeon_bios8(dev_priv, pll_info_block);

	pll->reference_freq = radeon_bios16(dev_priv, pll_info_block + 0xe);
	pll->reference_div = radeon_bios16(dev_priv, pll_info_block + 0x10);
	pll->pll_out_min = radeon_bios32(dev_priv, pll_info_block + 0x12);
	pll->pll_out_max = radeon_bios32(dev_priv, pll_info_block + 0x16);

	if (rev > 9) {
		pll->pll_in_min = radeon_bios32(dev_priv, pll_info_block + 0x36);
		pll->pll_in_max = radeon_bios32(dev_priv, pll_info_block + 0x3a);
	} else {
		pll->pll_in_min = 40;
		pll->pll_in_max = 500;
	}

	pll->xclk = radeon_bios16(dev_priv, pll_info_block + 0x08);

	// sclk/mclk use fixed point
	
	return true;

}

bool radeon_combios_get_lvds_info(struct radeon_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint16_t tmp;
	char stmp[30];
	int tmp0;
	int i;

	tmp = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x40);
	if (!tmp) {
		DRM_INFO("No panel info found in BIOS\n");
		return false;

	}

	for (i = 0; i < 24; i++)
		stmp[i] = radeon_bios8(dev_priv, tmp + i + 1);
	stmp[24] = 0;

	DRM_INFO("Panel ID String: %s\n", stmp);

	encoder->panel_xres = radeon_bios16(dev_priv, tmp + 25);
	encoder->panel_yres = radeon_bios16(dev_priv, tmp + 27);

	DRM_INFO("Panel Size %dx%d\n", encoder->panel_xres, encoder->panel_yres);

	encoder->panel_pwr_delay = radeon_bios16(dev_priv, tmp + 44);
	if (encoder->panel_pwr_delay > 2000 || encoder->panel_pwr_delay < 0)
		encoder->panel_pwr_delay = 2000;

	for (i = 0; i < 32; i++) {
		tmp0 = radeon_bios16(dev_priv, tmp + 64 + i * 2);
		if (tmp0 == 0) break;

		if ((radeon_bios16(dev_priv, tmp0) == encoder->panel_xres) &&
		    (radeon_bios16(dev_priv, tmp0 + 2) == encoder->panel_yres)) {
			encoder->hblank = (radeon_bios16(dev_priv, tmp0 + 17) -
					   radeon_bios16(dev_priv, tmp0 + 19)) * 8;
			encoder->hoverplus = (radeon_bios16(dev_priv, tmp0 + 21) -
					      radeon_bios16(dev_priv, tmp0 + 19) - 1) * 8;
			encoder->hsync_width = radeon_bios8(dev_priv, tmp0 + 23) * 8;

			encoder->vblank = (radeon_bios16(dev_priv, tmp0 + 24) -
					   radeon_bios16(dev_priv, tmp0 + 26));
			encoder->voverplus = ((radeon_bios16(dev_priv, tmp0 + 28) & 0x7fff) -
					      radeon_bios16(dev_priv, tmp0 + 26));
			encoder->vsync_width = ((radeon_bios16(dev_priv, tmp0 + 28) & 0xf800) >> 11);
			encoder->dotclock = radeon_bios16(dev_priv, tmp0 + 9) * 10;
			encoder->flags = 0;
		}
	}
	return true;
}

bool radeon_combios_get_tmds_info(struct radeon_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint16_t tmp;
	int i, n;
	uint8_t ver;

	tmp = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x34);
	if (!tmp) {
		DRM_INFO("No TMDS info found in BIOS\n");
		return false;
	}

	ver = radeon_bios8(dev_priv, tmp);
	DRM_INFO("DFP table revision: %d\n", ver);
	if (ver == 3) {
		n = radeon_bios8(dev_priv, tmp + 5) + 1;
		if (n > 4) n = 4;
		for (i = 0; i < n; i++) {
			encoder->tmds_pll[i].value = radeon_bios32(dev_priv, tmp+i*10+0x08);
			encoder->tmds_pll[i].freq = radeon_bios16(dev_priv, tmp+i*10+0x10);	
		}
		return true;
	} else if (ver == 4) {
		int stride = 0;
		n = radeon_bios8(dev_priv, tmp + 5) + 1;
		if (n > 4) n = 4;
		for (i = 0; i < n; i++) {
			encoder->tmds_pll[i].value = radeon_bios32(dev_priv, tmp+stride+0x08);
			encoder->tmds_pll[i].freq = radeon_bios16(dev_priv, tmp+stride+0x10);
			if (i == 0) stride += 10;
			else stride += 6;
		}
		return true;
	}
	return false;
}

static void radeon_apply_legacy_quirks(struct drm_device *dev, int bios_index)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;

	/* on XPRESS chips, CRT2_DDC and MONID_DCC both use the 
	 * MONID gpio, but use different pins.
	 * CRT2_DDC uses the standard pinout, MONID_DDC uses
	 * something else.
	 */
	if ((dev_priv->chip_family == CHIP_RS400 ||
	     dev_priv->chip_family == CHIP_RS480) &&
	    mode_info->bios_connector[bios_index].connector_type == CONNECTOR_VGA &&
	    mode_info->bios_connector[bios_index].ddc_i2c.mask_clk_reg == RADEON_GPIO_CRT2_DDC) {
		mode_info->bios_connector[bios_index].ddc_i2c = combios_setup_i2c_bus(RADEON_GPIO_MONID);
	}

	/* XPRESS desktop chips seem to have a proprietary connector listed for
	 * DVI-D, try and do the right thing here.
	 */
	if ((dev_priv->flags & RADEON_IS_MOBILITY) &&
	    (mode_info->bios_connector[bios_index].connector_type == CONNECTOR_LVDS)) {
	  DRM_INFO("proprietary connector found. assuming DVI-D\n");
	  mode_info->bios_connector[bios_index].dac_type = DAC_NONE;
	  mode_info->bios_connector[bios_index].tmds_type = TMDS_EXT;
	  mode_info->bios_connector[bios_index].connector_type = CONNECTOR_DVI_D;
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

}

bool radeon_get_legacy_connector_info_from_bios(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	uint32_t offset, entry;
	uint16_t tmp0, tmp1, tmp;
	enum radeon_combios_ddc ddctype;
	enum radeon_combios_connector connector_type;
	int i;
	
	DRM_DEBUG("\n");
	offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x50);
	if (offset) {
		for (i = 0; i < 4; i++) {
			entry = offset + 2 + i * 2;
	
			if (!radeon_bios16(dev_priv, entry))
				    break;
			
			mode_info->bios_connector[i].valid = true;

			tmp = radeon_bios16(dev_priv, entry);

			connector_type = (tmp >> 12) & 0xf;
			mode_info->bios_connector[i].connector_type = connector_type;

			switch(connector_type) {
			case CONNECTOR_PROPRIETARY_LEGACY:
				mode_info->bios_connector[i].connector_type = CONNECTOR_LVDS;
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

			ddctype = (tmp >> 8) & 0xf;
			switch (ddctype) {
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
		DRM_INFO("no connector table found in BIOS\n");
		offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x34);
		if (offset) {
			DRM_DEBUG("Found DFP table, assuming DVI connector\n");

			mode_info->bios_connector[0].valid = true;
			mode_info->bios_connector[0].connector_type = CONNECTOR_DVI_I;
			mode_info->bios_connector[0].dac_type = DAC_PRIMARY;
			mode_info->bios_connector[0].tmds_type = TMDS_INT;
			mode_info->bios_connector[0].ddc_i2c = combios_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
		} else {
			DRM_DEBUG("No table found\n");
			return false;
		}
	}

	if (dev_priv->flags & RADEON_IS_MOBILITY) {
		offset = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x40);
		if (offset) {
			mode_info->bios_connector[4].valid = true;
			mode_info->bios_connector[4].connector_type = CONNECTOR_LVDS;
			mode_info->bios_connector[4].dac_type = DAC_NONE;
			mode_info->bios_connector[4].tmds_type = TMDS_NONE;
			mode_info->bios_connector[4].ddc_i2c.valid = false;

			tmp = radeon_bios16(dev_priv, dev_priv->bios_header_start + 0x42);
			if (tmp) {
				tmp0 = radeon_bios16(dev_priv, tmp + 0x15);
				if (tmp0) {
					tmp1 = radeon_bios8(dev_priv, tmp0 + 2) & 0x07;
					if (tmp1) {
						ddctype = tmp1;

						switch(ddctype) {
						case DDC_MONID:
						case DDC_DVI:
						case DDC_CRT2:
						case DDC_LCD:
						case DDC_GPIO:
						default:
							break;
						}
						DRM_DEBUG("LCD DDC Info Table found!\n");
					}
				}
			} else
				mode_info->bios_connector[4].ddc_i2c.valid = false;
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
		
