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

#define CURSOR_WIDTH 64
#define CURSOR_HEIGHT 64

static void radeon_lock_cursor(struct drm_crtc *crtc, bool lock)
{
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	uint32_t cur_lock;

	if (radeon_is_avivo(dev_priv)) {
		cur_lock = RADEON_READ(AVIVO_D1CUR_UPDATE + radeon_crtc->crtc_offset);
		if (lock)
			cur_lock |= AVIVO_D1CURSOR_UPDATE_LOCK;
		else
			cur_lock &= ~AVIVO_D1CURSOR_UPDATE_LOCK;
		RADEON_WRITE(AVIVO_D1CUR_UPDATE + radeon_crtc->crtc_offset, cur_lock);
	} else {
		switch(radeon_crtc->crtc_id) {
		case 0:
			cur_lock = RADEON_READ(RADEON_CUR_OFFSET);
			if (lock)
				cur_lock |= RADEON_CUR_LOCK;
			else
				cur_lock &= ~RADEON_CUR_LOCK;
			RADEON_WRITE(RADEON_CUR_OFFSET, cur_lock);
			break;
		case 1:
			cur_lock = RADEON_READ(RADEON_CUR2_OFFSET);
			if (lock)
				cur_lock |= RADEON_CUR2_LOCK;
			else
				cur_lock &= ~RADEON_CUR2_LOCK;
			RADEON_WRITE(RADEON_CUR2_OFFSET, cur_lock);
			break;
		default:
			break;
		}
	}
}

static void radeon_hide_cursor(struct drm_crtc *crtc)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;

	if (radeon_is_avivo(dev_priv)) {
		RADEON_WRITE(RADEON_MM_INDEX, AVIVO_D1CUR_CONTROL + radeon_crtc->crtc_offset);
		RADEON_WRITE_P(RADEON_MM_DATA, 0, ~AVIVO_D1CURSOR_EN);
	} else {
		switch(radeon_crtc->crtc_id) {
		case 0:
			RADEON_WRITE(RADEON_MM_INDEX, RADEON_CRTC_GEN_CNTL);
			break;
		case 1:
			RADEON_WRITE(RADEON_MM_INDEX, RADEON_CRTC2_GEN_CNTL);
			break;
		default:
			return;
		}
		RADEON_WRITE_P(RADEON_MM_DATA, 0, ~RADEON_CRTC_CUR_EN);
	}
}

static void radeon_show_cursor(struct drm_crtc *crtc)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;

	if (radeon_is_avivo(dev_priv)) {
		RADEON_WRITE(RADEON_MM_INDEX, AVIVO_D1CUR_CONTROL + radeon_crtc->crtc_offset);
		RADEON_WRITE(RADEON_MM_DATA, AVIVO_D1CURSOR_EN |
			     (AVIVO_D1CURSOR_MODE_24BPP << AVIVO_D1CURSOR_MODE_SHIFT));
	} else {
		switch(radeon_crtc->crtc_id) {
		case 0:
			RADEON_WRITE(RADEON_MM_INDEX, RADEON_CRTC_GEN_CNTL);
			break;
		case 1:
			RADEON_WRITE(RADEON_MM_INDEX, RADEON_CRTC2_GEN_CNTL);
			break;
		default:
			return;
		}

		RADEON_WRITE_P(RADEON_MM_DATA, (RADEON_CRTC_CUR_EN |
						(RADEON_CRTC_CUR_MODE_24BPP << RADEON_CRTC_CUR_MODE_SHIFT)),
			       ~(RADEON_CRTC_CUR_EN | RADEON_CRTC_CUR_MODE_MASK));
	}
}

static void radeon_set_cursor(struct drm_crtc *crtc, struct drm_gem_object *obj,
			      uint32_t width, uint32_t height)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;
	struct drm_radeon_gem_object *obj_priv;

	obj_priv = obj->driver_private;

	if (radeon_is_avivo(dev_priv)) {
		RADEON_WRITE(AVIVO_D1CUR_SURFACE_ADDRESS + radeon_crtc->crtc_offset,
			     dev_priv->fb_location + obj_priv->bo->offset);
		RADEON_WRITE(AVIVO_D1CUR_SIZE + radeon_crtc->crtc_offset,
			     (width - 1) << 16 | (height - 1));
	} else {
		switch(radeon_crtc->crtc_id) {
		case 0:
			/* offset is from DISP_BASE_ADDRESS */
			RADEON_WRITE(RADEON_CUR_OFFSET, obj_priv->bo->offset);
			break;
		case 1:
			/* offset is from DISP2_BASE_ADDRESS */
			RADEON_WRITE(RADEON_CUR2_OFFSET, obj_priv->bo->offset);
			break;
		default:
			break;
		}
	}
}

int radeon_crtc_cursor_set(struct drm_crtc *crtc,
			   struct drm_file *file_priv,
			   uint32_t handle,
			   uint32_t width,
			   uint32_t height)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_gem_object *obj;

	if (!handle) {
		/* turn off cursor */
		radeon_hide_cursor(crtc);
		return 0;
	}

	obj = drm_gem_object_lookup(crtc->dev, file_priv, handle);
	if (!obj) {
		DRM_ERROR("Cannot find cursor object %x for crtc %d\n", handle, radeon_crtc->crtc_id);
		return -EINVAL;
	}

	if ((width > CURSOR_WIDTH) || (height > CURSOR_HEIGHT)) {
		DRM_ERROR("bad cursor width or height %d x %d\n", width, height);
		return -EINVAL;
	}

	radeon_lock_cursor(crtc, true);
	// XXX only 27 bit offset for legacy cursor
	radeon_set_cursor(crtc, obj, width, height);
	radeon_show_cursor(crtc);
	radeon_lock_cursor(crtc, false);

	mutex_lock(&crtc->dev->struct_mutex);
	drm_gem_object_unreference(obj);
	mutex_unlock(&crtc->dev->struct_mutex);

	return 0;
}

int radeon_crtc_cursor_move(struct drm_crtc *crtc,
			    int x, int y)
{
	struct radeon_crtc *radeon_crtc = to_radeon_crtc(crtc);
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;
	int xorigin = 0, yorigin = 0;

	if (x < 0)
		xorigin = -x + 1;
	if (y < 0)
		yorigin = -x + 1;
	if (xorigin >= CURSOR_WIDTH)
		xorigin = CURSOR_WIDTH - 1;
	if (yorigin >= CURSOR_WIDTH)
		yorigin = CURSOR_WIDTH - 1;

	radeon_lock_cursor(crtc, true);
	if (radeon_is_avivo(dev_priv)) {
		RADEON_WRITE(AVIVO_D1CUR_POSITION + radeon_crtc->crtc_offset,
			     ((xorigin ? 0: x) << 16) |
			     (yorigin ? 0 : y));
		RADEON_WRITE(AVIVO_D1CUR_HOT_SPOT + radeon_crtc->crtc_offset, (xorigin << 16) | yorigin);
	} else {
		if (crtc->mode.flags & DRM_MODE_FLAG_INTERLACE)
			y /= 2;
		else if (crtc->mode.flags & DRM_MODE_FLAG_DBLSCAN)
			y *= 2;

		switch(radeon_crtc->crtc_id) {
		case 0:
			RADEON_WRITE(RADEON_CUR_HORZ_VERT_OFF,  (RADEON_CUR_LOCK
								 | (xorigin << 16)
								 | yorigin));
			RADEON_WRITE(RADEON_CUR_HORZ_VERT_POSN, (RADEON_CUR_LOCK
								 | ((xorigin ? 0 : x) << 16)
								 | (yorigin ? 0 : y)));
			break;
		case 1:
			RADEON_WRITE(RADEON_CUR2_HORZ_VERT_OFF,  (RADEON_CUR2_LOCK
								 | (xorigin << 16)
								 | yorigin));
			RADEON_WRITE(RADEON_CUR2_HORZ_VERT_POSN, (RADEON_CUR2_LOCK
								 | ((xorigin ? 0 : x) << 16)
								 | (yorigin ? 0 : y)));
			break;
		default:
			break;
		}

	}
	radeon_lock_cursor(crtc, false);

	return 0;
}

