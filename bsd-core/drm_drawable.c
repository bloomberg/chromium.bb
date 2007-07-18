/* drm_drawable.h -- IOCTLs for drawables -*- linux-c -*-
 * Created: Tue Feb  2 08:37:54 1999 by faith@valinux.com
 */
/*-
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#include "drmP.h"

struct bsd_drm_drawable_info {
	struct drm_drawable_info info;
	int handle;
	RB_ENTRY(bsd_drm_drawable_info) tree;
};

static int
drm_drawable_compare(struct bsd_drm_drawable_info *a,
    struct bsd_drm_drawable_info *b)
{
	if (a->handle > b->handle)
		return 1;
	if (a->handle > b->handle)
		return -1;
	return 0;
}

RB_GENERATE_STATIC(drawable_tree, bsd_drm_drawable_info, tree,
    drm_drawable_compare);

struct drm_drawable_info *
drm_get_drawable_info(drm_device_t *dev, int handle)
{
	struct bsd_drm_drawable_info find, *result;

	find.handle = handle;
	result = RB_FIND(drawable_tree, &dev->drw_head, &find);

	return &result->info;
}

static struct drm_drawable_info *
drm_drawable_info_alloc(drm_device_t *dev, int handle)
{
	struct bsd_drm_drawable_info *info;

	info = drm_calloc(1, sizeof(struct bsd_drm_drawable_info),
	    DRM_MEM_DRAWABLE);
	if (info == NULL)
		return NULL;

	info->handle = handle;
	RB_INSERT(drawable_tree, &dev->drw_head, info);

	return &info->info;
}

static void
drm_drawable_info_free(drm_device_t *dev, struct drm_drawable_info *info)
{
	RB_REMOVE(drawable_tree, &dev->drw_head,
	    (struct bsd_drm_drawable_info *)info);
	drm_free(info, sizeof(struct bsd_drm_drawable_info), DRM_MEM_DRAWABLE);
}

int drm_adddraw(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_draw_t draw;

	draw.handle = alloc_unr(dev->drw_unrhdr);
	DRM_DEBUG("%d\n", draw.handle);

	DRM_COPY_TO_USER_IOCTL((drm_draw_t *)data, draw, sizeof(draw));

	return 0;
}

int drm_rmdraw(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_draw_t *draw = (drm_draw_t *)data;
	struct drm_drawable_info *info;

	free_unr(dev->drw_unrhdr, draw->handle);

	info = drm_get_drawable_info(dev, draw->handle);
	if (info != NULL) {
		drm_drawable_info_free(dev, info);
	}

	return 0;
}

int drm_update_draw(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	struct drm_drawable_info *info;
	struct drm_update_draw *update = (struct drm_update_draw *)data;

	info = drm_get_drawable_info(dev, update->handle);
	if (info == NULL) {
		info = drm_drawable_info_alloc(dev, update->handle);
		if (info == NULL)
			return ENOMEM;
	}

	switch (update->type) {
	case DRM_DRAWABLE_CLIPRECTS:
		DRM_SPINLOCK(&dev->drw_lock);
		if (update->num != info->num_rects) {
			drm_free(info->rects,
			    sizeof(*info->rects) * info->num_rects,
			    DRM_MEM_DRAWABLE);
			info->rects = NULL;
			info->num_rects = 0;
		}
		if (update->num == 0) {
			DRM_SPINUNLOCK(&dev->drw_lock);
			return 0;
		}
		if (info->rects == NULL) {
			info->rects = drm_alloc(sizeof(*info->rects) *
			    update->num, DRM_MEM_DRAWABLE);
			if (info->rects == NULL)
				return ENOMEM;
			info->num_rects = update->num;
		}
		/* For some reason the pointer arg is unsigned long long. */
		copyin((void *)(intptr_t)update->data, info->rects,
		    sizeof(*info->rects) * info->num_rects);
		DRM_SPINUNLOCK(&dev->drw_lock);
		return 0;
	default:
		return EINVAL;
	}
}
