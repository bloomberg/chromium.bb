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

#include "drm_crtc_helper.h"

void radeon_restore_common_regs(struct drm_device *dev)
{
	/* don't need this yet */
}

static void radeon_pll_wait_for_read_update_complete(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i = 0;

	/* FIXME: Certain revisions of R300 can't recover here.  Not sure of
	   the cause yet, but this workaround will mask the problem for now.
	   Other chips usually will pass at the very first test, so the
	   workaround shouldn't have any effect on them. */
	for (i = 0;
	     (i < 10000 &&
	      RADEON_READ_PLL(dev_priv, RADEON_PPLL_REF_DIV) & RADEON_PPLL_ATOMIC_UPDATE_R);
	     i++);
}

static void radeon_pll_write_update(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	while (RADEON_READ_PLL(dev_priv, RADEON_PPLL_REF_DIV) & RADEON_PPLL_ATOMIC_UPDATE_R);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_REF_DIV,
			   RADEON_PPLL_ATOMIC_UPDATE_W,
			   ~(RADEON_PPLL_ATOMIC_UPDATE_W));
}

static void radeon_pll2_wait_for_read_update_complete(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i = 0;


	/* FIXME: Certain revisions of R300 can't recover here.  Not sure of
	   the cause yet, but this workaround will mask the problem for now.
	   Other chips usually will pass at the very first test, so the
	   workaround shouldn't have any effect on them. */
	for (i = 0;
	     (i < 10000 &&
	      RADEON_READ_PLL(dev_priv, RADEON_P2PLL_REF_DIV) & RADEON_P2PLL_ATOMIC_UPDATE_R);
	     i++);
}

static void radeon_pll2_write_update(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	while (RADEON_READ_PLL(dev_priv, RADEON_P2PLL_REF_DIV) & RADEON_P2PLL_ATOMIC_UPDATE_R);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_P2PLL_REF_DIV,
			   RADEON_P2PLL_ATOMIC_UPDATE_W,
			   ~(RADEON_P2PLL_ATOMIC_UPDATE_W));
}

static uint8_t radeon_compute_pll_gain(uint16_t ref_freq, uint16_t ref_div,
				       uint16_t fb_div)
{
	unsigned int vcoFreq;

        if (!ref_div)
		return 1;

	vcoFreq = ((unsigned)ref_freq & fb_div) / ref_div;

	/*
	 * This is horribly crude: the VCO frequency range is divided into
	 * 3 parts, each part having a fixed PLL gain value.
	 */
	if (vcoFreq >= 30000)
		/*
		 * [300..max] MHz : 7
		 */
		return 7;
	else if (vcoFreq >= 18000)
		/*
		 * [180..300) MHz : 4
		 */
		return 4;
	else
		/*
		 * [0..180) MHz : 1
		 */
		return 1;
}

void radeon_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t mask;

	DRM_DEBUG("\n");

	mask = radeon_crtc->crtc_id ?
		(RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS | RADEON_CRTC2_DISP_REQ_EN_B) :
		(RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS | RADEON_CRTC_HSYNC_DIS);

	switch(mode) {
	case DRM_MODE_DPMS_ON:
		if (radeon_crtc->crtc_id)
			RADEON_WRITE_P(RADEON_CRTC2_GEN_CNTL, 0, ~mask);
		else {
			RADEON_WRITE_P(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_DISP_REQ_EN_B);
			RADEON_WRITE_P(RADEON_CRTC_EXT_CNTL, 0, ~mask);
		}
		break;
	case DRM_MODE_DPMS_STANDBY:
		if (radeon_crtc->crtc_id)
			RADEON_WRITE_P(RADEON_CRTC2_GEN_CNTL, (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_HSYNC_DIS), ~mask);
		else {
			RADEON_WRITE_P(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_DISP_REQ_EN_B);
			RADEON_WRITE_P(RADEON_CRTC_EXT_CNTL, (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS), ~mask);
		}
		break;
	case DRM_MODE_DPMS_SUSPEND:
		if (radeon_crtc->crtc_id)
			RADEON_WRITE_P(RADEON_CRTC2_GEN_CNTL, (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS), ~mask);
		else {
			RADEON_WRITE_P(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_DISP_REQ_EN_B);
			RADEON_WRITE_P(RADEON_CRTC_EXT_CNTL, (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS), ~mask);
		}
		break;
	case DRM_MODE_DPMS_OFF:
		if (radeon_crtc->crtc_id)
			RADEON_WRITE_P(RADEON_CRTC2_GEN_CNTL, mask, ~mask);
		else {
			RADEON_WRITE_P(RADEON_CRTC_GEN_CNTL, RADEON_CRTC_DISP_REQ_EN_B, ~RADEON_CRTC_DISP_REQ_EN_B);
			RADEON_WRITE_P(RADEON_CRTC_EXT_CNTL, mask, ~mask);
		}
		break;
	}

	if (mode != DRM_MODE_DPMS_OFF) {
		radeon_crtc_load_lut(crtc);
	}
}

/* properly set crtc bpp when using atombios */
void radeon_legacy_atom_set_surface(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	int format;
	uint32_t crtc_gen_cntl, crtc2_gen_cntl;

	switch (crtc->fb->bits_per_pixel) {
	case 15:      /*  555 */
		format = 3;
		break;
	case 16:      /*  565 */
		format = 4;
		break;
	case 24:      /*  RGB */
		format = 5;
		break;
	case 32:      /* xRGB */
		format = 6;
		break;
	default:
		return;
	}

	switch (radeon_crtc->crtc_id) {
	case 0:
		crtc_gen_cntl = RADEON_READ(RADEON_CRTC_GEN_CNTL) & 0xfffff0ff;
		crtc_gen_cntl |= (format << 8);
		crtc_gen_cntl |= RADEON_CRTC_EXT_DISP_EN;
		RADEON_WRITE(RADEON_CRTC_GEN_CNTL, crtc_gen_cntl);
		break;
	case 1:
		crtc2_gen_cntl = RADEON_READ(RADEON_CRTC2_GEN_CNTL) & 0xfffff0ff;
		crtc2_gen_cntl |= (format << 8);
		RADEON_WRITE(RADEON_CRTC2_GEN_CNTL, crtc2_gen_cntl);
		// not sure we need these...
		RADEON_WRITE(RADEON_FP_H2_SYNC_STRT_WID,   RADEON_READ(RADEON_CRTC2_H_SYNC_STRT_WID));
		RADEON_WRITE(RADEON_FP_V2_SYNC_STRT_WID,   RADEON_READ(RADEON_CRTC2_V_SYNC_STRT_WID));
		break;
	}
}

static bool radeon_set_crtc1_base(struct drm_crtc *crtc, int x, int y)
{
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_framebuffer *radeon_fb;
	struct drm_gem_object *obj;
	struct drm_radeon_gem_object *obj_priv;
	uint32_t base;
	uint32_t crtc_offset, crtc_offset_cntl, crtc_tile_x0_y0 = 0;
	uint32_t crtc_pitch;
	uint32_t disp_merge_cntl;

	DRM_DEBUG("\n");

	radeon_fb = to_radeon_framebuffer(crtc->fb);

	obj = radeon_fb->obj;
	obj_priv = obj->driver_private;

	crtc_offset = obj_priv->bo->offset;

	crtc_offset_cntl = 0;

	/* TODO tiling */
	if (0) {
		if (radeon_is_r300(dev_priv))
			crtc_offset_cntl |= (R300_CRTC_X_Y_MODE_EN |
					     R300_CRTC_MICRO_TILE_BUFFER_DIS |
					     R300_CRTC_MACRO_TILE_EN);
		else
			crtc_offset_cntl |= RADEON_CRTC_TILE_EN;
	} else {
		if (radeon_is_r300(dev_priv))
			crtc_offset_cntl &= ~(R300_CRTC_X_Y_MODE_EN |
					      R300_CRTC_MICRO_TILE_BUFFER_DIS |
					      R300_CRTC_MACRO_TILE_EN);
		else
			crtc_offset_cntl &= ~RADEON_CRTC_TILE_EN;
	}

	base = obj_priv->bo->offset;

	/* TODO more tiling */
	if (0) {
		if (radeon_is_r300(dev_priv)) {
			crtc_tile_x0_y0 = x | (y << 16);
			base &= ~0x7ff;
		} else {
			int byteshift = crtc->fb->bits_per_pixel >> 4;
			int tile_addr = (((y >> 3) * crtc->fb->width + x) >> (8 - byteshift)) << 11;
			base += tile_addr + ((x << byteshift) % 256) + ((y % 8) << 8);
			crtc_offset_cntl |= (y % 16);
		}
	} else {
		int offset = y * crtc->fb->pitch + x;
		switch (crtc->fb->bits_per_pixel) {
		case 15:
		case 16:
			offset *= 2;
			break;
		case 24:
			offset *= 3;
			break;
		case 32:
			offset *= 4;
			break;
		default:
			return false;
		}
		base += offset;
	}

	base &= ~7;

	/* update sarea TODO */

	crtc_offset = base;

	crtc_pitch  = ((((crtc->fb->pitch / (crtc->fb->bits_per_pixel / 8)) * crtc->fb->bits_per_pixel) +
			((crtc->fb->bits_per_pixel * 8) - 1)) /
		       (crtc->fb->bits_per_pixel * 8));
	crtc_pitch |= crtc_pitch << 16;

	DRM_DEBUG("mc_fb_location: 0x%x\n", dev_priv->fb_location);

	RADEON_WRITE(RADEON_DISPLAY_BASE_ADDR, dev_priv->fb_location);

	if (radeon_is_r300(dev_priv))
		RADEON_WRITE(R300_CRTC_TILE_X0_Y0, crtc_tile_x0_y0);
	RADEON_WRITE(RADEON_CRTC_OFFSET_CNTL, crtc_offset_cntl);
	RADEON_WRITE(RADEON_CRTC_OFFSET, crtc_offset);
	RADEON_WRITE(RADEON_CRTC_PITCH, crtc_pitch);

	disp_merge_cntl = RADEON_READ(RADEON_DISP_MERGE_CNTL);
	disp_merge_cntl &= ~RADEON_DISP_RGB_OFFSET_EN;
	RADEON_WRITE(RADEON_DISP_MERGE_CNTL, disp_merge_cntl);

	return true;
}

static bool radeon_set_crtc1_timing(struct drm_crtc *crtc, struct drm_display_mode *mode)
{
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int format;
	int hsync_start;
	int hsync_wid;
	int vsync_wid;
	uint32_t crtc_gen_cntl;
	uint32_t crtc_ext_cntl;
	uint32_t crtc_h_total_disp;
	uint32_t crtc_h_sync_strt_wid;
	uint32_t crtc_v_total_disp;
	uint32_t crtc_v_sync_strt_wid;

	DRM_DEBUG("\n");

	switch (crtc->fb->bits_per_pixel) {
	case 15:      /*  555 */
		format = 3;
		break;
	case 16:      /*  565 */
		format = 4;
		break;
	case 24:      /*  RGB */
		format = 5;
		break;
	case 32:      /* xRGB */
		format = 6;
		break;
	default:
		return false;
	}

	crtc_gen_cntl = (RADEON_CRTC_EXT_DISP_EN
			 | RADEON_CRTC_EN
			 | (format << 8)
			 | ((mode->flags & DRM_MODE_FLAG_DBLSCAN)
			    ? RADEON_CRTC_DBL_SCAN_EN
			    : 0)
			 | ((mode->flags & DRM_MODE_FLAG_CSYNC)
			    ? RADEON_CRTC_CSYNC_EN
			    : 0)
			 | ((mode->flags & DRM_MODE_FLAG_INTERLACE)
			    ? RADEON_CRTC_INTERLACE_EN
			    : 0));

	crtc_ext_cntl = RADEON_READ(RADEON_CRTC_EXT_CNTL);
	crtc_ext_cntl |= (RADEON_XCRT_CNT_EN |
			  RADEON_CRTC_VSYNC_DIS |
			  RADEON_CRTC_HSYNC_DIS |
			  RADEON_CRTC_DISPLAY_DIS);

	crtc_h_total_disp = ((((mode->crtc_htotal / 8) - 1) & 0x3ff)
			     | ((((mode->crtc_hdisplay / 8) - 1) & 0x1ff) << 16));

	hsync_wid = (mode->crtc_hsync_end - mode->crtc_hsync_start) / 8;
	if (!hsync_wid)
		hsync_wid = 1;
	hsync_start = mode->crtc_hsync_start - 8;

	crtc_h_sync_strt_wid = ((hsync_start & 0x1fff)
				| ((hsync_wid & 0x3f) << 16)
				| ((mode->flags & DRM_MODE_FLAG_NHSYNC)
				   ? RADEON_CRTC_H_SYNC_POL
				   : 0));

	/* This works for double scan mode. */
	crtc_v_total_disp = (((mode->crtc_vtotal - 1) & 0xffff)
			     | ((mode->crtc_vdisplay - 1) << 16));

	vsync_wid = mode->crtc_vsync_end - mode->crtc_vsync_start;
	if (!vsync_wid)
		vsync_wid = 1;

	crtc_v_sync_strt_wid = (((mode->crtc_vsync_start - 1) & 0xfff)
				| ((vsync_wid & 0x1f) << 16)
				| ((mode->flags & DRM_MODE_FLAG_NVSYNC)
				   ? RADEON_CRTC_V_SYNC_POL
				   : 0));

	/* TODO -> Dell Server */
	if (0) {
		uint32_t disp_hw_debug = RADEON_READ(RADEON_DISP_HW_DEBUG);
		uint32_t tv_dac_cntl = RADEON_READ(RADEON_TV_DAC_CNTL);
		uint32_t dac2_cntl = RADEON_READ(RADEON_DAC_CNTL2);
		uint32_t crtc2_gen_cntl = RADEON_READ(RADEON_CRTC2_GEN_CNTL);

		dac2_cntl &= ~RADEON_DAC2_DAC_CLK_SEL;
		dac2_cntl |= RADEON_DAC2_DAC2_CLK_SEL;

		/* For CRT on DAC2, don't turn it on if BIOS didn't
		   enable it, even it's detected.
		*/
		disp_hw_debug |= RADEON_CRT2_DISP1_SEL;
		tv_dac_cntl &= ~((1<<2) | (3<<8) | (7<<24) | (0xff<<16));
		tv_dac_cntl |= (0x03 | (2<<8) | (0x58<<16));

		RADEON_WRITE(RADEON_TV_DAC_CNTL, tv_dac_cntl);
		RADEON_WRITE(RADEON_DISP_HW_DEBUG, disp_hw_debug);
		RADEON_WRITE(RADEON_DAC_CNTL2, dac2_cntl);
		RADEON_WRITE(RADEON_CRTC2_GEN_CNTL, crtc2_gen_cntl);
	}

	RADEON_WRITE(RADEON_CRTC_GEN_CNTL, crtc_gen_cntl |
		     RADEON_CRTC_DISP_REQ_EN_B);

	RADEON_WRITE_P(RADEON_CRTC_EXT_CNTL, crtc_ext_cntl,
		       RADEON_CRTC_VSYNC_DIS | RADEON_CRTC_HSYNC_DIS | RADEON_CRTC_DISPLAY_DIS);

	RADEON_WRITE(RADEON_CRTC_H_TOTAL_DISP, crtc_h_total_disp);
	RADEON_WRITE(RADEON_CRTC_H_SYNC_STRT_WID, crtc_h_sync_strt_wid);
	RADEON_WRITE(RADEON_CRTC_V_TOTAL_DISP, crtc_v_total_disp);
	RADEON_WRITE(RADEON_CRTC_V_SYNC_STRT_WID, crtc_v_sync_strt_wid);

	RADEON_WRITE(RADEON_CRTC_GEN_CNTL, crtc_gen_cntl);

	return true;
}

static void radeon_set_pll1(struct drm_crtc *crtc, struct drm_display_mode *mode)
{
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_encoder *encoder;
	uint32_t feedback_div = 0;
	uint32_t reference_div = 0;
	uint32_t post_divider = 0;
	uint32_t freq = 0;
	uint8_t pll_gain;
	int pll_flags = RADEON_PLL_LEGACY;
	bool use_bios_divs = false;
	/* PLL registers */
	uint32_t ppll_ref_div = 0;
        uint32_t ppll_div_3 = 0;
        uint32_t htotal_cntl = 0;
        uint32_t vclk_ecp_cntl;

	struct radeon_pll *pll = &dev_priv->mode_info.p1pll;

	struct {
		int divider;
		int bitvalue;
	} *post_div, post_divs[]   = {
		/* From RAGE 128 VR/RAGE 128 GL Register
		 * Reference Manual (Technical Reference
		 * Manual P/N RRG-G04100-C Rev. 0.04), page
		 * 3-17 (PLL_DIV_[3:0]).
		 */
		{  1, 0 },              /* VCLK_SRC                 */
		{  2, 1 },              /* VCLK_SRC/2               */
		{  4, 2 },              /* VCLK_SRC/4               */
		{  8, 3 },              /* VCLK_SRC/8               */
		{  3, 4 },              /* VCLK_SRC/3               */
		{ 16, 5 },              /* VCLK_SRC/16              */
		{  6, 6 },              /* VCLK_SRC/6               */
		{ 12, 7 },              /* VCLK_SRC/12              */
		{  0, 0 }
	};

	if (mode->clock > 200000) /* range limits??? */
		pll_flags |= RADEON_PLL_PREFER_HIGH_FB_DIV;
	else
		pll_flags |= RADEON_PLL_PREFER_LOW_REF_DIV;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc == crtc) {
			if (encoder->encoder_type != DRM_MODE_ENCODER_DAC)
				pll_flags |= RADEON_PLL_NO_ODD_POST_DIV;
			if (encoder->encoder_type == DRM_MODE_ENCODER_LVDS) {
				struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);

				if (radeon_encoder->use_bios_dividers) {
					ppll_ref_div = radeon_encoder->panel_ref_divider;
					ppll_div_3   = (radeon_encoder->panel_fb_divider |
							(radeon_encoder->panel_post_divider << 16));
					htotal_cntl  = 0;
					use_bios_divs = true;
				} else
					pll_flags |= RADEON_PLL_USE_REF_DIV;
			}
		}
	}

	DRM_DEBUG("\n");

	if (!use_bios_divs) {
		radeon_compute_pll(pll, mode->clock, &freq, &feedback_div, &reference_div, &post_divider, pll_flags);

		for (post_div = &post_divs[0]; post_div->divider; ++post_div) {
			if (post_div->divider == post_divider)
				break;
		}

		if (!post_div->divider) {
			post_div = &post_divs[0];
		}

		DRM_DEBUG("dc=%u, fd=%d, rd=%d, pd=%d\n",
			  (unsigned)freq,
			  feedback_div,
			  reference_div,
			  post_divider);

		ppll_ref_div   = reference_div;
#if defined(__powerpc__) && (0) /* TODO */
		/* apparently programming this otherwise causes a hang??? */
		if (info->MacModel == RADEON_MAC_IBOOK)
			state->ppll_div_3 = 0x000600ad;
		else
#endif
			ppll_div_3     = (feedback_div | (post_div->bitvalue << 16));
		htotal_cntl    = mode->htotal & 0x7;

	}

	vclk_ecp_cntl = (RADEON_READ_PLL(dev_priv, RADEON_VCLK_ECP_CNTL) &
			 ~RADEON_VCLK_SRC_SEL_MASK) | RADEON_VCLK_SRC_SEL_PPLLCLK;

	pll_gain = radeon_compute_pll_gain(dev_priv->mode_info.p1pll.reference_freq,
					   ppll_ref_div & RADEON_PPLL_REF_DIV_MASK,
					   ppll_div_3 & RADEON_PPLL_FB3_DIV_MASK);

	if (dev_priv->flags & RADEON_IS_MOBILITY) {
		/* A temporal workaround for the occational blanking on certain laptop panels.
		   This appears to related to the PLL divider registers (fail to lock?).
		   It occurs even when all dividers are the same with their old settings.
		   In this case we really don't need to fiddle with PLL registers.
		   By doing this we can avoid the blanking problem with some panels.
		*/
		if ((ppll_ref_div == (RADEON_READ_PLL(dev_priv, RADEON_PPLL_REF_DIV) & RADEON_PPLL_REF_DIV_MASK)) &&
		    (ppll_div_3 == (RADEON_READ_PLL(dev_priv, RADEON_PPLL_DIV_3) &
					   (RADEON_PPLL_POST3_DIV_MASK | RADEON_PPLL_FB3_DIV_MASK)))) {
			RADEON_WRITE_P(RADEON_CLOCK_CNTL_INDEX,
				       RADEON_PLL_DIV_SEL,
				       ~(RADEON_PLL_DIV_SEL));
			radeon_pll_errata_after_index(dev_priv);
			return;
		}
	}

	RADEON_WRITE_PLL_P(dev_priv, RADEON_VCLK_ECP_CNTL,
			   RADEON_VCLK_SRC_SEL_CPUCLK,
			   ~(RADEON_VCLK_SRC_SEL_MASK));
	RADEON_WRITE_PLL_P(dev_priv,
			   RADEON_PPLL_CNTL,
			   RADEON_PPLL_RESET
			   | RADEON_PPLL_ATOMIC_UPDATE_EN
			   | RADEON_PPLL_VGA_ATOMIC_UPDATE_EN
			   | ((uint32_t)pll_gain << RADEON_PPLL_PVG_SHIFT),
			   ~(RADEON_PPLL_RESET
			     | RADEON_PPLL_ATOMIC_UPDATE_EN
			     | RADEON_PPLL_VGA_ATOMIC_UPDATE_EN
			     | RADEON_PPLL_PVG_MASK));

	RADEON_WRITE_P(RADEON_CLOCK_CNTL_INDEX,
		       RADEON_PLL_DIV_SEL,
		       ~(RADEON_PLL_DIV_SEL));
	radeon_pll_errata_after_index(dev_priv);

	if (radeon_is_r300(dev_priv) ||
	    (dev_priv->chip_family == CHIP_RS300) ||
	    (dev_priv->chip_family == CHIP_RS400) ||
	    (dev_priv->chip_family == CHIP_RS480)) {
		if (ppll_ref_div & R300_PPLL_REF_DIV_ACC_MASK) {
			/* When restoring console mode, use saved PPLL_REF_DIV
			 * setting.
			 */
			RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_REF_DIV,
					   ppll_ref_div,
					   0);
		} else {
			/* R300 uses ref_div_acc field as real ref divider */
			RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_REF_DIV,
					   (ppll_ref_div << R300_PPLL_REF_DIV_ACC_SHIFT),
					   ~R300_PPLL_REF_DIV_ACC_MASK);
		}
	} else {
		RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_REF_DIV,
				   ppll_ref_div,
				   ~RADEON_PPLL_REF_DIV_MASK);
	}

	RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_DIV_3,
			   ppll_div_3,
			   ~RADEON_PPLL_FB3_DIV_MASK);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_DIV_3,
			   ppll_div_3,
			   ~RADEON_PPLL_POST3_DIV_MASK);

	radeon_pll_write_update(dev);
	radeon_pll_wait_for_read_update_complete(dev);

	RADEON_WRITE_PLL(dev_priv, RADEON_HTOTAL_CNTL, htotal_cntl);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_CNTL,
			   0,
			   ~(RADEON_PPLL_RESET
			     | RADEON_PPLL_SLEEP
			     | RADEON_PPLL_ATOMIC_UPDATE_EN
			     | RADEON_PPLL_VGA_ATOMIC_UPDATE_EN));

	DRM_DEBUG("Wrote: 0x%08x 0x%08x 0x%08x (0x%08x)\n",
		  ppll_ref_div,
		  ppll_div_3,
		  (unsigned)htotal_cntl,
		  RADEON_READ_PLL(dev_priv, RADEON_PPLL_CNTL));
	DRM_DEBUG("Wrote: rd=%d, fd=%d, pd=%d\n",
		  ppll_ref_div & RADEON_PPLL_REF_DIV_MASK,
		  ppll_div_3 & RADEON_PPLL_FB3_DIV_MASK,
		  (ppll_div_3 & RADEON_PPLL_POST3_DIV_MASK) >> 16);

	mdelay(50); /* Let the clock to lock */

	RADEON_WRITE_PLL_P(dev_priv, RADEON_VCLK_ECP_CNTL,
			   RADEON_VCLK_SRC_SEL_PPLLCLK,
			   ~(RADEON_VCLK_SRC_SEL_MASK));

	/*RADEON_WRITE_PLL(dev_priv, RADEON_VCLK_ECP_CNTL, vclk_ecp_cntl);*/

}

static bool radeon_set_crtc2_base(struct drm_crtc *crtc, int x, int y)
{
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_framebuffer *radeon_fb;
	struct drm_gem_object *obj;
	struct drm_radeon_gem_object *obj_priv;
	uint32_t base;
	uint32_t crtc2_offset, crtc2_offset_cntl, crtc2_tile_x0_y0 = 0;
        uint32_t crtc2_pitch;
	uint32_t disp2_merge_cntl;

	DRM_DEBUG("\n");

	radeon_fb = to_radeon_framebuffer(crtc->fb);

	obj = radeon_fb->obj;
	obj_priv = obj->driver_private;

	crtc2_offset = obj_priv->bo->offset;

	crtc2_offset_cntl = 0;

	/* TODO tiling */
	if (0) {
		if (radeon_is_r300(dev_priv))
			crtc2_offset_cntl |= (R300_CRTC_X_Y_MODE_EN |
					      R300_CRTC_MICRO_TILE_BUFFER_DIS |
					      R300_CRTC_MACRO_TILE_EN);
		else
			crtc2_offset_cntl |= RADEON_CRTC_TILE_EN;
	} else {
		if (radeon_is_r300(dev_priv))
			crtc2_offset_cntl &= ~(R300_CRTC_X_Y_MODE_EN |
					       R300_CRTC_MICRO_TILE_BUFFER_DIS |
					       R300_CRTC_MACRO_TILE_EN);
		else
			crtc2_offset_cntl &= ~RADEON_CRTC_TILE_EN;
	}

	base = obj_priv->bo->offset;

	/* TODO more tiling */
	if (0) {
		if (radeon_is_r300(dev_priv)) {
			crtc2_tile_x0_y0 = x | (y << 16);
			base &= ~0x7ff;
		} else {
			int byteshift = crtc->fb->bits_per_pixel >> 4;
			int tile_addr = (((y >> 3) * crtc->fb->width + x) >> (8 - byteshift)) << 11;
			base += tile_addr + ((x << byteshift) % 256) + ((y % 8) << 8);
			crtc2_offset_cntl |= (y % 16);
		}
	} else {
		int offset = y * crtc->fb->pitch + x;
		switch (crtc->fb->bits_per_pixel) {
		case 15:
		case 16:
			offset *= 2;
			break;
		case 24:
			offset *= 3;
			break;
		case 32:
			offset *= 4;
			break;
		default:
			return false;
		}
		base += offset;
	}

	base &= ~7;

	/* update sarea TODO */

	crtc2_offset = base;

	crtc2_pitch  = ((((crtc->fb->pitch / (crtc->fb->bits_per_pixel / 8)) * crtc->fb->bits_per_pixel) +
			((crtc->fb->bits_per_pixel * 8) - 1)) /
		       (crtc->fb->bits_per_pixel * 8));
	crtc2_pitch |= crtc2_pitch << 16;

	RADEON_WRITE(RADEON_DISPLAY2_BASE_ADDR, dev_priv->fb_location);

	if (radeon_is_r300(dev_priv))
		RADEON_WRITE(R300_CRTC2_TILE_X0_Y0, crtc2_tile_x0_y0);
	RADEON_WRITE(RADEON_CRTC2_OFFSET_CNTL, crtc2_offset_cntl);
	RADEON_WRITE(RADEON_CRTC2_OFFSET, crtc2_offset);
	RADEON_WRITE(RADEON_CRTC2_PITCH, crtc2_pitch);

	disp2_merge_cntl = RADEON_READ(RADEON_DISP2_MERGE_CNTL);
	disp2_merge_cntl &= ~RADEON_DISP2_RGB_OFFSET_EN;
	RADEON_WRITE(RADEON_DISP2_MERGE_CNTL,      disp2_merge_cntl);

	return true;
}

static bool radeon_set_crtc2_timing(struct drm_crtc *crtc, struct drm_display_mode *mode)
{
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int format;
	int hsync_start;
	int hsync_wid;
	int vsync_wid;
	uint32_t crtc2_gen_cntl;
	uint32_t crtc2_h_total_disp;
        uint32_t crtc2_h_sync_strt_wid;
        uint32_t crtc2_v_total_disp;
        uint32_t crtc2_v_sync_strt_wid;
	uint32_t fp_h2_sync_strt_wid;
	uint32_t fp_v2_sync_strt_wid;

	DRM_DEBUG("\n");

	switch (crtc->fb->bits_per_pixel) {
		
	case 15:      /*  555 */
		format = 3;
		break;
	case 16:      /*  565 */
		format = 4;
		break;
	case 24:      /*  RGB */
		format = 5;
		break;
	case 32:      /* xRGB */
		format = 6;
		break;
	default:
		return false;
	}

	crtc2_h_total_disp =
		((((mode->crtc_htotal / 8) - 1) & 0x3ff)
		 | ((((mode->crtc_hdisplay / 8) - 1) & 0x1ff) << 16));

	hsync_wid = (mode->crtc_hsync_end - mode->crtc_hsync_start) / 8;
	if (!hsync_wid)
		hsync_wid = 1;
	hsync_start = mode->crtc_hsync_start - 8;

	crtc2_h_sync_strt_wid = ((hsync_start & 0x1fff)
				 | ((hsync_wid & 0x3f) << 16)
				 | ((mode->flags & DRM_MODE_FLAG_NHSYNC)
				    ? RADEON_CRTC_H_SYNC_POL
				    : 0));

	/* This works for double scan mode. */
	crtc2_v_total_disp = (((mode->crtc_vtotal - 1) & 0xffff)
			      | ((mode->crtc_vdisplay - 1) << 16));

	vsync_wid = mode->crtc_vsync_end - mode->crtc_vsync_start;
	if (!vsync_wid)
		vsync_wid = 1;

	crtc2_v_sync_strt_wid = (((mode->crtc_vsync_start - 1) & 0xfff)
				 | ((vsync_wid & 0x1f) << 16)
				 | ((mode->flags & DRM_MODE_FLAG_NVSYNC)
				    ? RADEON_CRTC2_V_SYNC_POL
				    : 0));

	/* check to see if TV DAC is enabled for another crtc and keep it enabled */
	if (RADEON_READ(RADEON_CRTC2_GEN_CNTL) & RADEON_CRTC2_CRT2_ON)
		crtc2_gen_cntl = RADEON_CRTC2_CRT2_ON;
	else
		crtc2_gen_cntl = 0;

	crtc2_gen_cntl |= (RADEON_CRTC2_EN
			   | (format << 8)
			   | RADEON_CRTC2_VSYNC_DIS
			   | RADEON_CRTC2_HSYNC_DIS
			   | RADEON_CRTC2_DISP_DIS
			   | ((mode->flags & DRM_MODE_FLAG_DBLSCAN)
			      ? RADEON_CRTC2_DBL_SCAN_EN
			      : 0)
			   | ((mode->flags & DRM_MODE_FLAG_CSYNC)
			      ? RADEON_CRTC2_CSYNC_EN
			      : 0)
			   | ((mode->flags & DRM_MODE_FLAG_INTERLACE)
			      ? RADEON_CRTC2_INTERLACE_EN
			      : 0));

	fp_h2_sync_strt_wid = crtc2_h_sync_strt_wid;
	fp_v2_sync_strt_wid = crtc2_v_sync_strt_wid;

	RADEON_WRITE(RADEON_CRTC2_GEN_CNTL,
		     crtc2_gen_cntl | RADEON_CRTC2_VSYNC_DIS |
		     RADEON_CRTC2_HSYNC_DIS | RADEON_CRTC2_DISP_DIS |
		     RADEON_CRTC2_DISP_REQ_EN_B);

	RADEON_WRITE(RADEON_CRTC2_H_TOTAL_DISP,    crtc2_h_total_disp);
	RADEON_WRITE(RADEON_CRTC2_H_SYNC_STRT_WID, crtc2_h_sync_strt_wid);
	RADEON_WRITE(RADEON_CRTC2_V_TOTAL_DISP,    crtc2_v_total_disp);
	RADEON_WRITE(RADEON_CRTC2_V_SYNC_STRT_WID, crtc2_v_sync_strt_wid);

	RADEON_WRITE(RADEON_FP_H2_SYNC_STRT_WID,   fp_h2_sync_strt_wid);
	RADEON_WRITE(RADEON_FP_V2_SYNC_STRT_WID,   fp_v2_sync_strt_wid);

	RADEON_WRITE(RADEON_CRTC2_GEN_CNTL,        crtc2_gen_cntl);

	return true;

}

static void radeon_set_pll2(struct drm_crtc *crtc, struct drm_display_mode *mode)
{
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_encoder *encoder;
	uint32_t feedback_div = 0;
	uint32_t reference_div = 0;
	uint32_t post_divider = 0;
	uint32_t freq = 0;
	uint8_t pll_gain;
	int pll_flags = RADEON_PLL_LEGACY;
	bool use_bios_divs = false;
	/* PLL2 registers */
	uint32_t p2pll_ref_div = 0;
	uint32_t p2pll_div_0 = 0;
	uint32_t htotal_cntl2 = 0;
	uint32_t pixclks_cntl;

	struct radeon_pll *pll = &dev_priv->mode_info.p2pll;

	struct {
		int divider;
		int bitvalue;
	} *post_div, post_divs[]   = {
		/* From RAGE 128 VR/RAGE 128 GL Register
		 * Reference Manual (Technical Reference
		 * Manual P/N RRG-G04100-C Rev. 0.04), page
		 * 3-17 (PLL_DIV_[3:0]).
		 */
		{  1, 0 },              /* VCLK_SRC                 */
		{  2, 1 },              /* VCLK_SRC/2               */
		{  4, 2 },              /* VCLK_SRC/4               */
		{  8, 3 },              /* VCLK_SRC/8               */
		{  3, 4 },              /* VCLK_SRC/3               */
		{  6, 6 },              /* VCLK_SRC/6               */
		{ 12, 7 },              /* VCLK_SRC/12              */
		{  0, 0 }
	};

	if (mode->clock > 200000) /* range limits??? */
		pll_flags |= RADEON_PLL_PREFER_HIGH_FB_DIV;
	else
		pll_flags |= RADEON_PLL_PREFER_LOW_REF_DIV;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc == crtc) {
			if (encoder->encoder_type != DRM_MODE_ENCODER_DAC)
				pll_flags |= RADEON_PLL_NO_ODD_POST_DIV;
			if (encoder->encoder_type == DRM_MODE_ENCODER_LVDS) {
				struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);

				if (radeon_encoder->use_bios_dividers) {
					p2pll_ref_div = radeon_encoder->panel_ref_divider;
					p2pll_div_0   = (radeon_encoder->panel_fb_divider |
							(radeon_encoder->panel_post_divider << 16));
					htotal_cntl2  = 0;
					use_bios_divs = true;
				} else
					pll_flags |= RADEON_PLL_USE_REF_DIV;
			}
		}
	}

	DRM_DEBUG("\n");

	if (!use_bios_divs) {
		radeon_compute_pll(pll, mode->clock, &freq, &feedback_div, &reference_div, &post_divider, pll_flags);

		for (post_div = &post_divs[0]; post_div->divider; ++post_div) {
			if (post_div->divider == post_divider)
				break;
		}

		if (!post_div->divider) {
			post_div = &post_divs[0];
		}

		DRM_DEBUG("dc=%u, fd=%d, rd=%d, pd=%d\n",
			  (unsigned)freq,
			  feedback_div,
			  reference_div,
			  post_divider);

		p2pll_ref_div    = reference_div;
		p2pll_div_0      = (feedback_div | (post_div->bitvalue << 16));
		htotal_cntl2     = mode->htotal & 0x7;

	}

	pixclks_cntl     = ((RADEON_READ_PLL(dev_priv, RADEON_PIXCLKS_CNTL) &
			     ~(RADEON_PIX2CLK_SRC_SEL_MASK)) |
			    RADEON_PIX2CLK_SRC_SEL_P2PLLCLK);

	pll_gain = radeon_compute_pll_gain(dev_priv->mode_info.p2pll.reference_freq,
					   p2pll_ref_div & RADEON_P2PLL_REF_DIV_MASK,
					   p2pll_div_0 & RADEON_P2PLL_FB0_DIV_MASK);


	RADEON_WRITE_PLL_P(dev_priv, RADEON_PIXCLKS_CNTL,
			   RADEON_PIX2CLK_SRC_SEL_CPUCLK,
			   ~(RADEON_PIX2CLK_SRC_SEL_MASK));

	RADEON_WRITE_PLL_P(dev_priv,
			   RADEON_P2PLL_CNTL,
			   RADEON_P2PLL_RESET
			   | RADEON_P2PLL_ATOMIC_UPDATE_EN
			   | ((uint32_t)pll_gain << RADEON_P2PLL_PVG_SHIFT),
			   ~(RADEON_P2PLL_RESET
			     | RADEON_P2PLL_ATOMIC_UPDATE_EN
			     | RADEON_P2PLL_PVG_MASK));


	RADEON_WRITE_PLL_P(dev_priv, RADEON_P2PLL_REF_DIV,
			   p2pll_ref_div,
			   ~RADEON_P2PLL_REF_DIV_MASK);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_P2PLL_DIV_0,
			   p2pll_div_0,
			   ~RADEON_P2PLL_FB0_DIV_MASK);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_P2PLL_DIV_0,
			   p2pll_div_0,
			   ~RADEON_P2PLL_POST0_DIV_MASK);

	radeon_pll2_write_update(dev);
	radeon_pll2_wait_for_read_update_complete(dev);

	RADEON_WRITE_PLL(dev_priv, RADEON_HTOTAL2_CNTL, htotal_cntl2);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_P2PLL_CNTL,
			   0,
			   ~(RADEON_P2PLL_RESET
			     | RADEON_P2PLL_SLEEP
			     | RADEON_P2PLL_ATOMIC_UPDATE_EN));

	DRM_DEBUG("Wrote2: 0x%08x 0x%08x 0x%08x (0x%08x)\n",
		  (unsigned)p2pll_ref_div,
		  (unsigned)p2pll_div_0,
		  (unsigned)htotal_cntl2,
		  RADEON_READ_PLL(dev_priv, RADEON_P2PLL_CNTL));
	DRM_DEBUG("Wrote2: rd=%u, fd=%u, pd=%u\n",
		  (unsigned)p2pll_ref_div & RADEON_P2PLL_REF_DIV_MASK,
		  (unsigned)p2pll_div_0 & RADEON_P2PLL_FB0_DIV_MASK,
		  (unsigned)((p2pll_div_0 &
			      RADEON_P2PLL_POST0_DIV_MASK) >>16));

	mdelay(50); /* Let the clock to lock */

	RADEON_WRITE_PLL_P(dev_priv, RADEON_PIXCLKS_CNTL,
			   RADEON_PIX2CLK_SRC_SEL_P2PLLCLK,
			   ~(RADEON_PIX2CLK_SRC_SEL_MASK));

	RADEON_WRITE_PLL(dev_priv, RADEON_PIXCLKS_CNTL, pixclks_cntl);

}

static bool radeon_crtc_mode_fixup(struct drm_crtc *crtc,
				   struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	return true;
}

void radeon_crtc_set_base(struct drm_crtc *crtc, int x, int y)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);

	switch(radeon_crtc->crtc_id) {
	case 0:
		radeon_set_crtc1_base(crtc, x, y);
		break;
	case 1:
		radeon_set_crtc2_base(crtc, x, y);
		break;

	}
}

static void radeon_crtc_mode_set(struct drm_crtc *crtc,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode,
				 int x, int y)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);

	DRM_DEBUG("\n");

	/* TODO TV */

	radeon_crtc_set_base(crtc, x, y);

	switch(radeon_crtc->crtc_id) {
	case 0:
		radeon_set_crtc1_timing(crtc, adjusted_mode);
		radeon_set_pll1(crtc, adjusted_mode);
		break;
	case 1:
		radeon_set_crtc2_timing(crtc, adjusted_mode);
		radeon_set_pll2(crtc, adjusted_mode);
		break;

	}
}

static void radeon_crtc_prepare(struct drm_crtc *crtc)
{
	radeon_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);
}

static void radeon_crtc_commit(struct drm_crtc *crtc)
{
	radeon_crtc_dpms(crtc, DRM_MODE_DPMS_ON);
}

static const struct drm_crtc_helper_funcs legacy_helper_funcs = {
	.dpms = radeon_crtc_dpms,
	.mode_fixup = radeon_crtc_mode_fixup,
	.mode_set = radeon_crtc_mode_set,
	.mode_set_base = radeon_crtc_set_base,
	.prepare = radeon_crtc_prepare,
	.commit = radeon_crtc_commit,
};


void radeon_legacy_init_crtc(struct drm_device *dev,
			       struct radeon_crtc *radeon_crtc)
{
	drm_crtc_helper_add(&radeon_crtc->base, &legacy_helper_funcs);
}
