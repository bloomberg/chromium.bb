/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <xf86drm.h>

#include "gbm_priv.h"
#include "util.h"

/*
gbm_buffer_base.cc:  gbm_bo_get_width(bo),
gbm_buffer_base.cc:  gbm_bo_get_height(bo),
gbm_buffer_base.cc:  gbm_bo_get_stride(bo),
gbm_buffer_base.cc:  gbm_bo_get_handle(bo).u32,
gbm_buffer_base.cc:  return gbm_bo_get_handle(bo_).u32;
gbm_buffer_base.cc:  return gfx::Size(gbm_bo_get_width(bo_), gbm_bo_get_height(bo_));
gbm_buffer.cc:    gbm_bo_destroy(bo());
gbm_buffer.cc:  gbm_bo* bo = gbm_bo_create(device,
gbm_surface.cc:    gbm_bo_set_user_data(bo, this, GbmSurfaceBuffer::Destroy);
gbm_surface.cc:      static_cast<GbmSurfaceBuffer*>(gbm_bo_get_user_data(buffer)));
gbm_surface.cc:    gbm_surface_release_buffer(native_surface_, current_buffer_);
gbm_surface.cc:    gbm_surface_destroy(native_surface_);
gbm_surface.cc:  native_surface_ = gbm_surface_create(
gbm_surface.cc:  gbm_bo* pending_buffer = gbm_surface_lock_front_buffer(native_surface_);
gbm_surface.cc:    gbm_surface_release_buffer(native_surface_, current_buffer_);
ozone_platform_gbm.cc:        device_(gbm_create_device(dri_->get_fd())) {}
ozone_platform_gbm.cc:    gbm_device_destroy(device_);
*/

extern struct gbm_driver gbm_driver_cirrus;
#ifdef GBM_EXYNOS
extern struct gbm_driver gbm_driver_exynos;
#endif
extern struct gbm_driver gbm_driver_gma500;
#ifdef GBM_I915
extern struct gbm_driver gbm_driver_i915;
#endif
#ifdef GBM_MEDIATEK
extern struct gbm_driver gbm_driver_mediatek;
#endif
#ifdef GBM_ROCKCHIP
extern struct gbm_driver gbm_driver_rockchip;
#endif
#ifdef GBM_TEGRA
extern struct gbm_driver gbm_driver_tegra;
#endif
extern struct gbm_driver gbm_driver_udl;

static struct gbm_driver *gbm_get_driver(int fd)
{
	drmVersionPtr drm_version;
	unsigned int i;

	drm_version = drmGetVersion(fd);

	if (!drm_version)
		return NULL;

	struct gbm_driver *driver_list[] = {
		&gbm_driver_cirrus,
#ifdef GBM_EXYNOS
		&gbm_driver_exynos,
#endif
		&gbm_driver_gma500,
#ifdef GBM_I915
		&gbm_driver_i915,
#endif
#ifdef GBM_MEDIATEK
		&gbm_driver_mediatek,
#endif
#ifdef GBM_ROCKCHIP
		&gbm_driver_rockchip,
#endif
#ifdef GBM_TEGRA
		&gbm_driver_tegra,
#endif
		&gbm_driver_udl,
	};

	for(i = 0; i < ARRAY_SIZE(driver_list); i++)
		if (!strcmp(drm_version->name, driver_list[i]->name))
		{
			drmFreeVersion(drm_version);
			return driver_list[i];
		}

	drmFreeVersion(drm_version);
	return NULL;
}

PUBLIC int
gbm_device_get_fd(struct gbm_device *gbm)
{
	return gbm->fd;
}

PUBLIC const char *
gbm_device_get_backend_name(struct gbm_device *gbm)
{
	return gbm->driver->name;
}

PUBLIC int
gbm_device_is_format_supported(struct gbm_device *gbm,
                             uint32_t format, uint32_t usage)
{
	unsigned i;

	if (format == GBM_BO_FORMAT_XRGB8888)
		format = GBM_FORMAT_XRGB8888;
	if (format == GBM_BO_FORMAT_ARGB8888)
		format = GBM_FORMAT_ARGB8888;

	if (usage & GBM_BO_USE_CURSOR &&
		usage & GBM_BO_USE_RENDERING)
		return 0;

	for(i = 0 ; i < ARRAY_SIZE(gbm->driver->format_list); i++)
	{
		if (!gbm->driver->format_list[i].format)
			break;

		if (gbm->driver->format_list[i].format == format &&
			(gbm->driver->format_list[i].usage & usage) == usage)
			return 1;
	}

	return 0;
}

PUBLIC struct gbm_device *gbm_create_device(int fd)
{
	struct gbm_device *gbm;
	int ret;

	gbm = (struct gbm_device*) malloc(sizeof(*gbm));
	if (!gbm)
		return NULL;

	gbm->fd = fd;

	gbm->driver = gbm_get_driver(fd);
	if (!gbm->driver) {
		free(gbm);
		return NULL;
	}

	if (gbm->driver->init) {
		ret = gbm->driver->init(gbm);
		if (ret) {
			free(gbm);
			return NULL;
		}
	}

	return gbm;
}

PUBLIC void gbm_device_destroy(struct gbm_device *gbm)
{
	if (gbm->driver->close)
		gbm->driver->close(gbm);
	free(gbm);
}

PUBLIC struct gbm_surface *gbm_surface_create(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags)
{
	struct gbm_surface *surface = (struct gbm_surface*) malloc(sizeof(*surface));
	if (!surface)
		return NULL;

	return surface;
}

PUBLIC void gbm_surface_destroy(struct gbm_surface *surface)
{
	free(surface);
}

PUBLIC struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surface)
{
	return NULL;
}

PUBLIC void gbm_surface_release_buffer(struct gbm_surface *surface, struct gbm_bo *bo)
{
}

PUBLIC struct gbm_bo *gbm_bo_create(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags)
{
	struct gbm_bo *bo;
	int ret;

	bo = (struct gbm_bo*) malloc(sizeof(*bo));
	if (!bo)
		return NULL;

	bo->gbm = gbm;
	bo->width = width;
	bo->height = height;
	bo->stride = 0;
	bo->format = format;
	bo->handle.u32 = 0;
	bo->destroy_user_data = NULL;
	bo->user_data = NULL;

	ret = gbm->driver->bo_create(bo, width, height, format, flags);
	if (ret) {
		free(bo);
		return NULL;
	}

	return bo;
}

PUBLIC void gbm_bo_destroy(struct gbm_bo *bo)
{
	if (bo->destroy_user_data) {
		bo->destroy_user_data(bo, bo->user_data);
		bo->destroy_user_data = NULL;
		bo->user_data = NULL;
	}

	bo->gbm->driver->bo_destroy(bo);
	free(bo);
}

PUBLIC uint32_t
gbm_bo_get_width(struct gbm_bo *bo)
{
	return bo->width;
}

PUBLIC uint32_t
gbm_bo_get_height(struct gbm_bo *bo)
{
	return bo->height;
}

PUBLIC uint32_t
gbm_bo_get_stride(struct gbm_bo *bo)
{
	return bo->stride;
}

PUBLIC uint32_t
gbm_bo_get_stride_or_tiling(struct gbm_bo *bo)
{
	return bo->tiling ? bo->tiling : bo->stride;
}

PUBLIC uint32_t
gbm_bo_get_format(struct gbm_bo *bo)
{
	return bo->format;
}

PUBLIC struct gbm_device *
gbm_bo_get_device(struct gbm_bo *bo)
{
	return bo->gbm;
}

PUBLIC union gbm_bo_handle
gbm_bo_get_handle(struct gbm_bo *bo)
{
	return bo->handle;
}

PUBLIC int
gbm_bo_get_fd(struct gbm_bo *bo)
{
	int fd;

	if (drmPrimeHandleToFD(gbm_device_get_fd(bo->gbm),
				gbm_bo_get_handle(bo).u32,
				DRM_CLOEXEC,
				&fd))
		return -1;
	else
		return fd;
}

PUBLIC void
gbm_bo_set_user_data(struct gbm_bo *bo, void *data,
		     void (*destroy_user_data)(struct gbm_bo *, void *))
{
	bo->user_data = data;
	bo->destroy_user_data = destroy_user_data;
}

PUBLIC void *
gbm_bo_get_user_data(struct gbm_bo *bo)
{
	return bo->user_data;
}
