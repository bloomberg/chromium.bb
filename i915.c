/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef GBM_I915

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <xf86drm.h>
#include <i915_drm.h>

#include "gbm_priv.h"
#include "helpers.h"
#include "util.h"

struct gbm_i915_device
{
	int gen;
};


static int get_gen(int device_id)
{
	const uint16_t gen3_ids[] = {0x2582, 0x2592, 0x2772, 0x27A2, 0x27AE, 0x29C2, 0x29B2, 0x29D2, 0xA001, 0xA011};
	unsigned i;
	for(i = 0; i < ARRAY_SIZE(gen3_ids); i++)
		if (gen3_ids[i] == device_id)
			return 3;

	return 4;
}

int gbm_i915_init(struct gbm_device *gbm)
{
	struct gbm_i915_device *i915_gbm;
	drm_i915_getparam_t get_param;
	int device_id;
	int ret;

	i915_gbm = (struct gbm_i915_device*)malloc(sizeof(*i915_gbm));
	if (!i915_gbm)
		return -1;

	memset(&get_param, 0, sizeof(get_param));
	get_param.param = I915_PARAM_CHIPSET_ID;
	get_param.value = &device_id;
	ret = drmIoctl(gbm->fd, DRM_IOCTL_I915_GETPARAM, &get_param);
	if (ret) {
		free(i915_gbm);
		return -1;
	}

	i915_gbm->gen = get_gen(device_id);

	gbm->priv = i915_gbm;

	return 0;
}

void gbm_i915_close(struct gbm_device *gbm)
{
	free(gbm->priv);
	gbm->priv = NULL;
}

static int i915_get_pitch(struct gbm_device *gbm, uint32_t pitch, int tiling_mode)
{
	struct gbm_i915_device *dev_priv = (struct gbm_i915_device *)gbm->priv;
	if (tiling_mode == I915_TILING_NONE)
		return ALIGN(pitch, 64);

	if (dev_priv->gen >= 4) {
		uint32_t tile_width = tiling_mode == I915_TILING_X ? 512 : 128;
		return ALIGN(pitch, tile_width);
	} else {
		uint32_t i = 128;
		while(i < pitch)
			i <<= 1;
		return i;
	}
}

int gbm_i915_bo_create(struct gbm_bo *bo, uint32_t width, uint32_t height, uint32_t format, uint32_t flags)
{
	size_t size = width * height * gbm_bytes_from_format(format);
	struct drm_i915_gem_create gem_create;
	struct drm_i915_gem_set_tiling gem_set_tiling;
	uint32_t tiling_mode = I915_TILING_NONE;
	int ret;

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = size;

	ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_I915_GEM_CREATE, &gem_create);
	if (ret)
		return ret;

	bo->handle.u32 = gem_create.handle;
	bo->size = size;

	if (flags & GBM_BO_USE_CURSOR)
		tiling_mode = I915_TILING_NONE;
	else if (flags & GBM_BO_USE_SCANOUT)
		tiling_mode = I915_TILING_X;
	else if (flags & GBM_BO_USE_RENDERING)
		tiling_mode = I915_TILING_Y;

	bo->stride = i915_get_pitch(bo->gbm,
		width * gbm_bytes_from_format(format),
		tiling_mode);

	memset(&gem_set_tiling, 0, sizeof(gem_set_tiling));
	do {
		gem_set_tiling.handle = bo->handle.u32;
		gem_set_tiling.tiling_mode = tiling_mode;
		gem_set_tiling.stride = bo->stride;
		ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_I915_GEM_SET_TILING, &gem_set_tiling);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));

	if (ret == -1) {
		struct drm_gem_close gem_close;
		gem_close.handle = bo->handle.u32;
		drmIoctl(bo->gbm->fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
		return -errno;
	}

	return 0;
}

const struct gbm_driver gbm_driver_i915 =
{
	.name = "i915",
	.init = gbm_i915_init,
	.close = gbm_i915_close,
	.bo_create = gbm_i915_bo_create,
	.bo_destroy = gbm_gem_bo_destroy,
	.format_list = {
		{GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING | GBM_BO_USE_WRITE},
		{GBM_FORMAT_ARGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING | GBM_BO_USE_WRITE},
		{GBM_FORMAT_XBGR8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING | GBM_BO_USE_WRITE},
		{GBM_FORMAT_ABGR8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING | GBM_BO_USE_WRITE},
		{GBM_FORMAT_XRGB1555, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING | GBM_BO_USE_WRITE},
		{GBM_FORMAT_ARGB1555, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING | GBM_BO_USE_WRITE},
		{GBM_FORMAT_RGB565,   GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING | GBM_BO_USE_WRITE},
	}
};

#endif
