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

#include "nv50_fb.h"
#include "nv50_lut.h"
#include "nv50_crtc.h"
#include "nv50_display.h"

static int nv50_fb_bind(struct nv50_crtc *crtc, struct nv50_fb_info *info)
{
	int rval = 0;

	NV50_DEBUG("\n");

	if (!crtc || !info) {
		DRM_ERROR("crtc %p info %p\n",crtc, info);
		return -EINVAL;
	}

	if (!info->block || !info->width || !info->height || !info->depth || !info->bpp) {
		DRM_ERROR("block %p width %d height %d depth %d bpp %d\n", info->block, info->width, info->height, info->depth, info->bpp);
		return -EINVAL;
	}

	crtc->fb->block = info->block;
	crtc->fb->width = info->width;
	crtc->fb->height = info->height;

	crtc->fb->y = info->x;
	crtc->fb->x = info->y;

	crtc->fb->depth = info->depth;
	crtc->fb->bpp = info->bpp;

	/* update lut if needed */
	if (crtc->fb->depth != crtc->lut->depth) {
		int r_size = 0, g_size = 0, b_size = 0;
		uint16_t *r_val, *g_val, *b_val;
		int i;

		switch (crtc->fb->depth) {
			case 15:
				r_size = 32;
				g_size = 32;
				b_size = 32;
				break;
			case 16:
				r_size = 32;
				g_size = 64;
				b_size = 32;
				break;
			case 24:
			default:
				r_size = 256;
				g_size = 256;
				b_size = 256;
				break;
		}

		r_val = kmalloc(r_size * sizeof(uint16_t), GFP_KERNEL);
		g_val = kmalloc(g_size * sizeof(uint16_t), GFP_KERNEL);
		b_val = kmalloc(b_size * sizeof(uint16_t), GFP_KERNEL);

		if (!r_val || !g_val || !b_val)
			return -ENOMEM;

		/* Set the color indices. */
		for (i = 0; i < r_size; i++) {
			r_val[i] = i << 8;
		}
		for (i = 0; i < g_size; i++) {
			g_val[i] = i << 8;
		}
		for (i = 0; i < b_size; i++) {
			b_val[i] = i << 8;
		}

		rval = crtc->lut->set(crtc, r_val, g_val, b_val);

		/* free before returning */
		kfree(r_val);
		kfree(g_val);
		kfree(b_val);

		if (rval != 0)
			return rval;
	}

	return 0;
}

int nv50_fb_create(struct nv50_crtc *crtc)
{
	if (!crtc)
		return -EINVAL;

	crtc->fb = kzalloc(sizeof(struct nv50_fb), GFP_KERNEL);

	crtc->fb->bind = nv50_fb_bind;

	return 0;
}

int nv50_fb_destroy(struct nv50_crtc *crtc)
{
	if (!crtc)
		return -EINVAL;

	kfree(crtc->fb);
	crtc->fb = NULL;

	return 0;
}
