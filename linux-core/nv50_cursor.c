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

#include "nv50_cursor.h"
#include "nv50_crtc.h"
#include "nv50_display.h"

static int nv50_cursor_enable(struct nv50_crtc *crtc)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;

	NV50_DEBUG("\n");

	NV_WRITE(NV50_PDISPLAY_CURSOR_CURSOR_CTRL2(crtc->index), 0x2000);
	while(NV_READ(NV50_PDISPLAY_CURSOR_CURSOR_CTRL2(crtc->index)) & NV50_PDISPLAY_CURSOR_CURSOR_CTRL2_STATUS_MASK);

	NV_WRITE(NV50_PDISPLAY_CURSOR_CURSOR_CTRL2(crtc->index), NV50_PDISPLAY_CURSOR_CURSOR_CTRL2_ON);
	while((NV_READ(NV50_PDISPLAY_CURSOR_CURSOR_CTRL2(crtc->index)) & NV50_PDISPLAY_CURSOR_CURSOR_CTRL2_STATUS_MASK)
		!= NV50_PDISPLAY_CURSOR_CURSOR_CTRL2_STATUS_ACTIVE);

	crtc->cursor->enabled = true;

	return 0;
}

static int nv50_cursor_disable(struct nv50_crtc *crtc)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;

	NV50_DEBUG("\n");

	NV_WRITE(NV50_PDISPLAY_CURSOR_CURSOR_CTRL2(crtc->index), 0);
	while(NV_READ(NV50_PDISPLAY_CURSOR_CURSOR_CTRL2(crtc->index)) & NV50_PDISPLAY_CURSOR_CURSOR_CTRL2_STATUS_MASK);

	crtc->cursor->enabled = false;

	return 0;
}

/* Calling update or changing the stored cursor state is left to the higher level ioctl's. */
static int nv50_cursor_show(struct nv50_crtc *crtc)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;
	uint32_t offset = crtc->index * 0x400;

	NV50_DEBUG("\n");

	/* Better not show the cursor when we have none. */
	/* TODO: is cursor offset actually set? */
	if (!crtc->cursor->block) {
		DRM_ERROR("No cursor available on crtc %d\n", crtc->index);
		return -EINVAL;
	}

	OUT_MODE(NV50_CRTC0_CURSOR_CTRL + offset, NV50_CRTC0_CURSOR_CTRL_SHOW);

	return 0;
}

static int nv50_cursor_hide(struct nv50_crtc *crtc)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;
	uint32_t offset = crtc->index * 0x400;

	NV50_DEBUG("\n");

	OUT_MODE(NV50_CRTC0_CURSOR_CTRL + offset, NV50_CRTC0_CURSOR_CTRL_HIDE);

	return 0;
}

static int nv50_cursor_set_pos(struct nv50_crtc *crtc, int x, int y)
{
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;

	NV_WRITE(NV50_HW_CURSOR_POS(crtc->index), ((y & 0xFFFF) << 16) | (x & 0xFFFF));
	/* Needed to make the cursor move. */
	NV_WRITE(NV50_HW_CURSOR_POS_CTRL(crtc->index), 0);

	return 0;
}

static int nv50_cursor_set_bo(struct nv50_crtc *crtc, drm_handle_t handle)
{
	struct mem_block *block = NULL;
	struct drm_nouveau_private *dev_priv = crtc->dev->dev_private;

	NV50_DEBUG("\n");

	block = find_block_by_handle(dev_priv->fb_heap, handle);

	if (block) {
		bool first_time = false;
		if (!crtc->cursor->block)
			first_time = true;

		crtc->cursor->block = block;

		/* set the cursor offset cursor */
		if (first_time) {
			OUT_MODE(NV50_CRTC0_CURSOR_OFFSET + crtc->index * 0x400, crtc->cursor->block->start >> 8);
			if (crtc->cursor->visible)
				crtc->cursor->show(crtc);
		}
	} else {
		DRM_ERROR("Unable to find cursor bo with handle 0x%X\n", handle);
		return -EINVAL;
	}

	return 0;
}

int nv50_cursor_create(struct nv50_crtc *crtc)
{
	NV50_DEBUG("\n");

	if (!crtc)
		return -EINVAL;

	crtc->cursor = kzalloc(sizeof(struct nv50_cursor), GFP_KERNEL);
	if (!crtc->cursor)
		return -ENOMEM;

	/* function pointers */
	crtc->cursor->show = nv50_cursor_show;
	crtc->cursor->hide = nv50_cursor_hide;
	crtc->cursor->set_pos = nv50_cursor_set_pos;
	crtc->cursor->set_bo = nv50_cursor_set_bo;
	crtc->cursor->enable = nv50_cursor_enable;
	crtc->cursor->disable = nv50_cursor_disable;

	return 0;
}

int nv50_cursor_destroy(struct nv50_crtc *crtc)
{
	int rval = 0;

	NV50_DEBUG("\n");

	if (!crtc)
		return -EINVAL;

	if (crtc->cursor->enabled) {
		rval = crtc->cursor->disable(crtc);
		if (rval != 0)
			return rval;
	}

	kfree(crtc->cursor);
	crtc->cursor = NULL;

	return 0;
}
