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

#include "nv50_lut.h"
#include "nv50_fb.h"
#include "nv50_crtc.h"
#include "nv50_display.h"

static int nv50_lut_alloc(struct nv50_crtc *crtc)
{
	struct mem_block *block;
	struct drm_file *file_priv = kzalloc(sizeof(struct drm_file), GFP_KERNEL);
	uint32_t flags = NOUVEAU_MEM_INTERNAL | NOUVEAU_MEM_FB | NOUVEAU_MEM_MAPPED;

	NV50_DEBUG("\n");

	if (!file_priv)
		return -ENOMEM;

	/* Any file_priv should do as it's pointer is used as identification. */
	block = nouveau_mem_alloc(crtc->dev, 0, 4096, flags, file_priv);

	if (!block)
		return -ENOMEM;

	crtc->lut->block = block;

	return 0;
}

static int nv50_lut_free(struct nv50_crtc *crtc)
{
	struct drm_file *file_priv = crtc->lut->block->file_priv;

	NV50_DEBUG("\n");

	nouveau_mem_free(crtc->dev, crtc->lut->block);

	kfree(file_priv);

	return 0;
}

#define NV50_LUT_INDEX(val, w) ((val << (8 - w)) | (val >> ((w << 1) - 8)))
static int nv50_lut_set(struct nv50_crtc *crtc, uint16_t *red, uint16_t *green, uint16_t *blue)
{
	uint32_t index = 0, i;
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;
	void __iomem *lut = NULL;

	NV50_DEBUG("\n");

	if (!crtc->lut || !crtc->lut->block) {
		DRM_ERROR("Something wrong with the LUT\n");
		return -EINVAL;
	}

	/* 16 bits, red, green, blue, unused, total of 64 bits per index */
	/* maybe switch to ioremap_wc once it becomes available. */
	lut = ioremap(dev_priv->fb_phys + crtc->lut->block->start, crtc->lut->block->size);
	if (!lut) {
		DRM_ERROR("ioremap failed on LUT\n");
		return -EINVAL;
	}

	/* 10 bits lut, with 14 bits values. */
	switch (crtc->fb->depth) {
		case 15:
			/* R5G5B5 */
			for (i = 0; i < 32; i++) {
				index = NV50_LUT_INDEX(i, 5);
				writew(red[i] >> 2, lut + 8*index + 0);
				writew(green[i] >> 2, lut + 8*index + 2);
				writew(blue[i] >> 2, lut + 8*index + 4);
			}
			break;
		case 16:
			/* R5G6B5 */
			for (i = 0; i < 32; i++) {
				index = NV50_LUT_INDEX(i, 5);
				writew(red[i] >> 2, lut + 8*index + 0);
				writew(blue[i] >> 2, lut + 8*index + 4);
			}

			/* Green has an extra bit. */
			for (i = 0; i < 64; i++) {
				index = NV50_LUT_INDEX(i, 6);
				writew(green[i] >> 2, lut + 8*index + 2);
			}
			break;
		default:
			/* R8G8B8 */
			for (i = 0; i < 256; i++) {
				writew(red[i] >> 2, lut + 8*i + 0);
				writew(green[i] >> 2, lut + 8*i + 2);
				writew(blue[i] >> 2, lut + 8*i + 4);
			}
			break;
	}

	crtc->lut->depth = crtc->fb->depth;

	/* Cleaning time. */
	iounmap(lut);

	return 0;
}

int nv50_lut_create(struct nv50_crtc *crtc)
{
	int rval = 0;

	NV50_DEBUG("\n");

	if (!crtc)
		return -EINVAL;

	crtc->lut = kzalloc(sizeof(struct nv50_lut), GFP_KERNEL);

	if (!crtc->lut)
		return -ENOMEM;

	rval = nv50_lut_alloc(crtc);
	if (rval != 0)
		return rval;

	/* lut will be inited when fb is bound */
	crtc->lut->depth = 0;

	/* function pointers */
	crtc->lut->set = nv50_lut_set;

	return 0;
}

int nv50_lut_destroy(struct nv50_crtc *crtc)
{
	int rval = 0;

	NV50_DEBUG("\n");

	if (!crtc)
		return -EINVAL;

	rval = nv50_lut_free(crtc);

	kfree(crtc->lut);
	crtc->lut = NULL;

	if (rval != 0)
		return rval;

	return 0;
}
