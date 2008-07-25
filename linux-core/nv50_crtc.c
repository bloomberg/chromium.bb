/*
 * Copyright (C) 2008 Maarten Maathuis.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "nv50_crtc.h"
#include "nv50_cursor.h"
#include "nv50_lut.h"
#include "nv50_fb.h"

static int nv50_crtc_validate_mode(struct nv50_crtc *crtc, struct nouveau_hw_mode *mode)
{
	NV50_DEBUG("\n");

	if (mode->clock > 400000)
		return MODE_CLOCK_HIGH;

	if (mode->clock < 25000)
		return MODE_CLOCK_LOW;

	return MODE_OK;
}

static int nv50_crtc_set_mode(struct nv50_crtc *crtc, struct nouveau_hw_mode *mode)
{
	struct nouveau_hw_mode *hw_mode = crtc->mode;
	uint8_t rval;

	NV50_DEBUG("index %d\n", crtc->index);

	if (!mode) {
		DRM_ERROR("No mode\n");
		return MODE_NOMODE;
	}

	if ((rval = crtc->validate_mode(crtc, mode))) {
		DRM_ERROR("Mode invalid\n");
		return rval;
	}

	/* copy values to mode */
	*hw_mode = *mode;

	return 0;
}

static int nv50_crtc_execute_mode(struct nv50_crtc *crtc)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;
	struct nouveau_hw_mode *hw_mode;
	uint32_t hsync_dur,  vsync_dur, hsync_start_to_end, vsync_start_to_end;
	uint32_t hunk1, vunk1, vunk2a, vunk2b;
	uint32_t offset = crtc->index * 0x400;

	NV50_DEBUG("index %d\n", crtc->index);
	NV50_DEBUG("%s native mode\n", crtc->use_native_mode ? "using" : "not using");

	if (crtc->use_native_mode)
		hw_mode = crtc->native_mode;
	else
		hw_mode = crtc->mode;

	hsync_dur = hw_mode->hsync_end - hw_mode->hsync_start;
	vsync_dur = hw_mode->vsync_end - hw_mode->vsync_start;
	hsync_start_to_end = hw_mode->hblank_end - hw_mode->hsync_start;
	vsync_start_to_end = hw_mode->vblank_end - hw_mode->vsync_start;
	/* I can't give this a proper name, anyone else can? */
	hunk1 = hw_mode->htotal - hw_mode->hsync_start + hw_mode->hblank_start;
	vunk1 = hw_mode->vtotal - hw_mode->vsync_start + hw_mode->vblank_start;
	/* Another strange value, this time only for interlaced modes. */
	vunk2a = 2*hw_mode->vtotal - hw_mode->vsync_start + hw_mode->vblank_start;
	vunk2b = hw_mode->vtotal - hw_mode->vsync_start + hw_mode->vblank_end;

	if (hw_mode->flags & DRM_MODE_FLAG_INTERLACE) {
		vsync_dur /= 2;
		vsync_start_to_end  /= 2;
		vunk1 /= 2;
		vunk2a /= 2;
		vunk2b /= 2;
		/* magic */
		if (hw_mode->flags & DRM_MODE_FLAG_DBLSCAN) {
			vsync_start_to_end -= 1;
			vunk1 -= 1;
			vunk2a -= 1;
			vunk2b -= 1;
		}
	}

	OUT_MODE(NV50_CRTC0_CLOCK + offset, hw_mode->clock | 0x800000);
	OUT_MODE(NV50_CRTC0_INTERLACE + offset, (hw_mode->flags & DRM_MODE_FLAG_INTERLACE) ? 2 : 0);
	OUT_MODE(NV50_CRTC0_DISPLAY_START + offset, 0);
	OUT_MODE(NV50_CRTC0_UNK82C + offset, 0);
	OUT_MODE(NV50_CRTC0_DISPLAY_TOTAL + offset, hw_mode->vtotal << 16 | hw_mode->htotal);
	OUT_MODE(NV50_CRTC0_SYNC_DURATION + offset, (vsync_dur - 1) << 16 | (hsync_dur - 1));
	OUT_MODE(NV50_CRTC0_SYNC_START_TO_BLANK_END + offset, (vsync_start_to_end - 1) << 16 | (hsync_start_to_end - 1));
	OUT_MODE(NV50_CRTC0_MODE_UNK1 + offset, (vunk1 - 1) << 16 | (hunk1 - 1));
	if (hw_mode->flags & DRM_MODE_FLAG_INTERLACE) {
		OUT_MODE(NV50_CRTC0_MODE_UNK2 + offset, (vunk2b - 1) << 16 | (vunk2a - 1));
	}

	crtc->set_fb(crtc);
	crtc->set_dither(crtc);

	/* This is the actual resolution of the mode. */
	OUT_MODE(NV50_CRTC0_REAL_RES + offset, (crtc->mode->vdisplay << 16) | crtc->mode->hdisplay);
	OUT_MODE(NV50_CRTC0_SCALE_CENTER_OFFSET + offset, NV50_CRTC_SCALE_CENTER_OFFSET_VAL(0,0));

	/* Maybe move this as well? */
	crtc->blank(crtc, false);

	return 0;
}

static int nv50_crtc_set_fb(struct nv50_crtc *crtc)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;
	uint32_t offset = crtc->index * 0x400;

	NV50_DEBUG("\n");

	OUT_MODE(NV50_CRTC0_FB_SIZE + offset, crtc->fb->height << 16 | crtc->fb->width);

	/* I suspect this flag indicates a linear fb. */
	OUT_MODE(NV50_CRTC0_FB_PITCH + offset, crtc->fb->pitch | 0x100000);

	switch (crtc->fb->depth) {
		case 8:
			OUT_MODE(NV50_CRTC0_DEPTH + offset, NV50_CRTC0_DEPTH_8BPP); 
			break;
		case 15:
			OUT_MODE(NV50_CRTC0_DEPTH + offset, NV50_CRTC0_DEPTH_15BPP);
			break;
		case 16:
			OUT_MODE(NV50_CRTC0_DEPTH + offset, NV50_CRTC0_DEPTH_16BPP);
			break;
		case 24:
			OUT_MODE(NV50_CRTC0_DEPTH + offset, NV50_CRTC0_DEPTH_24BPP); 
			break;
	}

	OUT_MODE(NV50_CRTC0_COLOR_CTRL + offset, NV50_CRTC_COLOR_CTRL_MODE_COLOR);
	OUT_MODE(NV50_CRTC0_FB_POS + offset, (crtc->fb->y << 16) | (crtc->fb->x));

	return 0;
}

static int nv50_crtc_blank(struct nv50_crtc *crtc, bool blanked)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;
	uint32_t offset = crtc->index * 0x400;

	NV50_DEBUG("index %d\n", crtc->index);
	NV50_DEBUG("%s\n", blanked ? "blanked" : "unblanked");

	/* We really need a framebuffer. */
	if (!crtc->fb->block && !blanked) {
		DRM_ERROR("No framebuffer available on crtc %d\n", crtc->index);
		return -EINVAL;
	}

	if (blanked) {
		crtc->cursor->hide(crtc);

		OUT_MODE(NV50_CRTC0_CLUT_MODE + offset, NV50_CRTC0_CLUT_MODE_BLANK);
		OUT_MODE(NV50_CRTC0_CLUT_OFFSET + offset, 0);
		if (dev_priv->chipset != 0x50)
			OUT_MODE(NV84_CRTC0_BLANK_UNK1 + offset, NV84_CRTC0_BLANK_UNK1_BLANK);
		OUT_MODE(NV50_CRTC0_BLANK_CTRL + offset, NV50_CRTC0_BLANK_CTRL_BLANK);
		if (dev_priv->chipset != 0x50)
			OUT_MODE(NV84_CRTC0_BLANK_UNK2 + offset, NV84_CRTC0_BLANK_UNK2_BLANK);
	} else {
		OUT_MODE(NV50_CRTC0_FB_OFFSET + offset, crtc->fb->block->start >> 8);
		OUT_MODE(0x864 + offset, 0);

		crtc->cursor->set_offset(crtc);

		if (dev_priv->chipset != 0x50)
			OUT_MODE(NV84_CRTC0_BLANK_UNK2 + offset, NV84_CRTC0_BLANK_UNK2_UNBLANK);

		if (crtc->cursor->visible)
			crtc->cursor->show(crtc);
		else
			crtc->cursor->hide(crtc);

		OUT_MODE(NV50_CRTC0_CLUT_MODE + offset, 
			crtc->fb->depth == 8 ? NV50_CRTC0_CLUT_MODE_OFF : NV50_CRTC0_CLUT_MODE_ON);
		OUT_MODE(NV50_CRTC0_CLUT_OFFSET + offset, crtc->lut->block->start >> 8);
		if (dev_priv->chipset != 0x50)
			OUT_MODE(NV84_CRTC0_BLANK_UNK1 + offset, NV84_CRTC0_BLANK_UNK1_UNBLANK);
		OUT_MODE(NV50_CRTC0_BLANK_CTRL + offset, NV50_CRTC0_BLANK_CTRL_UNBLANK);
	}

	/* sometimes you need to know if a screen is already blanked. */
	crtc->blanked = blanked;

	return 0;
}

static int nv50_crtc_set_dither(struct nv50_crtc *crtc)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;
	uint32_t offset = crtc->index * 0x400;

	NV50_DEBUG("\n");

	OUT_MODE(NV50_CRTC0_DITHERING_CTRL + offset, crtc->use_dithering ? 
			NV50_CRTC0_DITHERING_CTRL_ON : NV50_CRTC0_DITHERING_CTRL_OFF);

	return 0;
}

static void nv50_crtc_calc_scale(struct nv50_crtc *crtc, uint32_t *outX, uint32_t *outY)
{
	uint32_t hor_scale, ver_scale;

	/* max res is 8192, which is 2^13, which leaves 19 bits */
	hor_scale = (crtc->native_mode->hdisplay << 19)/crtc->mode->hdisplay;
	ver_scale = (crtc->native_mode->vdisplay << 19)/crtc->mode->vdisplay;

	if (ver_scale > hor_scale) {
		*outX = (crtc->mode->hdisplay * hor_scale) >> 19;
		*outY = (crtc->mode->vdisplay * hor_scale) >> 19;
	} else {
		*outX = (crtc->mode->hdisplay * ver_scale) >> 19;
		*outY = (crtc->mode->vdisplay * ver_scale) >> 19;
	}
}

static int nv50_crtc_set_scale(struct nv50_crtc *crtc)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;
	uint32_t offset = crtc->index * 0x400;
	uint32_t outX, outY;

	NV50_DEBUG("\n");

	switch (crtc->requested_scaling_mode) {
		case SCALE_ASPECT:
			nv50_crtc_calc_scale(crtc, &outX, &outY);
			break;
		case SCALE_FULLSCREEN:
			outX = crtc->native_mode->hdisplay;
			outY = crtc->native_mode->vdisplay;
			break;
		case SCALE_NOSCALE:
		case SCALE_NON_GPU:
		default:
			outX = crtc->mode->hdisplay;
			outY = crtc->mode->vdisplay;
			break;
	}

	/* Got a better name for SCALER_ACTIVE? */
	/* One day i've got to really figure out why this is needed. */
	if ((crtc->mode->flags & DRM_MODE_FLAG_DBLSCAN) || (crtc->mode->flags & DRM_MODE_FLAG_INTERLACE) ||
		crtc->mode->hdisplay != outX || crtc->mode->vdisplay != outY) {
		OUT_MODE(NV50_CRTC0_SCALE_CTRL + offset, NV50_CRTC0_SCALE_CTRL_SCALER_ACTIVE);
	} else {
		OUT_MODE(NV50_CRTC0_SCALE_CTRL + offset, NV50_CRTC0_SCALE_CTRL_SCALER_INACTIVE);
	}

	OUT_MODE(NV50_CRTC0_SCALE_RES1 + offset, outY << 16 | outX);
	OUT_MODE(NV50_CRTC0_SCALE_RES2 + offset, outY << 16 | outX);

	/* processed */
	crtc->scaling_mode = crtc->requested_scaling_mode;

	return 0;
}

static int nv50_crtc_calc_clock(struct nv50_crtc *crtc, 
	uint32_t *bestN1, uint32_t *bestN2, uint32_t *bestM1, uint32_t *bestM2, uint32_t *bestlog2P)
{
	struct nouveau_hw_mode *hw_mode;
	struct pll_lims limits;
	int clk, vco2, crystal;
	int minvco1, minvco2, minU1, maxU1, minU2, maxU2, minM1, maxM1;
	int maxvco1, maxvco2, minN1, maxN1, minM2, maxM2, minN2, maxN2;
	bool fixedgain2;
	int M1, N1, M2, N2, log2P;
	int clkP, calcclk1, calcclk2, calcclkout;
	int delta, bestdelta = INT_MAX;
	int bestclk = 0;

	NV50_DEBUG("\n");

	if (crtc->use_native_mode)
		hw_mode = crtc->native_mode;
	else
		hw_mode = crtc->mode;

	clk = hw_mode->clock;

	/* These are in the g80 bios tables, at least in mine. */
	if (!get_pll_limits(crtc->dev, NV50_PDISPLAY_CRTC_CLK_CLK_CTRL1(crtc->index), &limits))
		return -EINVAL;

	minvco1 = limits.vco1.minfreq, maxvco1 = limits.vco1.maxfreq;
	minvco2 = limits.vco2.minfreq, maxvco2 = limits.vco2.maxfreq;
	minU1 = limits.vco1.min_inputfreq, minU2 = limits.vco2.min_inputfreq;
	maxU1 = limits.vco1.max_inputfreq, maxU2 = limits.vco2.max_inputfreq;
	minM1 = limits.vco1.min_m, maxM1 = limits.vco1.max_m;
	minN1 = limits.vco1.min_n, maxN1 = limits.vco1.max_n;
	minM2 = limits.vco2.min_m, maxM2 = limits.vco2.max_m;
	minN2 = limits.vco2.min_n, maxN2 = limits.vco2.max_n;
	crystal = limits.refclk;
	fixedgain2 = (minM2 == maxM2 && minN2 == maxN2);

	vco2 = (maxvco2 - maxvco2/200) / 2;
	for (log2P = 0; clk && log2P < 6 && clk <= (vco2 >> log2P); log2P++) /* log2P is maximum of 6 */
		;
	clkP = clk << log2P;

	if (maxvco2 < clk + clk/200)	/* +0.5% */
		maxvco2 = clk + clk/200;

	for (M1 = minM1; M1 <= maxM1; M1++) {
		if (crystal/M1 < minU1)
			return bestclk;
		if (crystal/M1 > maxU1)
			continue;

		for (N1 = minN1; N1 <= maxN1; N1++) {
			calcclk1 = crystal * N1 / M1;
			if (calcclk1 < minvco1)
				continue;
			if (calcclk1 > maxvco1)
				break;

			for (M2 = minM2; M2 <= maxM2; M2++) {
				if (calcclk1/M2 < minU2)
					break;
				if (calcclk1/M2 > maxU2)
					continue;

				/* add calcclk1/2 to round better */
				N2 = (clkP * M2 + calcclk1/2) / calcclk1;
				if (N2 < minN2)
					continue;
				if (N2 > maxN2)
					break;

				if (!fixedgain2) {
					calcclk2 = calcclk1 * N2 / M2;
					if (calcclk2 < minvco2)
						break;
					if (calcclk2 > maxvco2)
						continue;
				} else
					calcclk2 = calcclk1;

				calcclkout = calcclk2 >> log2P;
				delta = abs(calcclkout - clk);
				/* we do an exhaustive search rather than terminating
				 * on an optimality condition...
				 */
				if (delta < bestdelta) {
					bestdelta = delta;
					bestclk = calcclkout;
					*bestN1 = N1;
					*bestN2 = N2;
					*bestM1 = M1;
					*bestM2 = M2;
					*bestlog2P = log2P;
					if (delta == 0)	/* except this one */
						return bestclk;
				}
			}
		}
	}

	return bestclk;
}

static int nv50_crtc_set_clock(struct nv50_crtc *crtc)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;

	uint32_t pll_reg = NV50_PDISPLAY_CRTC_CLK_CLK_CTRL1(crtc->index);

	uint32_t N1 = 0, N2 = 0, M1 = 0, M2 = 0, log2P = 0;

	uint32_t reg1 = NV_READ(pll_reg + 4);
	uint32_t reg2 = NV_READ(pll_reg + 8);

	NV50_DEBUG("\n");

	NV_WRITE(pll_reg, NV50_PDISPLAY_CRTC_CLK_CLK_CTRL1_CONNECTED | 0x10000011);

	/* The other bits are typically empty, but let's be on the safe side. */
	reg1 &= 0xff00ff00;
	reg2 &= 0x8000ff00;

	if (!nv50_crtc_calc_clock(crtc, &N1, &N2, &M1, &M2, &log2P))
		return -EINVAL;

	NV50_DEBUG("N1 %d N2 %d M1 %d M2 %d log2P %d\n", N1, N2, M1, M2, log2P);

	reg1 |= (M1 << 16) | N1;
	reg2 |= (log2P << 28) | (M2 << 16) | N2;

	NV_WRITE(pll_reg + 4, reg1);
	NV_WRITE(pll_reg + 8, reg2);

	return 0;
}

static int nv50_crtc_set_clock_mode(struct nv50_crtc *crtc)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;

	NV50_DEBUG("\n");

	/* This acknowledges a clock request. */
	NV_WRITE(NV50_PDISPLAY_CRTC_CLK_CLK_CTRL2(crtc->index), 0);

	return 0;
}

static int nv50_crtc_destroy(struct nv50_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_display *display = nv50_get_display(dev);

	NV50_DEBUG("\n");

	if (!display || !crtc)
		return -EINVAL;

	list_del(&crtc->item);

	nv50_fb_destroy(crtc);
	nv50_lut_destroy(crtc);
	nv50_cursor_destroy(crtc);

	kfree(crtc->mode);
	kfree(crtc->native_mode);

	if (dev_priv->free_crtc)
		dev_priv->free_crtc(crtc);

	return 0;
}

int nv50_crtc_create(struct drm_device *dev, int index)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv50_crtc *crtc = NULL;
	struct nv50_display *display = NULL;
	int rval = 0;

	NV50_DEBUG("\n");

	/* This allows the public layer to do it's thing. */
	if (dev_priv->alloc_crtc)
		crtc = dev_priv->alloc_crtc(dev);

	if (!crtc)
		return -ENOMEM;

	crtc->dev = dev;

	display = nv50_get_display(dev);
	if (!display) {
		rval = -EINVAL;
		goto out;
	}

	list_add_tail(&crtc->item, &display->crtcs);

	crtc->index = index;

	crtc->mode = kzalloc(sizeof(struct nouveau_hw_mode), GFP_KERNEL);
	crtc->native_mode = kzalloc(sizeof(struct nouveau_hw_mode), GFP_KERNEL);

	crtc->requested_scaling_mode = SCALE_INVALID;
	crtc->scaling_mode = SCALE_INVALID;

	if (!crtc->mode || !crtc->native_mode) {
		rval = -ENOMEM;
		goto out;
	}

	nv50_fb_create(crtc);
	nv50_lut_create(crtc);
	nv50_cursor_create(crtc);

	/* set function pointers */
	crtc->validate_mode = nv50_crtc_validate_mode;
	crtc->set_mode = nv50_crtc_set_mode;
	crtc->execute_mode = nv50_crtc_execute_mode;
	crtc->set_fb = nv50_crtc_set_fb;
	crtc->blank = nv50_crtc_blank;
	crtc->set_dither = nv50_crtc_set_dither;
	crtc->set_scale = nv50_crtc_set_scale;
	crtc->set_clock = nv50_crtc_set_clock;
	crtc->set_clock_mode = nv50_crtc_set_clock_mode;
	crtc->destroy = nv50_crtc_destroy;

	return 0;

out:
	if (crtc->mode)
		kfree(crtc->mode);
	if (crtc->native_mode)
		kfree(crtc->native_mode);
	if (dev_priv->free_crtc)
		dev_priv->free_crtc(crtc);

	return rval;
}
