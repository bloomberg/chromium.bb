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


static void radeon_legacy_rmx_mode_set(struct drm_encoder *encoder,
				       struct drm_display_mode *mode,
				       struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(encoder->crtc);
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	int    xres = mode->hdisplay;
	int    yres = mode->vdisplay;
	bool   hscale = true, vscale = true;
	int    hsync_wid;
	int    vsync_wid;
	int    hsync_start;
	uint32_t scale, inc;
	uint32_t fp_horz_stretch, fp_vert_stretch, crtc_more_cntl, fp_horz_vert_active;
	uint32_t fp_h_sync_strt_wid, fp_v_sync_strt_wid, fp_crtc_h_total_disp, fp_crtc_v_total_disp;

	DRM_DEBUG("\n");

	fp_vert_stretch = RADEON_READ(RADEON_FP_VERT_STRETCH) &
		(RADEON_VERT_STRETCH_RESERVED |
		 RADEON_VERT_AUTO_RATIO_INC);
	fp_horz_stretch = RADEON_READ(RADEON_FP_HORZ_STRETCH) &
		(RADEON_HORZ_FP_LOOP_STRETCH |
		 RADEON_HORZ_AUTO_RATIO_INC);

	crtc_more_cntl = 0;
	if ((dev_priv->chip_family == CHIP_RS100) ||
	    (dev_priv->chip_family == CHIP_RS200)) {
		/* This is to workaround the asic bug for RMX, some versions
		   of BIOS dosen't have this register initialized correctly. */
		crtc_more_cntl |= RADEON_CRTC_H_CUTOFF_ACTIVE_EN;
	}


	fp_crtc_h_total_disp = ((((mode->crtc_htotal / 8) - 1) & 0x3ff)
				| ((((mode->crtc_hdisplay / 8) - 1) & 0x1ff) << 16));

	hsync_wid = (mode->crtc_hsync_end - mode->crtc_hsync_start) / 8;
	if (!hsync_wid)
		hsync_wid = 1;
	hsync_start = mode->crtc_hsync_start - 8;

	fp_h_sync_strt_wid = ((hsync_start & 0x1fff)
			      | ((hsync_wid & 0x3f) << 16)
			      | ((mode->flags & DRM_MODE_FLAG_NHSYNC)
				 ? RADEON_CRTC_H_SYNC_POL
				 : 0));

	fp_crtc_v_total_disp = (((mode->crtc_vtotal - 1) & 0xffff)
				| ((mode->crtc_vdisplay - 1) << 16));

	vsync_wid = mode->crtc_vsync_end - mode->crtc_vsync_start;
	if (!vsync_wid)
		vsync_wid = 1;

	fp_v_sync_strt_wid = (((mode->crtc_vsync_start - 1) & 0xfff)
			      | ((vsync_wid & 0x1f) << 16)
			      | ((mode->flags & DRM_MODE_FLAG_NVSYNC)
				 ? RADEON_CRTC_V_SYNC_POL
				 : 0));

	fp_horz_vert_active = 0;

	if (radeon_encoder->panel_xres == 0 ||
	    radeon_encoder->panel_yres == 0) {
		hscale = false;
		vscale = false;
	} else {
		if (xres > radeon_encoder->panel_xres)
			xres = radeon_encoder->panel_xres;
		if (yres > radeon_encoder->panel_yres)
			yres = radeon_encoder->panel_yres;

		if (xres == radeon_encoder->panel_xres)
			hscale = false;
		if (yres == radeon_encoder->panel_yres)
			vscale = false;
	}

	if (radeon_encoder->flags & RADEON_USE_RMX) {
		if (radeon_encoder->rmx_type != RMX_CENTER) {
			if (!hscale)
				fp_horz_stretch |= ((xres/8-1) << 16);
			else {
				inc = (fp_horz_stretch & RADEON_HORZ_AUTO_RATIO_INC) ? 1 : 0;
				scale = ((xres + inc) * RADEON_HORZ_STRETCH_RATIO_MAX)
					/ radeon_encoder->panel_xres + 1;
				fp_horz_stretch |= (((scale) & RADEON_HORZ_STRETCH_RATIO_MASK) |
						    RADEON_HORZ_STRETCH_BLEND |
						    RADEON_HORZ_STRETCH_ENABLE |
						    ((radeon_encoder->panel_xres/8-1) << 16));
			}

			if (!vscale)
				fp_vert_stretch |= ((yres-1) << 12);
			else {
				inc = (fp_vert_stretch & RADEON_VERT_AUTO_RATIO_INC) ? 1 : 0;
				scale = ((yres + inc) * RADEON_VERT_STRETCH_RATIO_MAX)
					/ radeon_encoder->panel_yres + 1;
				fp_vert_stretch |= (((scale) & RADEON_VERT_STRETCH_RATIO_MASK) |
						    RADEON_VERT_STRETCH_ENABLE |
						    RADEON_VERT_STRETCH_BLEND |
						    ((radeon_encoder->panel_yres-1) << 12));
			}
		} else if (radeon_encoder->rmx_type == RMX_CENTER) {
			int    blank_width;

			fp_horz_stretch |= ((xres/8-1) << 16);
			fp_vert_stretch |= ((yres-1) << 12);

			crtc_more_cntl |= (RADEON_CRTC_AUTO_HORZ_CENTER_EN |
					   RADEON_CRTC_AUTO_VERT_CENTER_EN);

			blank_width = (mode->crtc_hblank_end - mode->crtc_hblank_start) / 8;
			if (blank_width > 110)
				blank_width = 110;

			fp_crtc_h_total_disp = (((blank_width) & 0x3ff)
						| ((((mode->crtc_hdisplay / 8) - 1) & 0x1ff) << 16));

			hsync_wid = (mode->crtc_hsync_end - mode->crtc_hsync_start) / 8;
			if (!hsync_wid)
				hsync_wid = 1;

			fp_h_sync_strt_wid = ((((mode->crtc_hsync_start - mode->crtc_hblank_start) / 8) & 0x1fff)
					      | ((hsync_wid & 0x3f) << 16)
					      | ((mode->flags & DRM_MODE_FLAG_NHSYNC)
						 ? RADEON_CRTC_H_SYNC_POL
						 : 0));

			fp_crtc_v_total_disp = (((mode->crtc_vblank_end - mode->crtc_vblank_start) & 0xffff)
						| ((mode->crtc_vdisplay - 1) << 16));

			vsync_wid = mode->crtc_vsync_end - mode->crtc_vsync_start;
			if (!vsync_wid)
				vsync_wid = 1;

			fp_v_sync_strt_wid = ((((mode->crtc_vsync_start - mode->crtc_vblank_start) & 0xfff)
					       | ((vsync_wid & 0x1f) << 16)
					       | ((mode->flags & DRM_MODE_FLAG_NVSYNC)
						  ? RADEON_CRTC_V_SYNC_POL
						  : 0)));

			fp_horz_vert_active = (((radeon_encoder->panel_yres) & 0xfff) |
					       (((radeon_encoder->panel_xres / 8) & 0x1ff) << 16));
		}
	} else {
		fp_horz_stretch |= ((xres/8-1) << 16);
		fp_vert_stretch |= ((yres-1) << 12);
	}

	RADEON_WRITE(RADEON_FP_HORZ_STRETCH,      fp_horz_stretch);
	RADEON_WRITE(RADEON_FP_VERT_STRETCH,      fp_vert_stretch);
	RADEON_WRITE(RADEON_CRTC_MORE_CNTL,       crtc_more_cntl);
	RADEON_WRITE(RADEON_FP_HORZ_VERT_ACTIVE,  fp_horz_vert_active);
	RADEON_WRITE(RADEON_FP_H_SYNC_STRT_WID,   fp_h_sync_strt_wid);
	RADEON_WRITE(RADEON_FP_V_SYNC_STRT_WID,   fp_v_sync_strt_wid);
	RADEON_WRITE(RADEON_FP_CRTC_H_TOTAL_DISP, fp_crtc_h_total_disp);
	RADEON_WRITE(RADEON_FP_CRTC_V_TOTAL_DISP, fp_crtc_v_total_disp);

}

static void radeon_legacy_lvds_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	uint32_t lvds_gen_cntl, lvds_pll_cntl, pixclks_cntl, disp_pwr_man;

	DRM_DEBUG("\n");

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		disp_pwr_man = RADEON_READ(RADEON_DISP_PWR_MAN);
		disp_pwr_man |= RADEON_AUTO_PWRUP_EN;
		RADEON_WRITE(RADEON_DISP_PWR_MAN, disp_pwr_man);
		lvds_pll_cntl |= RADEON_LVDS_PLL_EN;
		RADEON_WRITE(RADEON_LVDS_PLL_CNTL, lvds_pll_cntl);
		udelay(1000);
		lvds_pll_cntl = RADEON_READ(RADEON_LVDS_PLL_CNTL);
		lvds_pll_cntl &= ~RADEON_LVDS_PLL_RESET;
		RADEON_WRITE(RADEON_LVDS_PLL_CNTL, lvds_pll_cntl);
		lvds_gen_cntl = RADEON_READ(RADEON_LVDS_GEN_CNTL);
		lvds_gen_cntl |= (RADEON_LVDS_ON | RADEON_LVDS_BLON | RADEON_LVDS_EN);
		lvds_gen_cntl &= ~(RADEON_LVDS_DISPLAY_DIS);
		udelay(radeon_encoder->panel_pwr_delay * 1000);
		RADEON_WRITE(RADEON_LVDS_GEN_CNTL, lvds_gen_cntl);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
                pixclks_cntl = RADEON_READ_PLL(dev_priv, RADEON_PIXCLKS_CNTL);
		RADEON_WRITE_PLL_P(dev_priv, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
                lvds_gen_cntl = RADEON_READ(RADEON_LVDS_GEN_CNTL);
                lvds_gen_cntl |= RADEON_LVDS_DISPLAY_DIS;
                lvds_gen_cntl &= ~(RADEON_LVDS_ON | RADEON_LVDS_BLON | RADEON_LVDS_EN);
                RADEON_WRITE(RADEON_LVDS_GEN_CNTL, lvds_gen_cntl);
		RADEON_WRITE_PLL(dev_priv, RADEON_PIXCLKS_CNTL, pixclks_cntl);
		break;
	}
}

static void radeon_legacy_lvds_prepare(struct drm_encoder *encoder)
{
	radeon_legacy_lvds_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_legacy_lvds_commit(struct drm_encoder *encoder)
{
	radeon_legacy_lvds_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void radeon_legacy_lvds_mode_set(struct drm_encoder *encoder,
					struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(encoder->crtc);
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	uint32_t lvds_pll_cntl, lvds_gen_cntl;

	DRM_DEBUG("\n");

	if (radeon_crtc->crtc_id == 0)
		radeon_legacy_rmx_mode_set(encoder, mode, adjusted_mode);

	lvds_pll_cntl = RADEON_READ(RADEON_LVDS_PLL_CNTL);
	lvds_pll_cntl &= ~RADEON_LVDS_PLL_EN;
	lvds_gen_cntl = RADEON_READ(RADEON_LVDS_GEN_CNTL);
	lvds_gen_cntl |= RADEON_LVDS_DISPLAY_DIS;
	lvds_gen_cntl &= ~(RADEON_LVDS_ON |
			   RADEON_LVDS_BLON |
			   RADEON_LVDS_EN |
			   RADEON_LVDS_RST_FM);

	if (radeon_is_r300(dev_priv))
		lvds_pll_cntl &= ~(R300_LVDS_SRC_SEL_MASK);

	if (radeon_crtc->crtc_id == 0) {
		if (radeon_is_r300(dev_priv)) {
			if (radeon_encoder->flags & RADEON_USE_RMX)
				lvds_pll_cntl |= R300_LVDS_SRC_SEL_RMX;
		} else
			lvds_gen_cntl &= ~RADEON_LVDS_SEL_CRTC2;
	} else {
		if (radeon_is_r300(dev_priv)) {
			lvds_pll_cntl |= R300_LVDS_SRC_SEL_CRTC2;
		} else
			lvds_gen_cntl |= RADEON_LVDS_SEL_CRTC2;
	}

	RADEON_WRITE(RADEON_LVDS_GEN_CNTL, lvds_gen_cntl);
	RADEON_WRITE(RADEON_LVDS_PLL_CNTL, lvds_pll_cntl);

	if (dev_priv->chip_family == CHIP_RV410)
		RADEON_WRITE(RADEON_CLOCK_CNTL_INDEX, 0);
}

static bool radeon_legacy_lvds_mode_fixup(struct drm_encoder *encoder,
					  struct drm_display_mode *mode,
					  struct drm_display_mode *adjusted_mode)
{
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);

	radeon_encoder->flags &= ~RADEON_USE_RMX;

	if (radeon_encoder->rmx_type != RMX_OFF)
		radeon_rmx_mode_fixup(encoder, mode, adjusted_mode);

	return true;
}

static const struct drm_encoder_helper_funcs radeon_legacy_lvds_helper_funcs = {
	.dpms = radeon_legacy_lvds_dpms,
	.mode_fixup = radeon_legacy_lvds_mode_fixup,
	.prepare = radeon_legacy_lvds_prepare,
	.mode_set = radeon_legacy_lvds_mode_set,
	.commit = radeon_legacy_lvds_commit,
};


static const struct drm_encoder_funcs radeon_legacy_lvds_enc_funcs = {
	.destroy = radeon_enc_destroy,
};


struct drm_encoder *radeon_encoder_legacy_lvds_add(struct drm_device *dev, int bios_index)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder;
	struct drm_encoder *encoder;

	DRM_DEBUG("\n");

	radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	encoder->possible_crtcs = 0x3;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_legacy_lvds_enc_funcs,
			 DRM_MODE_ENCODER_LVDS);

	drm_encoder_helper_add(encoder, &radeon_legacy_lvds_helper_funcs);

	/* TODO get the LVDS info from the BIOS for panel size etc. */
	/* get the lvds info from the bios */
	radeon_combios_get_lvds_info(radeon_encoder);

	/* LVDS gets default RMX full scaling */
	radeon_encoder->rmx_type = RMX_FULL;

	return encoder;
}

static bool radeon_legacy_primary_dac_mode_fixup(struct drm_encoder *encoder,
						 struct drm_display_mode *mode,
						 struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void radeon_legacy_primary_dac_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t crtc_ext_cntl = RADEON_READ(RADEON_CRTC_EXT_CNTL);
	uint32_t dac_cntl = RADEON_READ(RADEON_DAC_CNTL);
	uint32_t dac_macro_cntl = RADEON_READ(RADEON_DAC_MACRO_CNTL);

	DRM_DEBUG("\n");

	switch(mode) {
	case DRM_MODE_DPMS_ON:
		crtc_ext_cntl |= RADEON_CRTC_CRT_ON;
		dac_cntl &= ~RADEON_DAC_PDWN;
		dac_macro_cntl &= ~(RADEON_DAC_PDWN_R |
				    RADEON_DAC_PDWN_G |
				    RADEON_DAC_PDWN_B);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		crtc_ext_cntl &= ~RADEON_CRTC_CRT_ON;
		dac_cntl |= RADEON_DAC_PDWN;
		dac_macro_cntl |= (RADEON_DAC_PDWN_R |
				   RADEON_DAC_PDWN_G |
				   RADEON_DAC_PDWN_B);
		break;
	}

	RADEON_WRITE(RADEON_CRTC_EXT_CNTL, crtc_ext_cntl);
	RADEON_WRITE(RADEON_DAC_CNTL, dac_cntl);
	RADEON_WRITE(RADEON_DAC_MACRO_CNTL, dac_macro_cntl);

}

static void radeon_legacy_primary_dac_prepare(struct drm_encoder *encoder)
{
	radeon_legacy_primary_dac_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_legacy_primary_dac_commit(struct drm_encoder *encoder)
{
	radeon_legacy_primary_dac_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void radeon_legacy_primary_dac_mode_set(struct drm_encoder *encoder,
					       struct drm_display_mode *mode,
					       struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(encoder->crtc);
	uint32_t disp_output_cntl, dac_cntl, dac2_cntl, dac_macro_cntl;

	DRM_DEBUG("\n");

	if (radeon_crtc->crtc_id == 0)
		radeon_legacy_rmx_mode_set(encoder, mode, adjusted_mode);

	if (radeon_crtc->crtc_id == 0) {
		if (dev_priv->chip_family == CHIP_R200 || radeon_is_r300(dev_priv)) {
			disp_output_cntl = RADEON_READ(RADEON_DISP_OUTPUT_CNTL) &
				~(RADEON_DISP_DAC_SOURCE_MASK);
			RADEON_WRITE(RADEON_DISP_OUTPUT_CNTL, disp_output_cntl);
		} else {
			dac2_cntl = RADEON_READ(RADEON_DAC_CNTL2)  & ~(RADEON_DAC2_DAC_CLK_SEL);
			RADEON_WRITE(RADEON_DAC_CNTL2, dac2_cntl);
		}
	} else {
		if (dev_priv->chip_family == CHIP_R200 || radeon_is_r300(dev_priv)) {
			disp_output_cntl = RADEON_READ(RADEON_DISP_OUTPUT_CNTL) &
				~(RADEON_DISP_DAC_SOURCE_MASK);
			disp_output_cntl |= RADEON_DISP_DAC_SOURCE_CRTC2;
			RADEON_WRITE(RADEON_DISP_OUTPUT_CNTL, disp_output_cntl);
		} else {
			dac2_cntl = RADEON_READ(RADEON_DAC_CNTL2) | RADEON_DAC2_DAC_CLK_SEL;
			RADEON_WRITE(RADEON_DAC_CNTL2, dac2_cntl);
		}
	}

	dac_cntl = (RADEON_DAC_MASK_ALL |
		    RADEON_DAC_VGA_ADR_EN |
		    /* TODO 6-bits */
		    RADEON_DAC_8BIT_EN);

	RADEON_WRITE_P(RADEON_DAC_CNTL,
		       dac_cntl,
		       RADEON_DAC_RANGE_CNTL |
		       RADEON_DAC_BLANKING);

	dac_macro_cntl = RADEON_READ(RADEON_DAC_MACRO_CNTL);
	RADEON_WRITE(RADEON_DAC_MACRO_CNTL, dac_macro_cntl);
}

static enum drm_connector_status radeon_legacy_primary_dac_detect(struct drm_encoder *encoder, struct drm_connector *connector)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);

	return connector_status_disconnected;

}

static const struct drm_encoder_helper_funcs radeon_legacy_primary_dac_helper_funcs = {
	.dpms = radeon_legacy_primary_dac_dpms,
	.mode_fixup = radeon_legacy_primary_dac_mode_fixup,
	.prepare = radeon_legacy_primary_dac_prepare,
	.mode_set = radeon_legacy_primary_dac_mode_set,
	.commit = radeon_legacy_primary_dac_commit,
	.detect = radeon_legacy_primary_dac_detect,
};


static const struct drm_encoder_funcs radeon_legacy_primary_dac_enc_funcs = {
	.destroy = radeon_enc_destroy,
};

struct drm_encoder *radeon_encoder_legacy_primary_dac_add(struct drm_device *dev, int bios_index, int has_tv)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder;
	struct drm_encoder *encoder;

	DRM_DEBUG("\n");

	radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	encoder->possible_crtcs = 0x3;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_legacy_primary_dac_enc_funcs,
			 DRM_MODE_ENCODER_DAC);

	drm_encoder_helper_add(encoder, &radeon_legacy_primary_dac_helper_funcs);

	/* TODO get the primary dac vals from bios tables */
	//radeon_combios_get_lvds_info(radeon_encoder);

	return encoder;
}


static bool radeon_legacy_tmds_int_mode_fixup(struct drm_encoder *encoder,
						 struct drm_display_mode *mode,
						 struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void radeon_legacy_tmds_int_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t fp_gen_cntl = RADEON_READ(RADEON_FP_GEN_CNTL);

	DRM_DEBUG("\n");

	switch(mode) {
	case DRM_MODE_DPMS_ON:
                fp_gen_cntl |= (RADEON_FP_FPON | RADEON_FP_TMDS_EN);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		fp_gen_cntl &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
		break;
	}

	RADEON_WRITE(RADEON_FP_GEN_CNTL, fp_gen_cntl);
}

static void radeon_legacy_tmds_int_prepare(struct drm_encoder *encoder)
{
	radeon_legacy_tmds_int_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_legacy_tmds_int_commit(struct drm_encoder *encoder)
{
	radeon_legacy_tmds_int_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void radeon_legacy_tmds_int_mode_set(struct drm_encoder *encoder,
					    struct drm_display_mode *mode,
					    struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(encoder->crtc);
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	uint32_t tmp, tmds_pll_cntl, tmds_transmitter_cntl, fp_gen_cntl;
	int i;

	DRM_DEBUG("\n");

	if (radeon_crtc->crtc_id == 0)
		radeon_legacy_rmx_mode_set(encoder, mode, adjusted_mode);

	tmp = tmds_pll_cntl = RADEON_READ(RADEON_TMDS_PLL_CNTL);
	tmp &= 0xfffff;
	if (dev_priv->chip_family == CHIP_RV280) {
		/* bit 22 of TMDS_PLL_CNTL is read-back inverted */
		tmp ^= (1 << 22);
		tmds_pll_cntl ^= (1 << 22);
	}

	for (i = 0; i < 4; i++) {
		if (radeon_encoder->tmds_pll[i].freq == 0)
			break;
		if ((uint32_t)(mode->clock / 10) < radeon_encoder->tmds_pll[i].freq) {
			tmp = radeon_encoder->tmds_pll[i].value ;
			break;
		}
	}

	if (radeon_is_r300(dev_priv) || (dev_priv->chip_family == CHIP_RV280)) {
		if (tmp & 0xfff00000)
			tmds_pll_cntl = tmp;
		else {
			tmds_pll_cntl &= 0xfff00000;
			tmds_pll_cntl |= tmp;
		}
	} else
		tmds_pll_cntl = tmp;

	tmds_transmitter_cntl = RADEON_READ(RADEON_TMDS_TRANSMITTER_CNTL) &
		~(RADEON_TMDS_TRANSMITTER_PLLRST);

    if (dev_priv->chip_family == CHIP_R200 ||
	dev_priv->chip_family == CHIP_R100 ||
	radeon_is_r300(dev_priv))
	    tmds_transmitter_cntl &= ~(RADEON_TMDS_TRANSMITTER_PLLEN);
    else /* RV chips got this bit reversed */
	    tmds_transmitter_cntl |= RADEON_TMDS_TRANSMITTER_PLLEN;

    fp_gen_cntl = (RADEON_READ(RADEON_FP_GEN_CNTL) |
		   (RADEON_FP_CRTC_DONT_SHADOW_VPAR |
		    RADEON_FP_CRTC_DONT_SHADOW_HEND));

    fp_gen_cntl &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);

    if (1) // FIXME rgbBits == 8
	    fp_gen_cntl |= RADEON_FP_PANEL_FORMAT;  /* 24 bit format */
    else
	    fp_gen_cntl &= ~RADEON_FP_PANEL_FORMAT;/* 18 bit format */

    if (radeon_crtc->crtc_id == 0) {
	    if (radeon_is_r300(dev_priv) || dev_priv->chip_family == CHIP_R200) {
		    fp_gen_cntl &= ~R200_FP_SOURCE_SEL_MASK;
		    if (radeon_encoder->flags & RADEON_USE_RMX)
			    fp_gen_cntl |= R200_FP_SOURCE_SEL_RMX;
		    else
			    fp_gen_cntl |= R200_FP_SOURCE_SEL_CRTC1;
	    } else
		    fp_gen_cntl |= RADEON_FP_SEL_CRTC1;
    } else {
	    if (radeon_is_r300(dev_priv) || dev_priv->chip_family == CHIP_R200) {
		    fp_gen_cntl &= ~R200_FP_SOURCE_SEL_MASK;
		    fp_gen_cntl |= R200_FP_SOURCE_SEL_CRTC2;
	    } else
		    fp_gen_cntl |= RADEON_FP_SEL_CRTC2;
    }

    RADEON_WRITE(RADEON_TMDS_PLL_CNTL, tmds_pll_cntl);
    RADEON_WRITE(RADEON_TMDS_TRANSMITTER_CNTL, tmds_transmitter_cntl);
    RADEON_WRITE(RADEON_FP_GEN_CNTL, fp_gen_cntl);
}

static const struct drm_encoder_helper_funcs radeon_legacy_tmds_int_helper_funcs = {
	.dpms = radeon_legacy_tmds_int_dpms,
	.mode_fixup = radeon_legacy_tmds_int_mode_fixup,
	.prepare = radeon_legacy_tmds_int_prepare,
	.mode_set = radeon_legacy_tmds_int_mode_set,
	.commit = radeon_legacy_tmds_int_commit,
};


static const struct drm_encoder_funcs radeon_legacy_tmds_int_enc_funcs = {
	.destroy = radeon_enc_destroy,
};

struct drm_encoder *radeon_encoder_legacy_tmds_int_add(struct drm_device *dev, int bios_index)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder;
	struct drm_encoder *encoder;

	DRM_DEBUG("\n");

	radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	encoder->possible_crtcs = 0x3;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_legacy_tmds_int_enc_funcs,
			 DRM_MODE_ENCODER_TMDS);

	drm_encoder_helper_add(encoder, &radeon_legacy_tmds_int_helper_funcs);

	radeon_combios_get_tmds_info(radeon_encoder);
	return encoder;
}

static bool radeon_legacy_tmds_ext_mode_fixup(struct drm_encoder *encoder,
					      struct drm_display_mode *mode,
					      struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void radeon_legacy_tmds_ext_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t fp2_gen_cntl = RADEON_READ(RADEON_FP2_GEN_CNTL);

	DRM_DEBUG("\n");

	switch(mode) {
	case DRM_MODE_DPMS_ON:
		fp2_gen_cntl &= ~RADEON_FP2_BLANK_EN;
		fp2_gen_cntl |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		fp2_gen_cntl |= RADEON_FP2_BLANK_EN;
		fp2_gen_cntl &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
		break;
	}

	RADEON_WRITE(RADEON_FP2_GEN_CNTL, fp2_gen_cntl);
}

static void radeon_legacy_tmds_ext_prepare(struct drm_encoder *encoder)
{
	radeon_legacy_tmds_ext_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_legacy_tmds_ext_commit(struct drm_encoder *encoder)
{
	radeon_legacy_tmds_ext_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void radeon_legacy_tmds_ext_mode_set(struct drm_encoder *encoder,
					    struct drm_display_mode *mode,
					    struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(encoder->crtc);
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	uint32_t fp2_gen_cntl = RADEON_READ(RADEON_FP2_GEN_CNTL);

	DRM_DEBUG("\n");

	if (radeon_crtc->crtc_id == 0)
		radeon_legacy_rmx_mode_set(encoder, mode, adjusted_mode);

	if (1) // FIXME rgbBits == 8
		fp2_gen_cntl |= RADEON_FP2_PANEL_FORMAT; /* 24 bit format, */
	else
		fp2_gen_cntl &= ~RADEON_FP2_PANEL_FORMAT;/* 18 bit format, */

	fp2_gen_cntl &= ~(RADEON_FP2_ON |
			  RADEON_FP2_DVO_EN |
			  RADEON_FP2_DVO_RATE_SEL_SDR);

	/* XXX: these are oem specific */
	if (radeon_is_r300(dev_priv)) {
		if ((dev->pdev->device == 0x4850) &&
		    (dev->pdev->subsystem_vendor == 0x1028) &&
		    (dev->pdev->subsystem_device == 0x2001)) /* Dell Inspiron 8600 */
			fp2_gen_cntl |= R300_FP2_DVO_CLOCK_MODE_SINGLE;
		else
			fp2_gen_cntl |= RADEON_FP2_PAD_FLOP_EN | R300_FP2_DVO_CLOCK_MODE_SINGLE;

		/*if (mode->clock > 165000)
			fp2_gen_cntl |= R300_FP2_DVO_DUAL_CHANNEL_EN;*/
	}

	if (radeon_crtc->crtc_id == 0) {
		if ((dev_priv->chip_family == CHIP_R200) || radeon_is_r300(dev_priv)) {
			fp2_gen_cntl &= ~R200_FP2_SOURCE_SEL_MASK;
			if (radeon_encoder->flags & RADEON_USE_RMX)
				fp2_gen_cntl |= R200_FP2_SOURCE_SEL_RMX;
			else
				fp2_gen_cntl |= R200_FP2_SOURCE_SEL_CRTC1;
		} else
			fp2_gen_cntl &= ~RADEON_FP2_SRC_SEL_CRTC2;
	} else {
		if ((dev_priv->chip_family == CHIP_R200) || radeon_is_r300(dev_priv)) {
			fp2_gen_cntl &= ~R200_FP2_SOURCE_SEL_MASK;
			fp2_gen_cntl |= R200_FP2_SOURCE_SEL_CRTC2;
		} else
			fp2_gen_cntl |= RADEON_FP2_SRC_SEL_CRTC2;
	}

	RADEON_WRITE(RADEON_FP2_GEN_CNTL, fp2_gen_cntl);
}

static const struct drm_encoder_helper_funcs radeon_legacy_tmds_ext_helper_funcs = {
	.dpms = radeon_legacy_tmds_ext_dpms,
	.mode_fixup = radeon_legacy_tmds_ext_mode_fixup,
	.prepare = radeon_legacy_tmds_ext_prepare,
	.mode_set = radeon_legacy_tmds_ext_mode_set,
	.commit = radeon_legacy_tmds_ext_commit,
};


static const struct drm_encoder_funcs radeon_legacy_tmds_ext_enc_funcs = {
	.destroy = radeon_enc_destroy,
};

struct drm_encoder *radeon_encoder_legacy_tmds_ext_add(struct drm_device *dev, int bios_index)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder;
	struct drm_encoder *encoder;

	DRM_DEBUG("\n");

	radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	encoder->possible_crtcs = 0x3;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_legacy_tmds_ext_enc_funcs,
			 DRM_MODE_ENCODER_TMDS);

	drm_encoder_helper_add(encoder, &radeon_legacy_tmds_ext_helper_funcs);

	//radeon_combios_get_tmds_info(radeon_encoder);
	return encoder;
}

static bool radeon_legacy_tv_dac_mode_fixup(struct drm_encoder *encoder,
					    struct drm_display_mode *mode,
					    struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void radeon_legacy_tv_dac_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t fp2_gen_cntl, crtc2_gen_cntl, tv_master_cntl, tv_dac_cntl;

	DRM_DEBUG("\n");

	if (dev_priv->chip_family == CHIP_R200)
		fp2_gen_cntl = RADEON_READ(RADEON_FP2_GEN_CNTL);
	else {
		crtc2_gen_cntl = RADEON_READ(RADEON_CRTC2_GEN_CNTL);
		// FIXME TV
		//tv_master_cntl = RADEON_READ(RADEON_TV_MASTER_CNTL);
		tv_dac_cntl = RADEON_READ(RADEON_TV_DAC_CNTL);
	}

	switch(mode) {
	case DRM_MODE_DPMS_ON:
                if (dev_priv->chip_family == CHIP_R200)
			fp2_gen_cntl |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                else {
			crtc2_gen_cntl |= RADEON_CRTC2_CRT2_ON;
			//tv_master_cntl |= RADEON_TV_ON;
			if (dev_priv->chip_family == CHIP_R420 ||
			    dev_priv->chip_family == CHIP_RV410)
				tv_dac_cntl &= ~(R420_TV_DAC_RDACPD |
						 R420_TV_DAC_GDACPD |
						 R420_TV_DAC_BDACPD |
						 RADEON_TV_DAC_BGSLEEP);
			else
				tv_dac_cntl &= ~(RADEON_TV_DAC_RDACPD |
						 RADEON_TV_DAC_GDACPD |
						 RADEON_TV_DAC_BDACPD |
						 RADEON_TV_DAC_BGSLEEP);
		}
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		if (dev_priv->chip_family == CHIP_R200)
                        fp2_gen_cntl &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
		else {
                        crtc2_gen_cntl &= ~RADEON_CRTC2_CRT2_ON;
			//tv_master_cntl &= ~RADEON_TV_ON;
			if (dev_priv->chip_family == CHIP_R420 ||
			    dev_priv->chip_family == CHIP_RV410)
				tv_dac_cntl |= (R420_TV_DAC_RDACPD |
						R420_TV_DAC_GDACPD |
						R420_TV_DAC_BDACPD |
						RADEON_TV_DAC_BGSLEEP);
			else
				tv_dac_cntl |= (RADEON_TV_DAC_RDACPD |
						RADEON_TV_DAC_GDACPD |
						RADEON_TV_DAC_BDACPD |
						RADEON_TV_DAC_BGSLEEP);
		}
		break;
	}

	if (dev_priv->chip_family == CHIP_R200)
		RADEON_WRITE(RADEON_FP2_GEN_CNTL, fp2_gen_cntl);
	else {
		RADEON_WRITE(RADEON_CRTC2_GEN_CNTL, crtc2_gen_cntl);
		//RADEON_WRITE(RADEON_TV_MASTER_CNTL, tv_master_cntl);
		RADEON_WRITE(RADEON_TV_DAC_CNTL, tv_dac_cntl);
	}

}

static void radeon_legacy_tv_dac_prepare(struct drm_encoder *encoder)
{
	radeon_legacy_tv_dac_dpms(encoder, DRM_MODE_DPMS_OFF);
}

static void radeon_legacy_tv_dac_commit(struct drm_encoder *encoder)
{
	radeon_legacy_tv_dac_dpms(encoder, DRM_MODE_DPMS_ON);
}

static void radeon_legacy_tv_dac_mode_set(struct drm_encoder *encoder,
					  struct drm_display_mode *mode,
					  struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(encoder->crtc);
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);
	uint32_t tv_dac_cntl, gpiopad_a, dac2_cntl, disp_output_cntl, fp2_gen_cntl;
	uint32_t disp_hw_debug;

	DRM_DEBUG("\n");

	if (radeon_crtc->crtc_id == 0)
		radeon_legacy_rmx_mode_set(encoder, mode, adjusted_mode);

	if (dev_priv->chip_family != CHIP_R200) {
		tv_dac_cntl = RADEON_READ(RADEON_TV_DAC_CNTL);
		if (dev_priv->chip_family == CHIP_R420 ||
		    dev_priv->chip_family == CHIP_RV410) {
			tv_dac_cntl &= ~(RADEON_TV_DAC_STD_MASK |
					 RADEON_TV_DAC_BGADJ_MASK |
					 R420_TV_DAC_DACADJ_MASK |
					 R420_TV_DAC_RDACPD |
					 R420_TV_DAC_GDACPD |
					 R420_TV_DAC_GDACPD |
					 R420_TV_DAC_TVENABLE);
		} else {
			tv_dac_cntl &= ~(RADEON_TV_DAC_STD_MASK |
					 RADEON_TV_DAC_BGADJ_MASK |
					 RADEON_TV_DAC_DACADJ_MASK |
					 RADEON_TV_DAC_RDACPD |
					 RADEON_TV_DAC_GDACPD |
					 RADEON_TV_DAC_GDACPD);
		}

		// FIXME TV
		tv_dac_cntl |= (RADEON_TV_DAC_NBLANK |
				RADEON_TV_DAC_NHOLD |
				RADEON_TV_DAC_STD_PS2 /*|
				radeon_encoder->ps2_tvdac_adj*/); // fixme, get from bios

		RADEON_WRITE(RADEON_TV_DAC_CNTL, tv_dac_cntl);
	}

	if (radeon_is_r300(dev_priv)) {
		gpiopad_a = RADEON_READ(RADEON_GPIOPAD_A) | 1;
		disp_output_cntl = RADEON_READ(RADEON_DISP_OUTPUT_CNTL);
	} else if (dev_priv->chip_family == CHIP_R200)
		fp2_gen_cntl = RADEON_READ(RADEON_FP2_GEN_CNTL);
	else
		disp_hw_debug = RADEON_READ(RADEON_DISP_HW_DEBUG);

	dac2_cntl = RADEON_READ(RADEON_DAC_CNTL2) | RADEON_DAC2_DAC2_CLK_SEL;

	if (radeon_crtc->crtc_id == 0) {
		if (radeon_is_r300(dev_priv)) {
			disp_output_cntl &= ~RADEON_DISP_TVDAC_SOURCE_MASK;
			disp_output_cntl |= RADEON_DISP_TVDAC_SOURCE_CRTC;
		} else if (dev_priv->chip_family == CHIP_R200) {
			fp2_gen_cntl &= ~(R200_FP2_SOURCE_SEL_MASK |
					  RADEON_FP2_DVO_RATE_SEL_SDR);
		} else
			disp_hw_debug |= RADEON_CRT2_DISP1_SEL;
	} else {
		if (radeon_is_r300(dev_priv)) {
			disp_output_cntl &= ~RADEON_DISP_TVDAC_SOURCE_MASK;
			disp_output_cntl |= RADEON_DISP_TVDAC_SOURCE_CRTC2;
		} else if (dev_priv->chip_family == CHIP_R200) {
			fp2_gen_cntl &= ~(R200_FP2_SOURCE_SEL_MASK |
					  RADEON_FP2_DVO_RATE_SEL_SDR);
			fp2_gen_cntl |= R200_FP2_SOURCE_SEL_CRTC2;
		} else
			disp_hw_debug &= ~RADEON_CRT2_DISP1_SEL;
	}

	RADEON_WRITE(RADEON_DAC_CNTL2, dac2_cntl);

	if (radeon_is_r300(dev_priv)) {
		RADEON_WRITE_P(RADEON_GPIOPAD_A, gpiopad_a, ~1);
		RADEON_WRITE(RADEON_DISP_TV_OUT_CNTL, disp_output_cntl);
	} else if (dev_priv->chip_family == CHIP_R200)
		RADEON_WRITE(RADEON_FP2_GEN_CNTL, fp2_gen_cntl);
	else
		RADEON_WRITE(RADEON_DISP_HW_DEBUG, disp_hw_debug);

}

static enum drm_connector_status radeon_legacy_tv_dac_detect(struct drm_encoder *encoder, struct drm_connector *connector)
{
	struct drm_device *dev = encoder->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);

	return connector_status_disconnected;

}

static const struct drm_encoder_helper_funcs radeon_legacy_tv_dac_helper_funcs = {
	.dpms = radeon_legacy_tv_dac_dpms,
	.mode_fixup = radeon_legacy_tv_dac_mode_fixup,
	.prepare = radeon_legacy_tv_dac_prepare,
	.mode_set = radeon_legacy_tv_dac_mode_set,
	.commit = radeon_legacy_tv_dac_commit,
	.detect = radeon_legacy_tv_dac_detect,
};


static const struct drm_encoder_funcs radeon_legacy_tv_dac_enc_funcs = {
	.destroy = radeon_enc_destroy,
};

struct drm_encoder *radeon_encoder_legacy_tv_dac_add(struct drm_device *dev, int bios_index, int has_tv)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct radeon_encoder *radeon_encoder;
	struct drm_encoder *encoder;

	DRM_DEBUG("\n");

	radeon_encoder = kzalloc(sizeof(struct radeon_encoder), GFP_KERNEL);
	if (!radeon_encoder) {
		return NULL;
	}

	encoder = &radeon_encoder->base;

	encoder->possible_crtcs = 0x3;
	encoder->possible_clones = 0;
	drm_encoder_init(dev, encoder, &radeon_legacy_tv_dac_enc_funcs,
			 DRM_MODE_ENCODER_DAC);

	drm_encoder_helper_add(encoder, &radeon_legacy_tv_dac_helper_funcs);

	/* TODO get the tv dac vals from bios tables */
	//radeon_combios_get_lvds_info(radeon_encoder);

	return encoder;
}
