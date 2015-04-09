/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <xf86drm.h>

#include "gbm_priv.h"
#include "helpers.h"

int gbm_bpp_from_format(uint32_t format)
{
	if (format == GBM_BO_FORMAT_XRGB8888)
		format = GBM_FORMAT_XRGB8888;
	if (format == GBM_BO_FORMAT_ARGB8888)
		format = GBM_FORMAT_ARGB8888;

	switch(format)
	{
		case GBM_FORMAT_C8:
		case GBM_FORMAT_RGB332:
		case GBM_FORMAT_BGR233:
			return 8;

		case GBM_FORMAT_XRGB4444:
		case GBM_FORMAT_XBGR4444:
		case GBM_FORMAT_RGBX4444:
		case GBM_FORMAT_BGRX4444:
		case GBM_FORMAT_ARGB4444:
		case GBM_FORMAT_ABGR4444:
		case GBM_FORMAT_RGBA4444:
		case GBM_FORMAT_BGRA4444:
		case GBM_FORMAT_XRGB1555:
		case GBM_FORMAT_XBGR1555:
		case GBM_FORMAT_RGBX5551:
		case GBM_FORMAT_BGRX5551:
		case GBM_FORMAT_ARGB1555:
		case GBM_FORMAT_ABGR1555:
		case GBM_FORMAT_RGBA5551:
		case GBM_FORMAT_BGRA5551:
		case GBM_FORMAT_RGB565:
		case GBM_FORMAT_BGR565:
			return 16;

		case GBM_FORMAT_RGB888:
		case GBM_FORMAT_BGR888:
			return 24;

		case GBM_FORMAT_XRGB8888:
		case GBM_FORMAT_XBGR8888:
		case GBM_FORMAT_RGBX8888:
		case GBM_FORMAT_BGRX8888:
		case GBM_FORMAT_ARGB8888:
		case GBM_FORMAT_ABGR8888:
		case GBM_FORMAT_RGBA8888:
		case GBM_FORMAT_BGRA8888:
		case GBM_FORMAT_XRGB2101010:
		case GBM_FORMAT_XBGR2101010:
		case GBM_FORMAT_RGBX1010102:
		case GBM_FORMAT_BGRX1010102:
		case GBM_FORMAT_ARGB2101010:
		case GBM_FORMAT_ABGR2101010:
		case GBM_FORMAT_RGBA1010102:
		case GBM_FORMAT_BGRA1010102:
		case GBM_FORMAT_YUYV:
		case GBM_FORMAT_YVYU:
		case GBM_FORMAT_UYVY:
		case GBM_FORMAT_VYUY:
		case GBM_FORMAT_AYUV:
			return 32;
	}

	printf("UNKNOWN FORMAT %d\n", format);
	return 0;
}

int gbm_bytes_from_format(uint32_t format)
{
	return gbm_bpp_from_format(format) / 8;
}

int gbm_is_format_supported(struct gbm_bo *bo)
{
	return 1;
}

int gbm_dumb_bo_create(struct gbm_bo *bo, uint32_t width, uint32_t height, uint32_t format, uint32_t flags)
{
	struct drm_mode_create_dumb create_dumb;
	int ret;

	memset(&create_dumb, 0, sizeof(create_dumb));
	create_dumb.height = height;
	create_dumb.width = width;
	create_dumb.bpp = gbm_bpp_from_format(format);
	create_dumb.flags = 0;

	ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
	if (ret) {
		fprintf(stderr, "minigbm: DRM_IOCTL_MODE_CREATE_DUMB failed "
				"(handle=%x)\n", bo->handle.u32);
		return ret;
	}

	bo->handle.u32 = create_dumb.handle;
	bo->size = create_dumb.size;
	bo->stride = create_dumb.pitch;

	return 0;
}

int gbm_dumb_bo_destroy(struct gbm_bo *bo)
{
	struct drm_mode_destroy_dumb destroy_dumb;
	int ret;

	memset(&destroy_dumb, 0, sizeof(destroy_dumb));
	destroy_dumb.handle = bo->handle.u32;

	ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
	if (ret) {
		fprintf(stderr, "minigbm: DRM_IOCTL_MODE_DESTROY_DUMB failed "
				"(handle=%x)\n", bo->handle.u32);
		return ret;
	}

	return 0;
}

int gbm_gem_bo_destroy(struct gbm_bo *bo)
{
	struct drm_gem_close gem_close;
	int ret;

	memset(&gem_close, 0, sizeof(gem_close));
	gem_close.handle = bo->handle.u32;

	ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
	if (ret) {
		fprintf(stderr, "minigbm: DRM_IOCTL_GEM_CLOSE failed "
				"(handle=%x)\n", bo->handle.u32);
		return ret;
	}

	return 0;
}
