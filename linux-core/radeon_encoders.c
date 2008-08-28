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
#include "drm_crtc_helper.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

extern int atom_debug;

void radeon_rmx_mode_fixup(struct drm_encoder *encoder,
			   struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode)
{
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	if (mode->hdisplay < radeon_encoder->panel_xres ||
	    mode->vdisplay < radeon_encoder->panel_yres) {
		radeon_encoder->flags |= RADEON_USE_RMX;
		adjusted_mode->hdisplay = radeon_encoder->panel_xres;
		adjusted_mode->vdisplay = radeon_encoder->panel_yres;
		adjusted_mode->htotal = radeon_encoder->panel_xres + radeon_encoder->hblank;
		adjusted_mode->hsync_start = radeon_encoder->panel_xres + radeon_encoder->hoverplus;
		adjusted_mode->hsync_end = adjusted_mode->hsync_start + radeon_encoder->hsync_width;
		adjusted_mode->vtotal = radeon_encoder->panel_yres + radeon_encoder->vblank;
		adjusted_mode->vsync_start = radeon_encoder->panel_yres + radeon_encoder->voverplus;
		adjusted_mode->vsync_end = adjusted_mode->vsync_start + radeon_encoder->vsync_width;
		/* update crtc values */
		drm_mode_set_crtcinfo(adjusted_mode, CRTC_INTERLACE_HALVE_V);
		/* adjust crtc values */
		adjusted_mode->crtc_hdisplay = radeon_encoder->panel_xres;
		adjusted_mode->crtc_vdisplay = radeon_encoder->panel_yres;
		adjusted_mode->crtc_htotal = adjusted_mode->crtc_hdisplay + radeon_encoder->hblank;
		adjusted_mode->crtc_hsync_start = adjusted_mode->crtc_hdisplay + radeon_encoder->hoverplus;
		adjusted_mode->crtc_hsync_end = adjusted_mode->crtc_hsync_start + radeon_encoder->hsync_width;
		adjusted_mode->crtc_vtotal = adjusted_mode->crtc_vdisplay + radeon_encoder->vblank;
		adjusted_mode->crtc_vsync_start = adjusted_mode->crtc_vdisplay + radeon_encoder->voverplus;
		adjusted_mode->crtc_vsync_end = adjusted_mode->crtc_vsync_start + radeon_encoder->vsync_width;
	} else {
		adjusted_mode->htotal = radeon_encoder->panel_xres + radeon_encoder->hblank;
		adjusted_mode->hsync_start = radeon_encoder->panel_xres + radeon_encoder->hoverplus;
		adjusted_mode->hsync_end = adjusted_mode->hsync_start + radeon_encoder->hsync_width;
		adjusted_mode->vtotal = radeon_encoder->panel_yres + radeon_encoder->vblank;
		adjusted_mode->vsync_start = radeon_encoder->panel_yres + radeon_encoder->voverplus;
		adjusted_mode->vsync_end = adjusted_mode->vsync_start + radeon_encoder->vsync_width;
		/* update crtc values */
		drm_mode_set_crtcinfo(adjusted_mode, CRTC_INTERLACE_HALVE_V);
		/* adjust crtc values */
		adjusted_mode->crtc_htotal = adjusted_mode->crtc_hdisplay + radeon_encoder->hblank;
		adjusted_mode->crtc_hsync_start = adjusted_mode->crtc_hdisplay + radeon_encoder->hoverplus;
		adjusted_mode->crtc_hsync_end = adjusted_mode->crtc_hsync_start + radeon_encoder->hsync_width;
		adjusted_mode->crtc_vtotal = adjusted_mode->crtc_vdisplay + radeon_encoder->vblank;
		adjusted_mode->crtc_vsync_start = adjusted_mode->crtc_vdisplay + radeon_encoder->voverplus;
		adjusted_mode->crtc_vsync_end = adjusted_mode->crtc_vsync_start + radeon_encoder->vsync_width;
	}
	adjusted_mode->clock = radeon_encoder->dotclock;
	adjusted_mode->flags = radeon_encoder->flags;
}


static int atom_dac_find_atom_type(struct radeon_encoder *radeon_encoder, struct drm_connector *connector)
{
	struct drm_device *dev = radeon_encoder->base.dev;
	struct drm_connector *connector_find;
	int atom_type = -1;

	if (!connector) {
		list_for_each_entry(connector_find, &dev->mode_config.connector_list, head) {
			if (connector_find->encoder == &radeon_encoder->base)
				connector = connector_find;
		}
	}
	if (connector) {
		/* look for the encoder in the connector list -
		   check if we the DAC is enabled on a VGA or STV/CTV or CV connector */
		/* work out the ATOM_DEVICE bits */
		switch (connector->connector_type) {
		case CONNECTOR_VGA:
		case CONNECTOR_DVI_I:
		case CONNECTOR_DVI_A:
			if (radeon_encoder->atom_device & ATOM_DEVICE_CRT1_SUPPORT)
				atom_type = ATOM_DEVICE_CRT1_INDEX;
			else if (radeon_encoder->atom_device & ATOM_DEVICE_CRT2_SUPPORT)
				atom_type = ATOM_DEVICE_CRT2_INDEX;
			break;
		case CONNECTOR_STV:
		case CONNECTOR_CTV:
			if (radeon_encoder->atom_device & ATOM_DEVICE_TV1_SUPPORT)
				atom_type = ATOM_DEVICE_TV1_INDEX;
			break;
		case CONNECTOR_DIN:
			if (radeon_encoder->atom_device & ATOM_DEVICE_TV1_SUPPORT)
				atom_type = ATOM_DEVICE_TV1_INDEX;
			if (radeon_encoder->atom_device & ATOM_DEVICE_CV_SUPPORT)
				atom_type = ATOM_DEVICE_CV_INDEX;
			break;
		}
	}

	return atom_type;
}

/* LVTMA encoder for LVDS usage */
static void atombios_display_device_control(struct drm_encoder *encoder, int index, uint8_t state)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION args;

	memset(&args, 0, sizeof(args));
	args.ucAction = state;

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

static void atombios_scaler_setup(struct drm_encoder *encoder, struct drm_display_mode *mode)
{
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(encoder->crtc);
	ENABLE_SCALER_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, EnableScaler);

	memset(&args, 0, sizeof(args));
	args.ucScaler = radeon_crtc->crtc_id;

	if (radeon_encoder->flags & RADEON_USE_RMX) {
		if (radeon_encoder->rmx_type == RMX_FULL)
			args.ucEnable = ATOM_SCALER_EXPANSION;
		else if (radeon_encoder->rmx_type == RMX_CENTER)
			args.ucEnable = ATOM_SCALER_CENTER;
	} else
		args.ucEnable = ATOM_SCALER_DISABLE;

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

void atombios_set_crtc_source(struct drm_encoder *encoder, int source)
{
	int index = GetIndexIntoMasterTable(COMMAND, SelectCRTC_Source);
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(encoder->crtc);
	struct drm_radeon_private *dev_priv = encoder->dev->dev_private;
	uint8_t frev, crev;
	SELECT_CRTC_SOURCE_PS_ALLOCATION crtc_src_param;
	SELECT_CRTC_SOURCE_PARAMETERS_V2 crtc_src_param2;
	uint32_t *param = NULL;

	atom_parse_cmd_header(dev_priv->mode_info.atom_context, index, &frev, &crev);
	switch (frev) {
	case 1: {
		switch (crev) {
		case 0:
		case 1:
		default:
			memset(&crtc_src_param, 0, sizeof(crtc_src_param));
			crtc_src_param.ucCRTC = radeon_crtc->crtc_id;
			crtc_src_param.ucDevice = source;
			param = (uint32_t *)&crtc_src_param;
			break;
		case 2:
			memset(&crtc_src_param2, 0, sizeof(crtc_src_param2));
			crtc_src_param2.ucCRTC = radeon_crtc->crtc_id;
			crtc_src_param2.ucEncoderID = source;
			switch (source) {
			case ATOM_DEVICE_CRT1_INDEX:
			case ATOM_DEVICE_CRT2_INDEX:
				crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_CRT;
				break;
			case ATOM_DEVICE_DFP1_INDEX:
			case ATOM_DEVICE_DFP2_INDEX:
			case ATOM_DEVICE_DFP3_INDEX:
				crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_DVI;
				// TODO ENCODER MODE
				break;
			case ATOM_DEVICE_LCD1_INDEX:
				crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_LVDS;
				break;
			case ATOM_DEVICE_TV1_INDEX:
				crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_TV;
				break;
			case ATOM_DEVICE_CV_INDEX:
				crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_CV;
				break;
			}
			param = (uint32_t *)&crtc_src_param2;
			break;
		}
	}
		break;
	default:
		return;
	}

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)param);

}

static void radeon_dfp_disable_dither(struct drm_encoder *encoder, int device)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;

	switch (device) {
	case ATOM_DEVICE_DFP1_INDEX:
		RADEON_WRITE(AVIVO_TMDSA_BIT_DEPTH_CONTROL, 0); /* TMDSA */
		break;
	case ATOM_DEVICE_DFP2_INDEX:
		if ((dev_priv->chip_family == CHIP_RS600) ||
		    (dev_priv->chip_family == CHIP_RS690) ||
		    (dev_priv->chip_family == CHIP_RS740))
			RADEON_WRITE(AVIVO_DDIA_BIT_DEPTH_CONTROL, 0); /* DDIA */
		else
			RADEON_WRITE(AVIVO_DVOA_BIT_DEPTH_CONTROL, 0); /* DVO */
		break;
		/*case ATOM_DEVICE_LCD1_INDEX:*/ /* LVDS panels need dither enabled */
	case ATOM_DEVICE_DFP3_INDEX:
		RADEON_WRITE(AVIVO_LVTMA_BIT_DEPTH_CONTROL, 0); /* LVTMA */
		break;
	default:
		break;
	}
}


static void radeon_lvtma_mode_set(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	LVDS_ENCODER_CONTROL_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, LVDSEncoderControl);

	memset(&args, 0, sizeof(args));
	atombios_scaler_setup(encoder, mode);
	atombios_set_crtc_source(encoder, ATOM_DEVICE_LCD1_INDEX);

	args.ucAction = 1;
	if (adjusted_mode->clock > 165000)
		args.ucMisc = 1;
	else
		args.ucMisc = 0;
	args.usPixelClock = cpu_to_le16(adjusted_mode->clock / 10);

	printk("executing set LVDS encoder\n");
	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}


static void radeon_lvtma_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc;
	int index = GetIndexIntoMasterTable(COMMAND, LCD1OutputControl);
	uint32_t bios_2_scratch, bios_3_scratch;
	int crtc_id = 0;

	if (encoder->crtc) {
		struct radeon_crtc *radeon_crtc = to_radeon_crtc(encoder->crtc);
		crtc_id = radeon_crtc->crtc_id;
	}

	if (dev_priv->chip_family >= CHIP_R600) {
		bios_2_scratch = RADEON_READ(R600_BIOS_2_SCRATCH);
		bios_3_scratch = RADEON_READ(R600_BIOS_3_SCRATCH);
	} else {
		bios_2_scratch = RADEON_READ(RADEON_BIOS_2_SCRATCH);
		bios_3_scratch = RADEON_READ(RADEON_BIOS_3_SCRATCH);
	}

	bios_2_scratch &= ~ATOM_S3_LCD1_CRTC_ACTIVE;
	bios_3_scratch |= (crtc_id << 17);

	switch(mode) {
	case DRM_MODE_DPMS_ON:
		atombios_display_device_control(encoder, index, ATOM_ENABLE);
		bios_2_scratch &= ~ATOM_S2_LCD1_DPMS_STATE;
		bios_3_scratch |= ATOM_S3_LCD1_ACTIVE;
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		atombios_display_device_control(encoder, index, ATOM_DISABLE);
		bios_2_scratch |= ATOM_S2_LCD1_DPMS_STATE;
		bios_3_scratch &= ~ATOM_S3_LCD1_ACTIVE;
		break;
	}

	if (dev_priv->chip_family >= CHIP_R600) {
		RADEON_WRITE(R600_BIOS_2_SCRATCH, bios_2_scratch);
		RADEON_WRITE(R600_BIOS_3_SCRATCH, bios_3_scratch);
	} else {
		RADEON_WRITE(RADEON_BIOS_2_SCRATCH, bios_2_scratch);
		RADEON_WRITE(RADEON_BIOS_3_SCRATCH, bios_3_scratch);
	}
}

static bool radeon_lvtma_mode_fixup(struct drm_encoder *encoder,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);

	radeon_encoder->flags &= ~RADEON_USE_RMX;

	if (radeon_encoder->rmx_type != RMX_OFF)
		radeon_rmx_mode_fixup(encoder, mode, adjusted_mode);

	return true;
}

static void radeon_lvtma_prepare(struct drm_encoder *encoder)
{
	radeon_atom_output_lock(encoder, true);
	radeon_lvtma_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_lvtma_commit(struct drm_encoder *encoder)
{
	radeon_lvtma_dpms(encoder, DRM_MODE_DPMS_ON);
	radeon_atom_output_lock(encoder, false);
}

static const struct drm_encoder_helper_funcs radeon_atom_lvtma_helper_funcs = {
	.dpms = radeon_lvtma_dpms,
	.mode_fixup = radeon_lvtma_mode_fixup,
	.prepare = radeon_lvtma_prepare,
	.mode_set = radeon_lvtma_mode_set,
	.commit = radeon_lvtma_commit,
};

void radeon_enc_destroy(struct drm_encoder *encoder)
{
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	drm_encoder_cleanup(encoder);
	kfree(radeon_encoder);
}

static const struct drm_encoder_funcs radeon_atom_lvtma_enc_funcs = {
	.destroy = radeon_enc_destroy,
};

struct drm_encoder *radeon_encoder_lvtma_add(struct drm_device *dev, int bios_index)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder;
	struct drm_encoder *encoder;
	radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	/* Set LVTMA to only use crtc 0 */
	encoder->possible_crtcs = 0x1;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_atom_lvtma_enc_funcs,
			 DRM_MODE_ENCODER_LVDS);

	drm_encoder_helper_add(encoder, &radeon_atom_lvtma_helper_funcs);
	radeon_encoder->atom_device = mode_info->bios_connector[bios_index].devices;

	/* TODO get the LVDS info from the BIOS for panel size etc. */
	/* get the lvds info from the bios */
	radeon_get_lvds_info(radeon_encoder);

	/* LVDS gets default RMX full scaling */
	radeon_encoder->rmx_type = RMX_FULL;

	return encoder;
}

static void radeon_atom_dac_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	struct radeon_crtc *radeon_crtc;
	int atom_type = -1;
	int index;
	uint32_t bios_2_scratch, bios_3_scratch;
	int crtc_id = 0;

	if (encoder->crtc) {
		radeon_crtc = to_radeon_crtc(encoder->crtc);
		crtc_id = radeon_crtc->crtc_id;
	}

	atom_type = atom_dac_find_atom_type(radeon_encoder, NULL);
	if (atom_type == -1)
		return;

	if (dev_priv->chip_family >= CHIP_R600) {
		bios_2_scratch = RADEON_READ(R600_BIOS_2_SCRATCH);
		bios_3_scratch = RADEON_READ(R600_BIOS_3_SCRATCH);
	} else {
		bios_2_scratch = RADEON_READ(RADEON_BIOS_2_SCRATCH);
		bios_3_scratch = RADEON_READ(RADEON_BIOS_3_SCRATCH);
	}

	switch(atom_type) {
	case ATOM_DEVICE_CRT1_INDEX:
		index = GetIndexIntoMasterTable(COMMAND, DAC1OutputControl);
		bios_2_scratch &= ~ATOM_S3_CRT1_CRTC_ACTIVE;
		bios_3_scratch |= (crtc_id << 16);
		switch(mode) {
		case DRM_MODE_DPMS_ON:
			bios_2_scratch &= ~ATOM_S2_CRT1_DPMS_STATE;
			bios_3_scratch |= ATOM_S3_CRT1_ACTIVE;
			break;
		case DRM_MODE_DPMS_STANDBY:
		case DRM_MODE_DPMS_SUSPEND:
		case DRM_MODE_DPMS_OFF:
			bios_2_scratch |= ATOM_S2_CRT1_DPMS_STATE;
			bios_3_scratch &= ~ATOM_S3_CRT1_ACTIVE;
			break;
		}
		break;
	case ATOM_DEVICE_CRT2_INDEX:
		index = GetIndexIntoMasterTable(COMMAND, DAC2OutputControl);
		bios_2_scratch &= ~ATOM_S3_CRT2_CRTC_ACTIVE;
		bios_3_scratch |= (crtc_id << 20);
		switch(mode) {
		case DRM_MODE_DPMS_ON:
			bios_2_scratch &= ~ATOM_S2_CRT2_DPMS_STATE;
			bios_3_scratch |= ATOM_S3_CRT2_ACTIVE;
			break;
		case DRM_MODE_DPMS_STANDBY:
		case DRM_MODE_DPMS_SUSPEND:
		case DRM_MODE_DPMS_OFF:
			bios_2_scratch |= ATOM_S2_CRT2_DPMS_STATE;
			bios_3_scratch &= ~ATOM_S3_CRT2_ACTIVE;
			break;
		}
		break;
	case ATOM_DEVICE_TV1_INDEX:
		index = GetIndexIntoMasterTable(COMMAND, TV1OutputControl);
		bios_3_scratch &= ~ATOM_S3_TV1_CRTC_ACTIVE;
		bios_3_scratch |= (crtc_id << 18);
		switch(mode) {
		case DRM_MODE_DPMS_ON:
			bios_2_scratch &= ~ATOM_S2_TV1_DPMS_STATE;
			bios_3_scratch |= ATOM_S3_TV1_ACTIVE;
			break;
		case DRM_MODE_DPMS_STANDBY:
		case DRM_MODE_DPMS_SUSPEND:
		case DRM_MODE_DPMS_OFF:
			bios_2_scratch |= ATOM_S2_TV1_DPMS_STATE;
			bios_3_scratch &= ~ATOM_S3_TV1_ACTIVE;
			break;
		}
		break;
	case ATOM_DEVICE_CV_INDEX:
		index = GetIndexIntoMasterTable(COMMAND, CV1OutputControl);
		bios_2_scratch &= ~ATOM_S3_CV_CRTC_ACTIVE;
		bios_3_scratch |= (crtc_id << 24);
		switch(mode) {
		case DRM_MODE_DPMS_ON:
			bios_2_scratch &= ~ATOM_S2_CV_DPMS_STATE;
			bios_3_scratch |= ATOM_S3_CV_ACTIVE;
			break;
		case DRM_MODE_DPMS_STANDBY:
		case DRM_MODE_DPMS_SUSPEND:
		case DRM_MODE_DPMS_OFF:
			bios_2_scratch |= ATOM_S2_CV_DPMS_STATE;
			bios_3_scratch &= ~ATOM_S3_CV_ACTIVE;
			break;
		}
		break;
	default:
		return;
	}

	switch(mode) {
	case DRM_MODE_DPMS_ON:
		atombios_display_device_control(encoder, index, ATOM_ENABLE);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		atombios_display_device_control(encoder, index, ATOM_DISABLE);
		break;
	}

	if (dev_priv->chip_family >= CHIP_R600) {
		RADEON_WRITE(R600_BIOS_2_SCRATCH, bios_2_scratch);
		RADEON_WRITE(R600_BIOS_3_SCRATCH, bios_3_scratch);
	} else {
		RADEON_WRITE(RADEON_BIOS_2_SCRATCH, bios_2_scratch);
		RADEON_WRITE(RADEON_BIOS_3_SCRATCH, bios_3_scratch);
	}
}

static bool radeon_atom_dac_mode_fixup(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void radeon_atom_dac_prepare(struct drm_encoder *encoder)
{
	radeon_atom_output_lock(encoder, true);
	radeon_atom_dac_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_atom_dac_commit(struct drm_encoder *encoder)
{
	radeon_atom_dac_dpms(encoder, DRM_MODE_DPMS_ON);
	radeon_atom_output_lock(encoder, false);
}

static int atombios_dac_setup(struct drm_encoder *encoder,
			      struct drm_display_mode *mode,
			      int atom_type)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	DAC_ENCODER_CONTROL_PS_ALLOCATION args;
	int id = (radeon_encoder->type.dac == DAC_TVDAC);
	int index;

	memset(&args, 0, sizeof(args));
	if (id == 0)
		index = GetIndexIntoMasterTable(COMMAND, DAC1EncoderControl);
	else
		index = GetIndexIntoMasterTable(COMMAND, DAC2EncoderControl);

	args.ucAction = 1;
	args.usPixelClock = cpu_to_le16(mode->clock / 10);
	if ((atom_type == ATOM_DEVICE_CRT1_INDEX) ||
	    (atom_type == ATOM_DEVICE_CRT2_INDEX))
		args.ucDacStandard = id ? ATOM_DAC2_PS2 : ATOM_DAC1_PS2;
	else if (atom_type == ATOM_DEVICE_CV_INDEX)
		args.ucDacStandard = id ? ATOM_DAC2_CV : ATOM_DAC1_CV;
	else if (atom_type == ATOM_DEVICE_TV1_INDEX)
		args.ucDacStandard = id ? ATOM_DAC2_NTSC : ATOM_DAC1_NTSC;
	/* TODO PAL */
	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);

	return 0;
}

static int atombios_tv1_setup(struct drm_encoder *encoder,
			      struct drm_display_mode *mode,
			      int atom_type)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	TV_ENCODER_CONTROL_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, TVEncoderControl);

	memset(&args, 0, sizeof(args));
	args.sTVEncoder.ucAction = 1;
	if (atom_type == ATOM_DEVICE_CV_INDEX)
		args.sTVEncoder.ucTvStandard = ATOM_TV_CV;
	else {
		// TODO PAL
		args.sTVEncoder.ucTvStandard = ATOM_TV_NTSC;
	}

	args.sTVEncoder.usPixelClock = cpu_to_le16(mode->clock / 10);

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
	return 0;
}

static void radeon_atom_dac_mode_set(struct drm_encoder *encoder,
				     struct drm_display_mode *mode,
				     struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	int atom_type = -1;

	atom_type = atom_dac_find_atom_type(radeon_encoder, NULL);
	if (atom_type == -1)
		return;

	atombios_scaler_setup(encoder, mode);
	atombios_set_crtc_source(encoder, atom_type);

	atombios_dac_setup(encoder, adjusted_mode, atom_type);
	if ((atom_type == ATOM_DEVICE_TV1_INDEX) ||
	    (atom_type == ATOM_DEVICE_CV_INDEX))
		atombios_tv1_setup(encoder, adjusted_mode, atom_type);

}

static bool atom_dac_load_detect(struct drm_encoder *encoder, int atom_devices)
{
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	DAC_LOAD_DETECTION_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, DAC_LoadDetection);

	memset(&args, 0, sizeof(args));
	args.sDacload.ucMisc = 0;
	args.sDacload.ucDacType = (radeon_encoder->type.dac == DAC_PRIMARY) ? ATOM_DAC_A : ATOM_DAC_B;

	if (atom_devices & ATOM_DEVICE_CRT1_SUPPORT)
		args.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_CRT1_SUPPORT);
	else if (atom_devices & ATOM_DEVICE_CRT2_SUPPORT)
		args.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_CRT2_SUPPORT);
	else if (atom_devices & ATOM_DEVICE_CV_SUPPORT) {
		args.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_CV_SUPPORT);
		if (radeon_is_dce3(dev_priv))
			args.sDacload.ucMisc = 1;
	} else if (atom_devices & ATOM_DEVICE_TV1_SUPPORT) {
		args.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_TV1_SUPPORT);
		if (radeon_is_dce3(dev_priv))
			args.sDacload.ucMisc = 1;
	} else
		return false;

	DRM_DEBUG("writing %x %x\n", args.sDacload.usDeviceID, args.sDacload.ucDacType);
	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
	return true;
}

static enum drm_connector_status radeon_atom_dac_detect(struct drm_encoder *encoder, struct drm_connector *connector)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	int atom_type = -1;
	uint32_t bios_0_scratch;

	atom_type = atom_dac_find_atom_type(radeon_encoder, connector);
	if (atom_type == -1) {
		DRM_DEBUG("exit after find \n");
		return connector_status_unknown;
	}

	if(!atom_dac_load_detect(encoder, (1 << atom_type))) {
		DRM_DEBUG("detect returned false \n");
		return connector_status_unknown;
	}


	if (dev_priv->chip_family >= CHIP_R600)
		bios_0_scratch = RADEON_READ(R600_BIOS_0_SCRATCH);
	else
		bios_0_scratch = RADEON_READ(RADEON_BIOS_0_SCRATCH);

	DRM_DEBUG("Bios 0 scratch %x\n", bios_0_scratch);
	if (radeon_encoder->atom_device & ATOM_DEVICE_CRT1_SUPPORT) {
		if (bios_0_scratch & ATOM_S0_CRT1_MASK)
			return connector_status_connected;
	} else if (radeon_encoder->atom_device & ATOM_DEVICE_CRT2_SUPPORT) {
		if (bios_0_scratch & ATOM_S0_CRT2_MASK)
			return connector_status_connected;
	} else if (radeon_encoder->atom_device & ATOM_DEVICE_CV_SUPPORT) {
		if (bios_0_scratch & (ATOM_S0_CV_MASK|ATOM_S0_CV_MASK_A))
			return connector_status_connected;
	} else if (radeon_encoder->atom_device & ATOM_DEVICE_TV1_SUPPORT) {
		if (bios_0_scratch & (ATOM_S0_TV1_COMPOSITE | ATOM_S0_TV1_COMPOSITE_A))
			return connector_status_connected; // CTV
		else if (bios_0_scratch & (ATOM_S0_TV1_SVIDEO | ATOM_S0_TV1_SVIDEO_A))
			return connector_status_connected; // STV
	}
	return connector_status_disconnected;
}

static const struct drm_encoder_helper_funcs radeon_atom_dac_helper_funcs = {
	.dpms = radeon_atom_dac_dpms,
	.mode_fixup = radeon_atom_dac_mode_fixup,
	.prepare = radeon_atom_dac_prepare,
	.mode_set = radeon_atom_dac_mode_set,
	.commit = radeon_atom_dac_commit,
	.detect = radeon_atom_dac_detect,
};

static const struct drm_encoder_funcs radeon_atom_dac_enc_funcs = {
	. destroy = radeon_enc_destroy,
};


static void atombios_tmds1_setup(struct drm_encoder *encoder,
				 struct drm_display_mode *mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	TMDS1_ENCODER_CONTROL_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, TMDS1EncoderControl);

	memset(&args, 0, sizeof(args));
	args.ucAction = 1;
	if (mode->clock > 165000)
		args.ucMisc = 1;
	else
		args.ucMisc = 0;

	args.usPixelClock = cpu_to_le16(mode->clock / 10);

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

static void atombios_tmds2_setup(struct drm_encoder *encoder,
				 struct drm_display_mode *mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	TMDS2_ENCODER_CONTROL_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, TMDS2EncoderControl);

	memset(&args, 0, sizeof(args));
	args.ucAction = 1;
	if (mode->clock > 165000)
		args.ucMisc = 1;
	else
		args.ucMisc = 0;

	args.usPixelClock = cpu_to_le16(mode->clock / 10);

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}


static void atombios_ext_tmds_setup(struct drm_encoder *encoder,
				    struct drm_display_mode *mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, DVOEncoderControl);

	memset(&args, 0, sizeof(args));
	args.sXTmdsEncoder.ucEnable = 1;

	if (mode->clock > 165000)
		args.sXTmdsEncoder.ucMisc = 1;
	else
		args.sXTmdsEncoder.ucMisc = 0;

	// TODO 6-bit DAC
//	args.usPixelClock = cpu_to_le16(mode->clock / 10);

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

static void atombios_dig1_setup(struct drm_encoder *encoder,
				struct drm_display_mode *mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	DIG_ENCODER_CONTROL_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, DIG1EncoderControl);

	args.ucAction = 1;
	args.usPixelClock = mode->clock / 10;
	args.ucConfig = ATOM_ENCODER_CONFIG_TRANSMITTER1;

	// TODO coherent mode
//	if (encoder->coherent_mode)
//		args.ucConfig |= ATOM_TRANSMITTER_CONFIG_COHERENT;

	if (mode->clock > 165000) {
		args.ucConfig |= ATOM_ENCODER_CONFIG_LINKA_B;
		args.ucLaneNum = 8;
	} else {
		args.ucConfig |= ATOM_ENCODER_CONFIG_LINKA;
		args.ucLaneNum = 4;
	}

	// TODO Encoder MODE
	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

static void atombios_ddia_setup(struct drm_encoder *encoder,
				struct drm_display_mode *mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	DVO_ENCODER_CONTROL_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, DVOEncoderControl);

	args.sDVOEncoder.ucAction = ATOM_ENABLE;
	args.sDVOEncoder.usPixelClock = mode->clock / 10;

	if (mode->clock > 165000)
		args.sDVOEncoder.usDevAttr.sDigAttrib.ucAttribute = PANEL_ENCODER_MISC_DUAL;
	else
		args.sDVOEncoder.usDevAttr.sDigAttrib.ucAttribute = 0;

	atom_execute_table(dev_priv->mode_info.atom_context, index, (uint32_t *)&args);
}

struct drm_encoder *radeon_encoder_atom_dac_add(struct drm_device *dev, int bios_index, int dac_type, int with_tv)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder = NULL;
	struct drm_encoder *encoder;
	int type = with_tv ? DRM_MODE_ENCODER_TVDAC : DRM_MODE_ENCODER_DAC;
	int found = 0;
	int digital_enc_mask = ~(ATOM_DEVICE_DFP1_SUPPORT | ATOM_DEVICE_DFP2_SUPPORT | ATOM_DEVICE_DFP3_SUPPORT |
				ATOM_DEVICE_LCD1_SUPPORT);
	/* we may already have added this encoder */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->encoder_type != DRM_MODE_ENCODER_DAC ||
		    encoder->encoder_type != DRM_MODE_ENCODER_TVDAC)
			continue;

		radeon_encoder = to_radeon_encoder(encoder);
		if (radeon_encoder->type.dac == dac_type) {
			found = 1;
			break;
		}
	}

	if (found) {
		/* upgrade to a TV controlling DAC */
		if (type == DRM_MODE_ENCODER_TVDAC)
			encoder->encoder_type = type;
		radeon_encoder->atom_device |= mode_info->bios_connector[bios_index].devices;
		radeon_encoder->atom_device &= digital_enc_mask;
		return encoder;
	}

	radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	encoder->possible_crtcs = 0x3;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_atom_dac_enc_funcs,
			 type);

	drm_encoder_helper_add(encoder, &radeon_atom_dac_helper_funcs);
	radeon_encoder->type.dac = dac_type;
	radeon_encoder->atom_device = mode_info->bios_connector[bios_index].devices;

	/* mask off any digital encoders */
	radeon_encoder->atom_device &= digital_enc_mask;
	return encoder;
}

static void radeon_atom_tmds_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	struct radeon_crtc *radeon_crtc = NULL;
	int crtc_id = 0;
	int atom_type = -1;
	int index = -1;
	uint32_t bios_2_scratch, bios_3_scratch;

	if (encoder->crtc) {
		radeon_crtc = to_radeon_crtc(encoder->crtc);
		crtc_id = radeon_crtc->crtc_id;
	} else if (mode == DRM_MODE_DPMS_ON)
		return;

	if (radeon_encoder->atom_device & ATOM_DEVICE_DFP1_SUPPORT)
		atom_type = ATOM_DEVICE_DFP1_INDEX;
	if (radeon_encoder->atom_device & ATOM_DEVICE_DFP2_SUPPORT)
		atom_type = ATOM_DEVICE_DFP2_INDEX;
	if (radeon_encoder->atom_device & ATOM_DEVICE_DFP3_SUPPORT)
		atom_type = ATOM_DEVICE_DFP3_INDEX;

	if (atom_type == -1)
		return;

	if (dev_priv->chip_family >= CHIP_R600) {
		bios_2_scratch = RADEON_READ(R600_BIOS_2_SCRATCH);
		bios_3_scratch = RADEON_READ(R600_BIOS_3_SCRATCH);
	} else {
		bios_2_scratch = RADEON_READ(RADEON_BIOS_2_SCRATCH);
		bios_3_scratch = RADEON_READ(RADEON_BIOS_3_SCRATCH);
	}

	switch(atom_type) {
	case ATOM_DEVICE_DFP1_INDEX:
		index = GetIndexIntoMasterTable(COMMAND, TMDSAOutputControl);
		bios_2_scratch &= ~ATOM_S3_DFP1_CRTC_ACTIVE;
		bios_3_scratch |= (crtc_id << 19);
		switch(mode) {
		case DRM_MODE_DPMS_ON:
			bios_2_scratch &= ~ATOM_S2_DFP1_DPMS_STATE;
			bios_3_scratch |= ATOM_S3_DFP1_ACTIVE;
			break;
		case DRM_MODE_DPMS_STANDBY:
		case DRM_MODE_DPMS_SUSPEND:
		case DRM_MODE_DPMS_OFF:
			bios_2_scratch |= ATOM_S2_DFP1_DPMS_STATE;
			bios_3_scratch &= ~ATOM_S3_DFP1_ACTIVE;
			break;
		}
		break;
	case ATOM_DEVICE_DFP2_INDEX:
		index = GetIndexIntoMasterTable(COMMAND, DVOOutputControl);
		bios_2_scratch &= ~ATOM_S3_DFP2_CRTC_ACTIVE;
		bios_3_scratch |= (crtc_id << 23);
		switch(mode) {
		case DRM_MODE_DPMS_ON:
			bios_2_scratch &= ~ATOM_S2_DFP2_DPMS_STATE;
			bios_3_scratch |= ATOM_S3_DFP2_ACTIVE;
			break;
		case DRM_MODE_DPMS_STANDBY:
		case DRM_MODE_DPMS_SUSPEND:
		case DRM_MODE_DPMS_OFF:
			bios_2_scratch |= ATOM_S2_DFP2_DPMS_STATE;
			bios_3_scratch &= ~ATOM_S3_DFP2_ACTIVE;
			break;
		}
		break;
	case ATOM_DEVICE_DFP3_INDEX:
		index = GetIndexIntoMasterTable(COMMAND, LVTMAOutputControl);
		bios_2_scratch &= ~ATOM_S3_DFP3_CRTC_ACTIVE;
		bios_3_scratch |= (crtc_id << 25);
		switch(mode) {
		case DRM_MODE_DPMS_ON:
			bios_2_scratch &= ~ATOM_S2_DFP3_DPMS_STATE;
			bios_3_scratch |= ATOM_S3_DFP3_ACTIVE;
			break;
		case DRM_MODE_DPMS_STANDBY:
		case DRM_MODE_DPMS_SUSPEND:
		case DRM_MODE_DPMS_OFF:
			bios_2_scratch |= ATOM_S2_DFP3_DPMS_STATE;
			bios_3_scratch &= ~ATOM_S3_DFP3_ACTIVE;
			break;
		}
		break;
	}

	if (index == -1)
		return;

	switch(mode) {
	case DRM_MODE_DPMS_ON:
		atombios_display_device_control(encoder, index, ATOM_ENABLE);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		atombios_display_device_control(encoder, index, ATOM_DISABLE);
		break;
	}

	if (dev_priv->chip_family >= CHIP_R600) {
		RADEON_WRITE(R600_BIOS_2_SCRATCH, bios_2_scratch);
		RADEON_WRITE(R600_BIOS_3_SCRATCH, bios_3_scratch);
	} else {
		RADEON_WRITE(RADEON_BIOS_2_SCRATCH, bios_2_scratch);
		RADEON_WRITE(RADEON_BIOS_3_SCRATCH, bios_3_scratch);
	}
}

static bool radeon_atom_tmds_mode_fixup(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void radeon_atom_tmds_mode_set(struct drm_encoder *encoder,
				      struct drm_display_mode *mode,
				      struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	int atom_type;

	if (radeon_encoder->atom_device & ATOM_DEVICE_DFP1_SUPPORT)
		atom_type = ATOM_DEVICE_DFP1_INDEX;
	if (radeon_encoder->atom_device & ATOM_DEVICE_DFP2_SUPPORT)
		atom_type = ATOM_DEVICE_DFP2_INDEX;
	if (radeon_encoder->atom_device & ATOM_DEVICE_DFP3_SUPPORT)
		atom_type = ATOM_DEVICE_DFP3_INDEX;

	atombios_scaler_setup(encoder, mode);
	atombios_set_crtc_source(encoder, atom_type);

	if (atom_type == ATOM_DEVICE_DFP1_INDEX)
		atombios_tmds1_setup(encoder, adjusted_mode);
	if (atom_type == ATOM_DEVICE_DFP2_INDEX) {
		if ((dev_priv->chip_family == CHIP_RS600) ||
		    (dev_priv->chip_family == CHIP_RS690) ||
		    (dev_priv->chip_family == CHIP_RS740))
			atombios_ddia_setup(encoder, adjusted_mode);
		else
			atombios_ext_tmds_setup(encoder, adjusted_mode);
	}
	if (atom_type == ATOM_DEVICE_DFP3_INDEX)
		atombios_tmds2_setup(encoder, adjusted_mode);
	radeon_dfp_disable_dither(encoder, atom_type);


}

static void radeon_atom_tmds_prepare(struct drm_encoder *encoder)
{
	radeon_atom_output_lock(encoder, true);
	radeon_atom_tmds_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_atom_tmds_commit(struct drm_encoder *encoder)
{
	radeon_atom_tmds_dpms(encoder, DRM_MODE_DPMS_ON);
	radeon_atom_output_lock(encoder, false);
}

static const struct drm_encoder_helper_funcs radeon_atom_tmds_helper_funcs = {
	.dpms = radeon_atom_tmds_dpms,
	.mode_fixup = radeon_atom_tmds_mode_fixup,
	.prepare = radeon_atom_tmds_prepare,
	.mode_set = radeon_atom_tmds_mode_set,
	.commit = radeon_atom_tmds_commit,
	/* no detect for TMDS */
};

static const struct drm_encoder_funcs radeon_atom_tmds_enc_funcs = {
	. destroy = radeon_enc_destroy,
};

struct drm_encoder *radeon_encoder_atom_tmds_add(struct drm_device *dev, int bios_index, int tmds_type)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder = NULL;
	struct drm_encoder *encoder;
	int analog_enc_mask = ~(ATOM_DEVICE_CRT1_SUPPORT | ATOM_DEVICE_CRT2_SUPPORT);

		radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	encoder->possible_crtcs = 0x3;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_atom_tmds_enc_funcs,
			 DRM_MODE_ENCODER_TMDS);

	drm_encoder_helper_add(encoder, &radeon_atom_tmds_helper_funcs);

	radeon_encoder->atom_device = mode_info->bios_connector[bios_index].devices;

	/* mask off any analog encoders */
	radeon_encoder->atom_device &= analog_enc_mask;
	return encoder;
}
