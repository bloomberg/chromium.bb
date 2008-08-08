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

void radeon_restore_common_regs(struct drm_device *dev, struct radeon_legacy_state *state)
{
	/* don't need this yet */
}

void radeon_restore_crtc_registers(struct drm_device *dev, struct radeon_legacy_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	RADEON_WRITE(RADEON_CRTC_GEN_CNTL, state->crtc_gen_cntl |
		     RADEON_CRTC_DISP_REQ_EN_B);

	RADEON_WRITE_P(RADEON_CRTC_EXT_CNTL, state->crtc_ext_cntl,
		       RADEON_CRTC_VSYNC_DIS | RADEON_CRTC_HSYNC_DIS | RADEON_CRTC_DISPLAY_DIS);

	RADEON_WRITE(RADEON_CRTC_H_TOTAL_DISP, state->crtc_h_total_disp);
	RADEON_WRITE(RADEON_CRTC_H_SYNC_STRT_WID, state->crtc_h_sync_strt_wid);
	RADEON_WRITE(RADEON_CRTC_V_TOTAL_DISP, state->crtc_v_total_disp);
	RADEON_WRITE(RADEON_CRTC_V_SYNC_STRT_WID, state->crtc_v_sync_strt_wid);

	if (radeon_is_r300(dev_priv))
		RADEON_WRITE(R300_CRTC_TILE_X0_Y0, state->crtc_tile_x0_y0);

	RADEON_WRITE(RADEON_CRTC_OFFSET_CNTL, state->crtc_offset_cntl);
	RADEON_WRITE(RADEON_CRTC_OFFSET, state->crtc_offset);

	RADEON_WRITE(RADEON_CRTC_PITCH, state->crtc_pitch);
	RADEON_WRITE(RADEON_DISP_MERGE_CNTL, state->disp_merge_cntl);

	/* if dell server */
	if (0)
	{
		RADEON_WRITE(RADEON_TV_DAC_CNTL, state->tv_dac_cntl);
		RADEON_WRITE(RADEON_DISP_HW_DEBUG, state->disp_hw_debug);
		RADEON_WRITE(RADEON_DAC_CNTL2, state->dac2_cntl);
		RADEON_WRITE(RADEON_CRTC2_GEN_CNTL, state->crtc2_gen_cntl);
	}

	RADEON_WRITE(RADEON_CRTC_GEN_CNTL, state->crtc_gen_cntl);
}

void radeon_restore_crtc2_registers(struct drm_device *dev, struct radeon_legacy_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	/* We prevent the CRTC from hitting th
  e memory controller until
	 * fully programmed
	 */
	RADEON_WRITE(RADEON_CRTC2_GEN_CNTL,
	       state->crtc2_gen_cntl | RADEON_CRTC2_VSYNC_DIS |
	       RADEON_CRTC2_HSYNC_DIS | RADEON_CRTC2_DISP_DIS |
	       RADEON_CRTC2_DISP_REQ_EN_B);

	RADEON_WRITE(RADEON_CRTC2_H_TOTAL_DISP,    state->crtc2_h_total_disp);
	RADEON_WRITE(RADEON_CRTC2_H_SYNC_STRT_WID, state->crtc2_h_sync_strt_wid);
	RADEON_WRITE(RADEON_CRTC2_V_TOTAL_DISP,    state->crtc2_v_total_disp);
	RADEON_WRITE(RADEON_CRTC2_V_SYNC_STRT_WID, state->crtc2_v_sync_strt_wid);
	
	RADEON_WRITE(RADEON_FP_H2_SYNC_STRT_WID,   state->fp_h2_sync_strt_wid);
	RADEON_WRITE(RADEON_FP_V2_SYNC_STRT_WID,   state->fp_v2_sync_strt_wid);

	if (radeon_is_r300(dev_priv))
		RADEON_WRITE(R300_CRTC2_TILE_X0_Y0, state->crtc2_tile_x0_y0);
	RADEON_WRITE(RADEON_CRTC2_OFFSET_CNTL,     state->crtc2_offset_cntl);
	RADEON_WRITE(RADEON_CRTC2_OFFSET,          state->crtc2_offset);

	RADEON_WRITE(RADEON_CRTC2_PITCH,           state->crtc2_pitch);
	RADEON_WRITE(RADEON_DISP2_MERGE_CNTL,      state->disp2_merge_cntl);

	RADEON_WRITE(RADEON_CRTC2_GEN_CNTL, state->crtc2_gen_cntl);
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

void radeon_restore_pll_registers(struct drm_device *dev, struct radeon_legacy_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint8_t pll_gain;
	
	pll_gain = radeon_compute_pll_gain(dev_priv->mode_info.pll.reference_freq,
					  state->ppll_ref_div & RADEON_PPLL_REF_DIV_MASK,
					  state->ppll_div_3 & RADEON_PPLL_FB3_DIV_MASK);

	if (dev_priv->flags & RADEON_IS_MOBILITY) {
		/* A temporal workaround for the occational blanking on certain laptop panels.
		   This appears to related to the PLL divider registers (fail to lock?).
		   It occurs even when all dividers are the same with their old settings.
		   In this case we really don't need to fiddle with PLL registers.
		   By doing this we can avoid the blanking problem with some panels.
		*/
		if ((state->ppll_ref_div == (RADEON_READ_PLL(dev_priv, RADEON_PPLL_REF_DIV) & RADEON_PPLL_REF_DIV_MASK)) &&
		    (state->ppll_div_3 == (RADEON_READ_PLL(dev_priv, RADEON_PPLL_DIV_3) & 
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
		if (state->ppll_ref_div & R300_PPLL_REF_DIV_ACC_MASK) {
			/* When restoring console mode, use saved PPLL_REF_DIV
			 * setting.
			 */
			RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_REF_DIV,
				state->ppll_ref_div,
				0);
		} else {
			/* R300 uses ref_div_acc field as real ref divider */
			RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_REF_DIV,
				(state->ppll_ref_div << R300_PPLL_REF_DIV_ACC_SHIFT),
				~R300_PPLL_REF_DIV_ACC_MASK);
		}
	} else {
		RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_REF_DIV,
			state->ppll_ref_div,
			~RADEON_PPLL_REF_DIV_MASK);
	}

	RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_DIV_3,
		state->ppll_div_3,
		~RADEON_PPLL_FB3_DIV_MASK);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_DIV_3,
		state->ppll_div_3,
		~RADEON_PPLL_POST3_DIV_MASK);

	radeon_pll_write_update(dev);
	radeon_pll_wait_for_read_update_complete(dev);

	RADEON_WRITE_PLL(dev_priv, RADEON_HTOTAL_CNTL, state->htotal_cntl);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_PPLL_CNTL,
		0,
		~(RADEON_PPLL_RESET
		  | RADEON_PPLL_SLEEP
		  | RADEON_PPLL_ATOMIC_UPDATE_EN
		  | RADEON_PPLL_VGA_ATOMIC_UPDATE_EN));

	DRM_DEBUG("Wrote: 0x%08x 0x%08x 0x%08x (0x%08x)\n",
		  state->ppll_ref_div,
		  state->ppll_div_3,
		  (unsigned)state->htotal_cntl,
		  RADEON_READ_PLL(dev_priv, RADEON_PPLL_CNTL));
	DRM_DEBUG("Wrote: rd=%d, fd=%d, pd=%d\n",
		  state->ppll_ref_div & RADEON_PPLL_REF_DIV_MASK,
		  state->ppll_div_3 & RADEON_PPLL_FB3_DIV_MASK,
		  (state->ppll_div_3 & RADEON_PPLL_POST3_DIV_MASK) >> 16);

	mdelay(50); /* Let the clock to lock */

	RADEON_WRITE_PLL_P(dev_priv, RADEON_VCLK_ECP_CNTL,
			   RADEON_VCLK_SRC_SEL_PPLLCLK,
			   ~(RADEON_VCLK_SRC_SEL_MASK));

	/*RADEON_WRITE_PLL(dev_priv, RADEON_VCLK_ECP_CNTL, state->vclk_ecp_cntl);*/

}

void radeon_restore_pll2_registers(struct drm_device *dev, struct radeon_legacy_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint8_t pll_gain;

	pll_gain = radeon_compute_pll_gain(dev_priv->mode_info.pll.reference_freq,
					   state->p2pll_ref_div & RADEON_P2PLL_REF_DIV_MASK,
					   state->p2pll_div_0 & RADEON_P2PLL_FB0_DIV_MASK);


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
		state->p2pll_ref_div,
		~RADEON_P2PLL_REF_DIV_MASK);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_P2PLL_DIV_0,
		state->p2pll_div_0,
		~RADEON_P2PLL_FB0_DIV_MASK);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_P2PLL_DIV_0,
		state->p2pll_div_0,
		~RADEON_P2PLL_POST0_DIV_MASK);

	radeon_pll2_write_update(dev);
	radeon_pll2_wait_for_read_update_complete(dev);

	RADEON_WRITE_PLL(dev_priv, RADEON_HTOTAL2_CNTL, state->htotal_cntl2);

	RADEON_WRITE_PLL_P(dev_priv, RADEON_P2PLL_CNTL,
		0,
		~(RADEON_P2PLL_RESET
		  | RADEON_P2PLL_SLEEP
		  | RADEON_P2PLL_ATOMIC_UPDATE_EN));

	DRM_DEBUG("Wrote2: 0x%08x 0x%08x 0x%08x (0x%08x)\n",
		  (unsigned)state->p2pll_ref_div,
		  (unsigned)state->p2pll_div_0,
		  (unsigned)state->htotal_cntl2,
		  RADEON_READ_PLL(dev_priv, RADEON_P2PLL_CNTL));
	DRM_DEBUG("Wrote2: rd=%u, fd=%u, pd=%u\n",
		  (unsigned)state->p2pll_ref_div & RADEON_P2PLL_REF_DIV_MASK,
		  (unsigned)state->p2pll_div_0 & RADEON_P2PLL_FB0_DIV_MASK,
		  (unsigned)((state->p2pll_div_0 &
			      RADEON_P2PLL_POST0_DIV_MASK) >>16));

	mdelay(50); /* Let the clock to lock */

	RADEON_WRITE_PLL_P(dev_priv, RADEON_PIXCLKS_CNTL,
		RADEON_PIX2CLK_SRC_SEL_P2PLLCLK,
		~(RADEON_PIX2CLK_SRC_SEL_MASK));

	RADEON_WRITE_PLL(dev_priv, RADEON_PIXCLKS_CNTL, state->pixclks_cntl);


}


void radeon_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;

	uint32_t mask;

	mask = radeon_crtc->crtc_id ? 
		(RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS | RADEON_CRTC2_DISP_REQ_EN_B) :
		(RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS | RADEON_CRTC_HSYNC_DIS);

	switch(mode) {
	case DRM_MODE_DPMS_ON:
		if (radeon_crtc->crtc_id) {
			RADEON_WRITE_P(RADEON_CRTC2_GEN_CNTL, 0, ~mask);
		} else {
			RADEON_WRITE_P(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_DISP_REQ_EN_B);
			RADEON_WRITE_P(RADEON_CRTC_EXT_CNTL, 0, ~mask);
		}
		break;
	case DRM_MODE_DPMS_STANDBY:
		if (radeon_crtc->crtc_id) {
			RADEON_WRITE_P(RADEON_CRTC2_GEN_CNTL, (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_HSYNC_DIS), ~mask);
		} else {
			RADEON_WRITE_P(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_DISP_REQ_EN_B);
			RADEON_WRITE_P(RADEON_CRTC_EXT_CNTL, (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS), ~mask);
		}
		break;
	case DRM_MODE_DPMS_SUSPEND:
		if (radeon_crtc->crtc_id) {
			RADEON_WRITE_P(RADEON_CRTC2_GEN_CNTL, (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS), ~mask);
		} else {
			RADEON_WRITE_P(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_DISP_REQ_EN_B);
			RADEON_WRITE_P(RADEON_CRTC_EXT_CNTL, (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS), ~mask);
		}
		break;
	case DRM_MODE_DPMS_OFF:
		if (radeon_crtc->crtc_id) {
			RADEON_WRITE_P(RADEON_CRTC2_GEN_CNTL, mask, ~mask);
		} else {
			RADEON_WRITE_P(RADEON_CRTC_GEN_CNTL, RADEON_CRTC_DISP_REQ_EN_B, ~RADEON_CRTC_DISP_REQ_EN_B);
			RADEON_WRITE_P(RADEON_CRTC_EXT_CNTL, mask, ~mask);
		}
		break;
	}
  
	if (mode != DRM_MODE_DPMS_OFF) {
		radeon_crtc_load_lut(crtc);
	}
}

static bool radeon_init_crtc_base(struct drm_crtc *crtc, struct radeon_legacy_state *state, int x, int y)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);	
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_framebuffer *radeon_fb;
	struct drm_radeon_gem_object *obj_priv;
	uint32_t base;

	radeon_fb = to_radeon_framebuffer(crtc->fb);

	obj_priv = radeon_fb->obj->driver_private;

	state->crtc_offset = obj_priv->bo->offset + dev_priv->fb_location;

	state->crtc_offset_cntl = 0;

	/* TODO tiling */
	if (0) {
		if (radeon_is_r300(dev_priv)) {
			state->crtc_offset_cntl |= (R300_CRTC_X_Y_MODE_EN |
						   R300_CRTC_MICRO_TILE_BUFFER_DIS |
						   R300_CRTC_MACRO_TILE_EN);
		} else {
			state->crtc_offset_cntl |= RADEON_CRTC_TILE_EN;
		}
	} else {
		if (radeon_is_r300(dev_priv)) {
			state->crtc_offset_cntl &= ~(R300_CRTC_X_Y_MODE_EN |
						    R300_CRTC_MICRO_TILE_BUFFER_DIS |
						    R300_CRTC_MACRO_TILE_EN);
		} else {
			state->crtc_offset_cntl &= ~RADEON_CRTC_TILE_EN;
		}
	}

	base = obj_priv->bo->offset;

	/* TODO more tiling */
	if (0) {
		if (radeon_is_r300(dev_priv)) {
			    state->crtc_tile_x0_y0 = x | (y << 16);
			    base &= ~0x7ff;
		    } else {
			    int byteshift = crtc->fb->bits_per_pixel >> 4;
			    int tile_addr = (((y >> 3) * crtc->fb->width + x) >> (8 - byteshift)) << 11;
			    base += tile_addr + ((x << byteshift) % 256) + ((y % 8) << 8);
			    state->crtc_offset_cntl |= (y % 16);
		    }
	} else {
		int offset = y * crtc->fb->pitch + x;
		switch (crtc->fb->bits_per_pixel) {
		}
		base += offset;
	}

	base &= ~7;

	/* update sarea TODO */

	state->crtc_offset = base;
	return true;
}

static bool radeon_init_crtc_registers(struct drm_crtc *crtc, struct radeon_legacy_state *state,
				      struct drm_display_mode *mode)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);	
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int format;
	int hsync_start;
	int hsync_wid;
	int vsync_wid;

	switch (crtc->fb->depth) {
		
	case 15: format = 3; break;      /*  555 */
	case 16: format = 4; break;      /*  565 */
	case 24: format = 5; break;      /*  RGB */
	case 32: format = 6; break;      /* xRGB */
	default:
		return false;
	}

	state->crtc_gen_cntl = (RADEON_CRTC_EXT_DISP_EN
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

	state->crtc_ext_cntl |= (RADEON_XCRT_CNT_EN|
				 RADEON_CRTC_VSYNC_DIS |
				 RADEON_CRTC_HSYNC_DIS |
				 RADEON_CRTC_DISPLAY_DIS);

	state->disp_merge_cntl = RADEON_READ(RADEON_DISP_MERGE_CNTL);
	state->disp_merge_cntl &= ~RADEON_DISP_RGB_OFFSET_EN;

	state->crtc_h_total_disp = ((((mode->crtc_htotal / 8) - 1) & 0x3ff)
				   | ((((mode->crtc_hdisplay / 8) - 1) & 0x1ff)
				      << 16));

	hsync_wid = (mode->crtc_hsync_end - mode->crtc_hsync_start) / 8;
	if (!hsync_wid)       hsync_wid = 1;
	hsync_start = mode->crtc_hsync_start - 8;

	state->crtc_h_sync_strt_wid = ((hsync_start & 0x1fff)
				      | ((hsync_wid & 0x3f) << 16)
				      | ((mode->flags & DRM_MODE_FLAG_NHSYNC)
					 ? RADEON_CRTC_H_SYNC_POL
					 : 0));

	/* This works for double scan mode. */
	state->crtc_v_total_disp = (((mode->crtc_vtotal - 1) & 0xffff)
				   | ((mode->crtc_vdisplay - 1) << 16));

	vsync_wid = mode->crtc_vsync_end - mode->crtc_vsync_start;
	if (!vsync_wid)       vsync_wid = 1;

	state->crtc_v_sync_strt_wid = (((mode->crtc_vsync_start - 1) & 0xfff)
				      | ((vsync_wid & 0x1f) << 16)
				      | ((mode->flags & DRM_MODE_FLAG_NVSYNC)
					 ? RADEON_CRTC_V_SYNC_POL
					 : 0));

	state->crtc_pitch  = (((crtc->fb->pitch * crtc->fb->bits_per_pixel) +
			      ((crtc->fb->bits_per_pixel * 8) -1)) /
			     (crtc->fb->bits_per_pixel * 8));
	state->crtc_pitch |= state->crtc_pitch << 16;

	/* TODO -> Dell Server */
	if (0) {
//		state->dac2_cntl = info->StatedReg->dac2_cntl;
//		state->tv_dac_cntl = info->StatedReg->tv_dac_cntl;
//		state->crtc2_gen_cntl = info->StatedReg->crtc2_gen_cntl;
//		state->disp_hw_debug = info->StatedReg->disp_hw_debug;

//		state->dac2_cntl &= ~RADEON_DAC2_DAC_CLK_SEL;
//		state->dac2_cntl |= RADEON_DAC2_DAC2_CLK_SEL;

		/* For CRT on DAC2, don't turn it on if BIOS didn't
		   enable it, even it's detected.
		*/
		state->disp_hw_debug |= RADEON_CRT2_DISP1_SEL;
		state->tv_dac_cntl &= ~((1<<2) | (3<<8) | (7<<24) | (0xff<<16));
		state->tv_dac_cntl |= (0x03 | (2<<8) | (0x58<<16));
	}

	return true;
}

static void radeon_init_pll_registers(struct drm_crtc *crtc, struct radeon_legacy_state *state,
				      struct drm_display_mode *mode, int flags)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);	
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t feedback_div = 0;
	uint32_t reference_div = 0;
	uint32_t post_divider = 0;
	uint32_t freq = 0;
	struct radeon_pll *pll = &dev_priv->mode_info.pll;
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
	
#if 0 // TODO
	if ((flags & RADEON_PLL_USE_BIOS_DIVS) && info->UseBiosDividers) {
		state->ppll_ref_div = info->RefDivider;
		state->ppll_div_3   = info->FeedbackDivider | (info->PostDivider << 16);
		state->htotal_cntl  = 0;
		return;
	}
#endif

	DRM_DEBUG("\n");
	radeon_compute_pll(pll, mode->clock, &freq, &feedback_div, &reference_div, &post_divider, flags);

	for (post_div = &post_divs[0]; post_div->divider; ++post_div) {
		if (post_div->divider == post_divider)
			break;
	}

	if (!post_div->divider) {
		state->pll_output_freq = freq;
		post_div = &post_divs[0];
	}
	
	state->dot_clock_freq = freq;
	state->feedback_div   = feedback_div;
	state->reference_div  = reference_div;
	state->post_div       = post_divider;

	DRM_DEBUG("dc=%u, of=%u, fd=%d, rd=%d, pd=%d\n",
		  (unsigned)state->dot_clock_freq,
		  (unsigned)state->pll_output_freq,
		  state->feedback_div,
		  state->reference_div,
		  state->post_div);

	state->ppll_ref_div   = state->reference_div;

#if defined(__powerpc__) && (0) /* TODO */
	/* apparently programming this otherwise causes a hang??? */
	if (info->MacModel == RADEON_MAC_IBOOK)
		state->ppll_div_3 = 0x000600ad;
	else
#endif
		state->ppll_div_3     = (state->feedback_div | (post_div->bitvalue << 16));
	
    state->htotal_cntl    = mode->htotal & 0x7;

    state->vclk_ecp_cntl = (RADEON_READ_PLL(dev_priv, RADEON_VCLK_ECP_CNTL) &
			    ~RADEON_VCLK_SRC_SEL_MASK) | RADEON_VCLK_SRC_SEL_PPLLCLK;

}

static bool radeon_crtc_mode_fixup(struct drm_crtc *crtc,
				   struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void radeon_crtc_mode_set(struct drm_crtc *crtc,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode,
				 int x, int y)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);	
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_encoder *encoder;
	int pll_flags = RADEON_PLL_LEGACY | RADEON_PLL_PREFER_LOW_REF_DIV;
	int i;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		struct radeon_encoder *radeon_encoder = to_radeon_encoder(encoder);

		if (encoder->crtc == crtc) {
			if (encoder->encoder_type != DRM_MODE_ENCODER_DAC)
				pll_flags |= RADEON_PLL_NO_ODD_POST_DIV;
			if (encoder->encoder_type == DRM_MODE_ENCODER_LVDS)
				pll_flags |= RADEON_PLL_USE_BIOS_DIVS | RADEON_PLL_USE_REF_DIV;
		}
	}

	switch(radeon_crtc->crtc_id) {
	case 0:
		radeon_init_crtc_registers(crtc, &dev_priv->mode_info.legacy_state, adjusted_mode);
		radeon_init_crtc_base(crtc, &dev_priv->mode_info.legacy_state, crtc->x, crtc->y);
//		dot_clock = adjusted_mode->clock / 1000;

		//	if (dot_clock)
		radeon_init_pll_registers(crtc, &dev_priv->mode_info.legacy_state, adjusted_mode,
					  pll_flags);
		break;
	case 1:
		break;

	}

	/* TODO TV */

	switch (radeon_crtc->crtc_id) {
	case 0:
		radeon_restore_crtc_registers(dev, &dev_priv->mode_info.legacy_state);
		radeon_restore_pll_registers(dev, &dev_priv->mode_info.legacy_state);
		break;
	case 1:
		radeon_restore_crtc2_registers(dev, &dev_priv->mode_info.legacy_state);
		radeon_restore_pll2_registers(dev, &dev_priv->mode_info.legacy_state);
		break;
	}

}

void radeon_crtc_set_base(struct drm_crtc *crtc, int x, int y)
{
}

static void radeon_crtc_prepare(struct drm_crtc *crtc)
{
}

static void radeon_crtc_commit(struct drm_crtc *crtc)
{
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
