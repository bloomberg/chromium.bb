/*
 * Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

extern struct backend backend_cirrus;
extern struct backend backend_evdi;
#ifdef DRV_EXYNOS
extern struct backend backend_exynos;
#endif
extern struct backend backend_gma500;
#ifdef DRV_I915
extern struct backend backend_i915;
#endif
#ifdef DRV_MARVELL
extern struct backend backend_marvell;
#endif
#ifdef DRV_MEDIATEK
extern struct backend backend_mediatek;
#endif
#ifdef DRV_ROCKCHIP
extern struct backend backend_rockchip;
#endif
#ifdef DRV_TEGRA
extern struct backend backend_tegra;
#endif
extern struct backend backend_udl;
extern struct backend backend_virtio_gpu;

static struct backend *drv_get_backend(int fd)
{
	drmVersionPtr drm_version;
	unsigned int i;

	drm_version = drmGetVersion(fd);

	if (!drm_version)
		return NULL;

	struct backend *backend_list[] = {
		&backend_cirrus,
		&backend_evdi,
#ifdef DRV_EXYNOS
		&backend_exynos,
#endif
		&backend_gma500,
#ifdef DRV_I915
		&backend_i915,
#endif
#ifdef DRV_MARVELL
		&backend_marvell,
#endif
#ifdef DRV_MEDIATEK
		&backend_mediatek,
#endif
#ifdef DRV_ROCKCHIP
		&backend_rockchip,
#endif
#ifdef DRV_TEGRA
		&backend_tegra,
#endif
		&backend_udl,
		&backend_virtio_gpu,
	};

	for(i = 0; i < ARRAY_SIZE(backend_list); i++)
		if (!strcmp(drm_version->name, backend_list[i]->name)) {
			drmFreeVersion(drm_version);
			return backend_list[i];
		}

	drmFreeVersion(drm_version);
	return NULL;
}

struct driver *drv_create(int fd)
{
	struct driver *drv;
	int ret;

	drv = (struct driver *) calloc(1, sizeof(*drv));

	if (!drv)
		return NULL;

	drv->fd = fd;
	drv->backend = drv_get_backend(fd);

	if (!drv->backend) {
		free(drv);
		return NULL;
	}

	if (pthread_mutex_init(&drv->table_lock, NULL)) {
		free(drv);
		return NULL;
	}

	drv->buffer_table = drmHashCreate();
	if (!drv->buffer_table) {
		free(drv);
		return NULL;
	}

	if (drv->backend->init) {
		ret = drv->backend->init(drv);
		if (ret) {
			free(drv);
			return NULL;
		}
	}

	return drv;
}

void drv_destroy(struct driver *drv)
{
	if (drv->backend->close)
		drv->backend->close(drv);

	pthread_mutex_destroy(&drv->table_lock);
	drmHashDestroy(drv->buffer_table);

	free(drv);
}

int drv_get_fd(struct driver *drv)
{
	return drv->fd;
}

const char *
drv_get_name(struct driver *drv)
{
	return drv->backend->name;
}

int drv_is_format_supported(struct driver *drv, drv_format_t format,
			    uint64_t usage)
{
	unsigned int i;

	if (format == DRV_FORMAT_NONE || usage == DRV_BO_USE_NONE)
		return 0;

	for (i = 0 ; i < ARRAY_SIZE(drv->backend->format_list); i++)
	{
		if (!drv->backend->format_list[i].format)
			break;

		if (drv->backend->format_list[i].format == format &&
		      (drv->backend->format_list[i].usage & usage) == usage)
			return 1;
	}

	return 0;
}

struct bo *drv_bo_new(struct driver *drv, uint32_t width, uint32_t height,
		      drv_format_t format)
{

	struct bo *bo;
	bo = (struct bo *) calloc(1, sizeof(*bo));

	if (!bo)
		return NULL;

	bo->drv = drv;
	bo->width = width;
	bo->height = height;
	bo->format = format;
	bo->num_planes = drv_num_planes_from_format(format);

	if (!bo->num_planes) {
		free(bo);
		return NULL;
	}

	return bo;
}

struct bo *drv_bo_create(struct driver *drv, uint32_t width, uint32_t height,
			 drv_format_t format, uint64_t flags)
{
	int ret;
	size_t plane;
	struct bo *bo;

	bo = drv_bo_new(drv, width, height, format);

	if (!bo)
		return NULL;

	ret = drv->backend->bo_create(bo, width, height, format, flags);

	if (ret) {
		free(bo);
		return NULL;
	}

	pthread_mutex_lock(&drv->table_lock);

	for (plane = 0; plane < bo->num_planes; plane++)
		drv_increment_reference_count(drv, bo, plane);

	pthread_mutex_unlock(&drv->table_lock);

	return bo;
}

void drv_bo_destroy(struct bo *bo)
{
	size_t plane;
	uintptr_t total = 0;
	struct driver *drv = bo->drv;

	pthread_mutex_lock(&drv->table_lock);

	for (plane = 0; plane < bo->num_planes; plane++)
		drv_decrement_reference_count(drv, bo, plane);

	for (plane = 0; plane < bo->num_planes; plane++)
		total += drv_get_reference_count(drv, bo, plane);

	pthread_mutex_unlock(&drv->table_lock);

	if (total == 0)
		bo->drv->backend->bo_destroy(bo);

	free(bo);
}

void *
drv_bo_map(struct bo *bo)
{
	assert(drv_num_buffers_per_bo(bo) == 1);

	if (!bo->map_data || bo->map_data == MAP_FAILED)
		bo->map_data = bo->drv->backend->bo_map(bo);

	return bo->map_data;
}

int
drv_bo_unmap(struct bo *bo)
{
	int ret = -1;

	assert(drv_num_buffers_per_bo(bo) == 1);

	if (bo->map_data && bo->map_data != MAP_FAILED)
		ret = munmap(bo->map_data, bo->total_size);

	bo->map_data = NULL;

	return ret;
}

struct bo *drv_bo_import(struct driver *drv, struct drv_import_fd_data *data)
{
	int ret;
	size_t plane;
	struct bo *bo;
	struct drm_prime_handle prime_handle;

	bo = drv_bo_new(drv, data->width, data->height, data->format);

	if (!bo)
		return NULL;

	for (plane = 0; plane < bo->num_planes; plane++) {

		memset(&prime_handle, 0, sizeof(prime_handle));
		prime_handle.fd = data->fds[plane];

		ret = drmIoctl(drv->fd, DRM_IOCTL_PRIME_FD_TO_HANDLE,
			       &prime_handle);

		if (ret) {
			fprintf(stderr, "drv: DRM_IOCTL_PRIME_FD_TO_HANDLE failed "
				"(fd=%u)\n", prime_handle.fd);

			if (plane > 0) {
				bo->num_planes = plane;
				drv_bo_destroy(bo);
			} else {
				free(bo);
			}

			return NULL;
		}

		bo->handles[plane].u32 = prime_handle.handle;
		bo->strides[plane] = data->strides[plane];
		bo->offsets[plane] = data->offsets[plane];
		bo->sizes[plane] = data->sizes[plane];

		pthread_mutex_lock(&drv->table_lock);
		drv_increment_reference_count(drv, bo, plane);
		pthread_mutex_unlock(&drv->table_lock);
	}

	return bo;
}

uint32_t drv_bo_get_width(struct bo *bo)
{
	return bo->width;
}

uint32_t drv_bo_get_height(struct bo *bo)
{
	return bo->height;
}

uint32_t drv_bo_get_stride_or_tiling(struct bo *bo)
{
	return bo->tiling ? bo->tiling : drv_bo_get_plane_stride(bo, 0);
}

size_t drv_bo_get_num_planes(struct bo *bo)
{
	return bo->num_planes;
}

union bo_handle drv_bo_get_plane_handle(struct bo *bo, size_t plane)
{
	return bo->handles[plane];
}

#ifndef DRM_RDWR
#define DRM_RDWR O_RDWR
#endif

int drv_bo_get_plane_fd(struct bo *bo, size_t plane)
{

	int ret, fd;
	assert(plane < bo->num_planes);

	ret = drmPrimeHandleToFD(bo->drv->fd, bo->handles[plane].u32,
				 DRM_CLOEXEC | DRM_RDWR, &fd);

	return (ret) ? ret : fd;

}

uint32_t drv_bo_get_plane_offset(struct bo *bo, size_t plane)
{
	assert(plane < bo->num_planes);
	return bo->offsets[plane];
}

uint32_t drv_bo_get_plane_size(struct bo *bo, size_t plane)
{
	assert(plane < bo->num_planes);
	return bo->sizes[plane];
}

uint32_t drv_bo_get_plane_stride(struct bo *bo, size_t plane)
{
	assert(plane < bo->num_planes);
	return bo->strides[plane];
}

uint64_t drv_bo_get_plane_format_modifier(struct bo *bo, size_t plane)
{
        assert(plane < bo->num_planes);
	return bo->format_modifiers[plane];
}

drv_format_t drv_resolve_format(struct driver *drv, drv_format_t format)
{
	if (drv->backend->resolve_format)
		return drv->backend->resolve_format(format);

	return format;
}
