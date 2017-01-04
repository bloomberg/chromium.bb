/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

int drv_bpp_from_format(uint32_t format, size_t plane)
{
	assert(plane < drv_num_planes_from_format(format));

	switch (format) {
	case DRM_FORMAT_BGR233:
	case DRM_FORMAT_C8:
	case DRM_FORMAT_R8:
	case DRM_FORMAT_RGB332:
	case DRM_FORMAT_YVU420:
		return 8;

	/*
	 * NV12 is laid out as follows. Each letter represents a byte.
	 * Y plane:
	 * Y0_0, Y0_1, Y0_2, Y0_3, ..., Y0_N
	 * Y1_0, Y1_1, Y1_2, Y1_3, ..., Y1_N
	 * ...
	 * YM_0, YM_1, YM_2, YM_3, ..., YM_N
	 * CbCr plane:
	 * Cb01_01, Cr01_01, Cb01_23, Cr01_23, ..., Cb01_(N-1)N, Cr01_(N-1)N
	 * Cb23_01, Cr23_01, Cb23_23, Cr23_23, ..., Cb23_(N-1)N, Cr23_(N-1)N
	 * ...
	 * Cb(M-1)M_01, Cr(M-1)M_01, ..., Cb(M-1)M_(N-1)N, Cr(M-1)M_(N-1)N
	 *
	 * Pixel (0, 0) requires Y0_0, Cb01_01 and Cr01_01. Pixel (0, 1) requires
	 * Y0_1, Cb01_01 and Cr01_01.  So for a single pixel, 2 bytes of luma data
	 * are required.
	 */
	case DRM_FORMAT_NV12:
		return (plane == 0) ? 8 : 16;

	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_GR88:
	case DRM_FORMAT_RG88:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		return 16;

	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_RGB888:
		return 24;

	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_AYUV:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX1010102:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_XRGB8888:
		return 32;
	}

	fprintf(stderr, "drv: UNKNOWN FORMAT %d\n", format);
	return 0;
}

/*
 * This function fills in the buffer object given driver aligned dimensions
 * and a format.  This function assumes there is just one kernel buffer per
 * buffer object.
 */
int drv_bo_from_format(struct bo *bo, uint32_t width, uint32_t height,
		       uint32_t format)
{

	size_t p, num_planes;
	uint32_t offset = 0;

	num_planes = drv_num_planes_from_format(format);
	assert(num_planes);

	for (p = 0; p < num_planes; p++) {
		bo->strides[p] = drv_stride_from_format(format, width, p);
		bo->sizes[p] = drv_size_from_format(format, bo->strides[p],
						    height, p);
		bo->offsets[p] = offset;
		offset += bo->sizes[p];
	}

	bo->total_size = bo->offsets[bo->num_planes - 1] +
			 bo->sizes[bo->num_planes - 1];

	return 0;
}

int drv_dumb_bo_create(struct bo *bo, uint32_t width, uint32_t height,
		       uint32_t format, uint32_t flags)
{
	struct drm_mode_create_dumb create_dumb;
	int ret;

	/* Only single-plane formats are supported */
	assert(drv_num_planes_from_format(format) == 1);

	memset(&create_dumb, 0, sizeof(create_dumb));
	create_dumb.height = height;
	create_dumb.width = width;
	create_dumb.bpp = drv_bpp_from_format(format, 0);
	create_dumb.flags = 0;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_MODE_CREATE_DUMB failed\n");
		return ret;
	}

	bo->width = width;
	bo->height = height;
	bo->handles[0].u32 = create_dumb.handle;
	bo->offsets[0] = 0;
	bo->total_size = bo->sizes[0] = create_dumb.size;
	bo->strides[0] = create_dumb.pitch;

	return 0;
}

int drv_dumb_bo_destroy(struct bo *bo)
{
	struct drm_mode_destroy_dumb destroy_dumb;
	int ret;

	memset(&destroy_dumb, 0, sizeof(destroy_dumb));
	destroy_dumb.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_dumb);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_MODE_DESTROY_DUMB failed "
				"(handle=%x)\n", bo->handles[0].u32);
		return ret;
	}

	return 0;
}

int drv_gem_bo_destroy(struct bo *bo)
{
	struct drm_gem_close gem_close;
	int ret, error = 0;
	size_t plane, i;

	for (plane = 0; plane < bo->num_planes; plane++) {
		for (i = 0; i < plane; i++)
			if (bo->handles[i].u32 == bo->handles[plane].u32)
				break;
		/* Make sure close hasn't already been called on this handle */
		if (i != plane)
			continue;

		memset(&gem_close, 0, sizeof(gem_close));
		gem_close.handle = bo->handles[plane].u32;

		ret = drmIoctl(bo->drv->fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
		if (ret) {
			fprintf(stderr, "drv: DRM_IOCTL_GEM_CLOSE failed "
					"(handle=%x) error %d\n",
					bo->handles[plane].u32, ret);
			error = ret;
		}
	}

	return error;
}

int drv_prime_bo_import(struct bo *bo, struct drv_import_fd_data *data)
{
	int ret;
	size_t plane;
	struct drm_prime_handle prime_handle;

	for (plane = 0; plane < bo->num_planes; plane++) {
		memset(&prime_handle, 0, sizeof(prime_handle));
		prime_handle.fd = data->fds[plane];

		ret = drmIoctl(bo->drv->fd, DRM_IOCTL_PRIME_FD_TO_HANDLE,
			       &prime_handle);

		if (ret) {
			fprintf(stderr, "drv: DRM_IOCTL_PRIME_FD_TO_HANDLE "
				"failed (fd=%u)\n", prime_handle.fd);

			/*
			 * Need to call GEM close on planes that were opened,
			 * if any. Adjust the num_planes variable to be the
			 * plane that failed, so GEM close will be called on
			 * planes before that plane.
			 */
			bo->num_planes = plane;
			drv_gem_bo_destroy(bo);
			return ret;
		}

		bo->handles[plane].u32 = prime_handle.handle;
	}

	for (plane = 0; plane < bo->num_planes; plane++) {
		pthread_mutex_lock(&bo->drv->driver_lock);
		drv_increment_reference_count(bo->drv, bo, plane);
		pthread_mutex_unlock(&bo->drv->driver_lock);
	}

	return 0;
}

void *drv_dumb_bo_map(struct bo *bo, struct map_info *data, size_t plane)
{
	int ret;
	size_t i;
	struct drm_mode_map_dumb map_dumb;

	memset(&map_dumb, 0, sizeof(map_dumb));
	map_dumb.handle = bo->handles[plane].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_MODE_MAP_DUMB failed \n");
		return MAP_FAILED;
	}

	for (i = 0; i < bo->num_planes; i++)
		if (bo->handles[i].u32 == bo->handles[plane].u32)
			data->length += bo->sizes[i];

	return mmap(0, data->length, PROT_READ | PROT_WRITE, MAP_SHARED,
		    bo->drv->fd, map_dumb.offset);
}

uintptr_t drv_get_reference_count(struct driver *drv, struct bo *bo,
				  size_t plane)
{
	void *count;
	uintptr_t num = 0;

	if (!drmHashLookup(drv->buffer_table, bo->handles[plane].u32, &count))
		num = (uintptr_t) (count);

	return num;
}

void drv_increment_reference_count(struct driver *drv, struct bo *bo,
				   size_t plane)
{
	uintptr_t num = drv_get_reference_count(drv, bo, plane);

	/* If a value isn't in the table, drmHashDelete is a no-op */
	drmHashDelete(drv->buffer_table, bo->handles[plane].u32);
	drmHashInsert(drv->buffer_table, bo->handles[plane].u32,
		      (void *) (num + 1));
}

void drv_decrement_reference_count(struct driver *drv, struct bo *bo,
				   size_t plane)
{
	uintptr_t num = drv_get_reference_count(drv, bo, plane);

	drmHashDelete(drv->buffer_table, bo->handles[plane].u32);

	if (num > 0)
		drmHashInsert(drv->buffer_table, bo->handles[plane].u32,
			      (void *) (num - 1));
}

uint32_t drv_log_base2(uint32_t value)
{
	int ret = 0;

	while (value >>= 1)
		++ret;

	return ret;
}

/* Inserts a combination into list -- caller should have lock on driver. */
void drv_insert_supported_combination(struct driver *drv, uint32_t format,
			              uint64_t usage, uint64_t modifier)
{
	struct combination_list_element *elem;

	elem = calloc(1, sizeof(*elem));
	elem->combination.format = format;
	elem->combination.modifier = modifier;
	elem->combination.usage = usage;
	LIST_ADD(&elem->link, &drv->backend->combinations);
}

void drv_insert_combinations(struct driver *drv, struct supported_combination *combos,
			     uint32_t size)
{
	unsigned int i;

	pthread_mutex_lock(&drv->driver_lock);

	for (i = 0; i < size; i++)
		drv_insert_supported_combination(drv, combos[i].format,
						 combos[i].usage,
						 combos[i].modifier);

	pthread_mutex_unlock(&drv->driver_lock);
}

void drv_modify_supported_combination(struct driver *drv, uint32_t format,
				      uint64_t usage, uint64_t modifier)
{
	/*
	 * Attempts to add the specified usage to an existing {format, modifier}
	 * pair. If the pair is not present, a new combination is created.
	 */
	int found = 0;

	pthread_mutex_lock(&drv->driver_lock);

	list_for_each_entry(struct combination_list_element,
			    elem, &drv->backend->combinations, link) {
		if (elem->combination.format == format &&
		    elem->combination.modifier == modifier) {
			elem->combination.usage |= usage;
			found = 1;
		}
	}


	if (!found)
		drv_insert_supported_combination(drv, format, usage, modifier);

	pthread_mutex_unlock(&drv->driver_lock);
}

int drv_add_kms_flags(struct driver *drv)
{
	int ret;
	uint32_t i, j;
	uint64_t flag, usage;
	drmModePlanePtr plane;
	drmModePropertyPtr prop;
	drmModePlaneResPtr resources;
	drmModeObjectPropertiesPtr props;

	/*
	 * All current drivers can scanout XRGB8888/ARGB8888 as a primary plane.
	 * Some older kernel versions can only return overlay planes, so add the
	 * combination here. Note that the kernel disregards the alpha component
	 * of ARGB unless it's an overlay plane.
	 */
	drv_modify_supported_combination(drv, DRM_FORMAT_XRGB8888,
					 BO_USE_SCANOUT, 0);
	drv_modify_supported_combination(drv, DRM_FORMAT_ARGB8888,
					 BO_USE_SCANOUT, 0);

	/*
	 * The ability to return universal planes is only complete on
	 * ChromeOS kernel versions >= v3.18.  The SET_CLIENT_CAP ioctl
	 * therefore might return an error code, so don't check it.  If it
	 * fails, it'll just return the plane list as overlay planes, which is
	 * fine in our case (our drivers already have cursor bits set).
	 * modetest in libdrm does the same thing.
	 */
	drmSetClientCap(drv->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

	resources = drmModeGetPlaneResources(drv->fd);
	if (!resources)
		goto err;

	for (i = 0; i < resources->count_planes; i++) {

		plane = drmModeGetPlane(drv->fd, resources->planes[i]);

		if (!plane)
			goto err;

		props = drmModeObjectGetProperties(drv->fd, plane->plane_id,
						   DRM_MODE_OBJECT_PLANE);
		if (!props)
			goto err;

		for (j = 0; j < props->count_props; j++) {

			prop = drmModeGetProperty(drv->fd, props->props[j]);
			if (prop) {
				if (strcmp(prop->name, "type") == 0) {
					flag = props->prop_values[j];
				}
				drmModeFreeProperty(prop);
			}
		}

		switch (flag) {
		case DRM_PLANE_TYPE_OVERLAY:
		case DRM_PLANE_TYPE_PRIMARY:
			usage = BO_USE_SCANOUT;
			break;
		case DRM_PLANE_TYPE_CURSOR:
			usage = BO_USE_CURSOR;
			break;
		default:
			assert(0);
		}

		for (j = 0; j < plane->count_formats; j++)
			drv_modify_supported_combination(drv, plane->formats[j],
							 usage, 0);

		drmModeFreeObjectProperties(props);
		drmModeFreePlane(plane);

	}

	drmModeFreePlaneResources(resources);
	return 0;

err:
	ret = -1;
	return ret;
}
