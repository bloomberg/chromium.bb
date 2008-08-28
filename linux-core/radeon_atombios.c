/*
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

#include "atom.h"
#include "atom-bits.h"


union atom_supported_devices {
  struct _ATOM_SUPPORTED_DEVICES_INFO info;
  struct _ATOM_SUPPORTED_DEVICES_INFO_2 info_2;
  struct _ATOM_SUPPORTED_DEVICES_INFO_2d1 info_2d1;
};

static inline struct radeon_i2c_bus_rec radeon_lookup_gpio_for_ddc(struct drm_device *dev, uint8_t id)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct atom_context *ctx = dev_priv->mode_info.atom_context;
	ATOM_GPIO_I2C_ASSIGMENT gpio;
	struct radeon_i2c_bus_rec i2c;
	int index = GetIndexIntoMasterTable(DATA, GPIO_I2C_Info);
	struct _ATOM_GPIO_I2C_INFO *i2c_info;
	uint16_t data_offset;

	memset(&i2c, 0, sizeof(struct radeon_i2c_bus_rec));
	i2c.valid = false;

	atom_parse_data_header(ctx, index, NULL, NULL, NULL, &data_offset);

	i2c_info = (struct _ATOM_GPIO_I2C_INFO *)(ctx->bios + data_offset);

	gpio = i2c_info->asGPIO_Info[id];

	i2c.mask_clk_reg = le16_to_cpu(gpio.usClkMaskRegisterIndex) * 4;
	i2c.mask_data_reg = le16_to_cpu(gpio.usDataMaskRegisterIndex) * 4;
	i2c.put_clk_reg = le16_to_cpu(gpio.usClkEnRegisterIndex) * 4;
	i2c.put_data_reg = le16_to_cpu(gpio.usDataEnRegisterIndex) * 4;
	i2c.get_clk_reg = le16_to_cpu(gpio.usClkY_RegisterIndex) * 4;
	i2c.get_data_reg = le16_to_cpu(gpio.usDataY_RegisterIndex) * 4;
	i2c.a_clk_reg = le16_to_cpu(gpio.usClkA_RegisterIndex) * 4;
	i2c.a_data_reg = le16_to_cpu(gpio.usDataA_RegisterIndex) * 4;
	i2c.mask_clk_mask = (1 << gpio.ucClkMaskShift);
	i2c.mask_data_mask = (1 << gpio.ucDataMaskShift);
	i2c.put_clk_mask = (1 << gpio.ucClkEnShift);
	i2c.put_data_mask = (1 << gpio.ucDataEnShift);
	i2c.get_clk_mask = (1 << gpio.ucClkY_Shift);
	i2c.get_data_mask = (1 <<  gpio.ucDataY_Shift);
	i2c.a_clk_mask = (1 << gpio.ucClkA_Shift);
	i2c.a_data_mask = (1 <<  gpio.ucDataA_Shift);
	i2c.valid = true;

	return i2c;
}

static void radeon_atom_apply_quirks(struct drm_device *dev, int index)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;

	if ((dev->pdev->device == 0x791e) &&
	    (dev->pdev->subsystem_vendor == 0x1043) &&
	    (dev->pdev->subsystem_device == 0x826d)) {
		if ((mode_info->bios_connector[index].connector_type == CONNECTOR_HDMI_TYPE_A) &&
		    (mode_info->bios_connector[index].tmds_type == TMDS_LVTMA)) {
			mode_info->bios_connector[index].connector_type = CONNECTOR_DVI_D;
		}
	}

	if ((dev->pdev->device == 0x5653) &&
	    (dev->pdev->subsystem_vendor == 0x1462) &&
	    (dev->pdev->subsystem_device == 0x0291)) {
		if (mode_info->bios_connector[index].connector_type == CONNECTOR_LVDS) {
			mode_info->bios_connector[index].ddc_i2c.valid = false;
		}
	}
}

bool radeon_get_atom_connector_info_from_bios_connector_table(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct atom_context *ctx = mode_info->atom_context;
	int index = GetIndexIntoMasterTable(DATA, SupportedDevicesInfo);
	uint16_t size, data_offset;
	uint8_t frev, crev;
	uint16_t device_support;

	union atom_supported_devices *supported_devices;
	int i,j;
	atom_parse_data_header(ctx, index, &size, &frev, &crev, &data_offset);

	supported_devices = (union atom_supported_devices *)(ctx->bios + data_offset);

	device_support = le16_to_cpu(supported_devices->info.usDeviceSupport);

	for (i = 0; i < ATOM_MAX_SUPPORTED_DEVICE; i++) {

		ATOM_CONNECTOR_INFO_I2C ci = supported_devices->info.asConnInfo[i];

		if (!(device_support & (1 << i))) {
			mode_info->bios_connector[i].valid = false;
			continue;
		}

		if (i == ATOM_DEVICE_CV_INDEX) {
			DRM_DEBUG("Skipping Component Video\n");
			mode_info->bios_connector[i].valid = false;
			continue;
		}

		if (i == ATOM_DEVICE_TV1_INDEX) {
			DRM_DEBUG("Skipping TV Out\n");
			mode_info->bios_connector[i].valid = false;
			continue;
		}

		mode_info->bios_connector[i].valid = true;
		mode_info->bios_connector[i].output_id = ci.sucI2cId.sbfAccess.bfI2C_LineMux;
		mode_info->bios_connector[i].devices = 1 << i;
		mode_info->bios_connector[i].connector_type = ci.sucConnectorInfo.sbfAccess.bfConnectorType;

		if (mode_info->bios_connector[i].connector_type == CONNECTOR_NONE) {
			mode_info->bios_connector[i].valid = false;
			continue;
		}

		mode_info->bios_connector[i].dac_type = ci.sucConnectorInfo.sbfAccess.bfAssociatedDAC;

		if ((i == ATOM_DEVICE_TV1_INDEX) ||
		    (i == ATOM_DEVICE_TV2_INDEX) ||
		    (i == ATOM_DEVICE_TV1_INDEX))
			mode_info->bios_connector[i].ddc_i2c.valid = false;
		else if ((dev_priv->chip_family == CHIP_RS600) ||
			 (dev_priv->chip_family == CHIP_RS690) ||
			 (dev_priv->chip_family == CHIP_RS740)) {
			if ((i == ATOM_DEVICE_DFP2_INDEX) || (i == ATOM_DEVICE_DFP3_INDEX))
				mode_info->bios_connector[i].ddc_i2c =
					radeon_lookup_gpio_for_ddc(dev, ci.sucI2cId.sbfAccess.bfI2C_LineMux + 1);
			else
				mode_info->bios_connector[i].ddc_i2c =
					radeon_lookup_gpio_for_ddc(dev, ci.sucI2cId.sbfAccess.bfI2C_LineMux);
		} else
			mode_info->bios_connector[i].ddc_i2c =
				radeon_lookup_gpio_for_ddc(dev, ci.sucI2cId.sbfAccess.bfI2C_LineMux);

		if (i == ATOM_DEVICE_DFP1_INDEX)
			mode_info->bios_connector[i].tmds_type = TMDS_INT;
		else if (i == ATOM_DEVICE_DFP2_INDEX) {
			if ((dev_priv->chip_family == CHIP_RS600) ||
			    (dev_priv->chip_family == CHIP_RS690) ||
			    (dev_priv->chip_family == CHIP_RS740))
				mode_info->bios_connector[i].tmds_type = TMDS_DDIA;
			else
				mode_info->bios_connector[i].tmds_type = TMDS_EXT;
		} else if (i == ATOM_DEVICE_DFP3_INDEX)
			mode_info->bios_connector[i].tmds_type = TMDS_LVTMA;
		else
			mode_info->bios_connector[i].tmds_type = TMDS_NONE;

		/* Always set the connector type to VGA for CRT1/CRT2. if they are
		 * shared with a DVI port, we'll pick up the DVI connector below when we
		 * merge the outputs
		 */
		if ((i == ATOM_DEVICE_CRT1_INDEX || i == ATOM_DEVICE_CRT2_INDEX) &&
		    (mode_info->bios_connector[i].connector_type == CONNECTOR_DVI_I ||
		     mode_info->bios_connector[i].connector_type == CONNECTOR_DVI_D ||
		     mode_info->bios_connector[i].connector_type == CONNECTOR_DVI_A)) {
			mode_info->bios_connector[i].connector_type = CONNECTOR_VGA;
		}

		if (crev > 1) {
			ATOM_CONNECTOR_INC_SRC_BITMAP isb = supported_devices->info_2.asIntSrcInfo[i];

			switch(isb.ucIntSrcBitmap) {
			case 0x4:
				mode_info->bios_connector[i].hpd_mask = 0x1;
				break;
			case 0xa:
				mode_info->bios_connector[i].hpd_mask = 0x100;
				break;
			default:
				mode_info->bios_connector[i].hpd_mask = 0;
				break;
			}
		} else {
			mode_info->bios_connector[i].hpd_mask = 0;
		}

		radeon_atom_apply_quirks(dev, i);
	}

	/* CRTs/DFPs may share a port */
	for (i = 0; i < ATOM_MAX_SUPPORTED_DEVICE; i++) {
		if (!mode_info->bios_connector[i].valid)
			continue;

		for (j = 0; j < ATOM_MAX_SUPPORTED_DEVICE; j++) {
			if (mode_info->bios_connector[j].valid && (i != j)) {
				if (mode_info->bios_connector[i].output_id ==
				    mode_info->bios_connector[j].output_id) {
					if (((i == ATOM_DEVICE_DFP1_INDEX) ||
					     (i == ATOM_DEVICE_DFP2_INDEX) ||
					     (i == ATOM_DEVICE_DFP3_INDEX)) &&
					    ((j == ATOM_DEVICE_CRT1_INDEX) ||
					     (j == ATOM_DEVICE_CRT2_INDEX))) {
						mode_info->bios_connector[i].dac_type = mode_info->bios_connector[j].dac_type;
						mode_info->bios_connector[i].devices |= mode_info->bios_connector[j].devices;
						mode_info->bios_connector[i].hpd_mask = mode_info->bios_connector[j].hpd_mask;
						mode_info->bios_connector[j].valid = false;
					} else if (((j == ATOM_DEVICE_DFP1_INDEX) ||
						    (j == ATOM_DEVICE_DFP2_INDEX) ||
						    (j == ATOM_DEVICE_DFP3_INDEX)) &&
						   ((i == ATOM_DEVICE_CRT1_INDEX) ||
						    (i == ATOM_DEVICE_CRT2_INDEX))) {
						mode_info->bios_connector[j].dac_type = mode_info->bios_connector[i].dac_type;
						mode_info->bios_connector[j].devices |= mode_info->bios_connector[i].devices;
						mode_info->bios_connector[j].hpd_mask = mode_info->bios_connector[i].hpd_mask;
						mode_info->bios_connector[i].valid = false;
					}
				}
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

union firmware_info {
	ATOM_FIRMWARE_INFO info;
	ATOM_FIRMWARE_INFO_V1_2 info_12;
	ATOM_FIRMWARE_INFO_V1_3 info_13;
	ATOM_FIRMWARE_INFO_V1_4 info_14;
};

bool radeon_atom_get_clock_info(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	int index = GetIndexIntoMasterTable(DATA, FirmwareInfo);
	union firmware_info *firmware_info;
	uint8_t frev, crev;
	struct radeon_pll *pll = &mode_info->pll;
	uint16_t data_offset;

	atom_parse_data_header(mode_info->atom_context, index, NULL, &frev, &crev, &data_offset);

	firmware_info = (union firmware_info *)(mode_info->atom_context->bios + data_offset);

	pll->reference_freq = le16_to_cpu(firmware_info->info.usReferenceClock);
	pll->reference_div = 0;

	pll->pll_out_min = le16_to_cpu(firmware_info->info.usMinPixelClockPLL_Output);
	pll->pll_out_max = le32_to_cpu(firmware_info->info.ulMaxPixelClockPLL_Output);

	if (pll->pll_out_min == 0) {
		if (radeon_is_avivo(dev_priv))
			pll->pll_out_min = 64800;
		else
			pll->pll_out_min = 20000;
	}

	pll->pll_in_min = le16_to_cpu(firmware_info->info.usMinPixelClockPLL_Input);
	pll->pll_in_max = le16_to_cpu(firmware_info->info.usMaxPixelClockPLL_Input);

	pll->xclk = le16_to_cpu(firmware_info->info.usMaxPixelClock);

	return true;
}

union lvds_info {
	struct _ATOM_LVDS_INFO info;
	struct _ATOM_LVDS_INFO_V12 info_12;
};

void radeon_get_lvds_info(struct radeon_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	int index = GetIndexIntoMasterTable(DATA, LVDS_Info);
	uint16_t data_offset;
	union lvds_info *lvds_info;
	uint8_t frev, crev;

	atom_parse_data_header(mode_info->atom_context, index, NULL, &frev, &crev, &data_offset);

	lvds_info = (union lvds_info *)(mode_info->atom_context->bios + data_offset);

	encoder->dotclock = le16_to_cpu(lvds_info->info.sLCDTiming.usPixClk) * 10;
	encoder->panel_xres = le16_to_cpu(lvds_info->info.sLCDTiming.usHActive);
	encoder->panel_yres = le16_to_cpu(lvds_info->info.sLCDTiming.usVActive);
	encoder->hblank = le16_to_cpu(lvds_info->info.sLCDTiming.usHBlanking_Time);
	encoder->hoverplus = le16_to_cpu(lvds_info->info.sLCDTiming.usHSyncOffset);
	encoder->hsync_width = le16_to_cpu(lvds_info->info.sLCDTiming.usHSyncWidth);

	encoder->vblank = le16_to_cpu(lvds_info->info.sLCDTiming.usVBlanking_Time);
	encoder->voverplus = le16_to_cpu(lvds_info->info.sLCDTiming.usVSyncOffset);
	encoder->vsync_width = le16_to_cpu(lvds_info->info.sLCDTiming.usVSyncWidth);
	encoder->panel_pwr_delay = le16_to_cpu(lvds_info->info.usOffDelayInMs);
}

void radeon_atom_dyn_clk_setup(struct drm_device *dev, int enable)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct atom_context *ctx = mode_info->atom_context;
	DYNAMIC_CLOCK_GATING_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, DynamicClockGating);

	args.ucEnable = enable;

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

void radeon_atom_static_pwrmgt_setup(struct drm_device *dev, int enable)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct atom_context *ctx = mode_info->atom_context;
	ENABLE_ASIC_STATIC_PWR_MGT_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, EnableASIC_StaticPwrMgt);

	args.ucEnable = enable;

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

void radeon_atom_initialize_bios_scratch_regs(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t bios_2_scratch, bios_6_scratch;

	if (dev_priv->chip_family >= CHIP_R600) {
		bios_2_scratch = RADEON_READ(RADEON_BIOS_0_SCRATCH);
		bios_6_scratch = RADEON_READ(RADEON_BIOS_6_SCRATCH);
	} else {
		bios_2_scratch = RADEON_READ(RADEON_BIOS_0_SCRATCH);
		bios_6_scratch = RADEON_READ(RADEON_BIOS_6_SCRATCH);
	}

	/* let the bios control the backlight */
	bios_2_scratch &= ~ATOM_S2_VRI_BRIGHT_ENABLE;

	/* tell the bios not to handle mode switching */
	bios_6_scratch |= (ATOM_S6_ACC_BLOCK_DISPLAY_SWITCH |
			   ATOM_S6_ACC_MODE);

	if (dev_priv->chip_family >= CHIP_R600) {
		RADEON_WRITE(R600_BIOS_2_SCRATCH, bios_2_scratch);
		RADEON_WRITE(R600_BIOS_6_SCRATCH, bios_6_scratch);
	} else {
		RADEON_WRITE(RADEON_BIOS_2_SCRATCH, bios_2_scratch);
		RADEON_WRITE(RADEON_BIOS_6_SCRATCH, bios_6_scratch);
	}

}

void
radeon_atom_output_lock(struct drm_encoder *encoder, bool lock)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t bios_6_scratch;

	if (dev_priv->chip_family >= CHIP_R600)
		bios_6_scratch = RADEON_READ(R600_BIOS_6_SCRATCH);
	else
		bios_6_scratch = RADEON_READ(RADEON_BIOS_6_SCRATCH);

	if (lock)
		bios_6_scratch |= ATOM_S6_CRITICAL_STATE;
	else
		bios_6_scratch &= ~ATOM_S6_CRITICAL_STATE;

	if (dev_priv->chip_family >= CHIP_R600)
		RADEON_WRITE(R600_BIOS_6_SCRATCH, bios_6_scratch);
	else
		RADEON_WRITE(RADEON_BIOS_6_SCRATCH, bios_6_scratch);
}
