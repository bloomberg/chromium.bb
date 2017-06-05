/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
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

static uint32_t subsample_stride(uint32_t stride, uint32_t format, size_t plane)
{

	if (plane != 0) {
		switch (format) {
		case DRM_FORMAT_YVU420:
		case DRM_FORMAT_YVU420_ANDROID:
			stride = DIV_ROUND_UP(stride, 2);
			break;
		}
	}

	return stride;
}

static uint32_t bpp_from_format(uint32_t format, size_t plane)
{
	assert(plane < drv_num_planes_from_format(format));

	switch (format) {
	case DRM_FORMAT_BGR233:
	case DRM_FORMAT_C8:
	case DRM_FORMAT_R8:
	case DRM_FORMAT_RGB332:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YVU420_ANDROID:
		return 8;

	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		return (plane == 0) ? 8 : 4;

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

uint32_t drv_bo_get_stride_in_pixels(struct bo *bo)
{
	uint32_t bytes_per_pixel = DIV_ROUND_UP(bpp_from_format(bo->format, 0), 8);
	return DIV_ROUND_UP(bo->strides[0], bytes_per_pixel);
}

/*
 * This function returns the stride for a given format, width and plane.
 */
uint32_t drv_stride_from_format(uint32_t format, uint32_t width, size_t plane)
{
	uint32_t stride = DIV_ROUND_UP(width * bpp_from_format(format, plane), 8);

	/*
	 * The stride of Android YV12 buffers is required to be aligned to 16 bytes
	 * (see <system/graphics.h>).
	 */
	if (format == DRM_FORMAT_YVU420_ANDROID)
		stride = (plane == 0) ? ALIGN(stride, 32) : ALIGN(stride, 16);

	return stride;
}

/*
 * This function fills in the buffer object given the driver aligned stride of
 * the first plane, height and a format. This function assumes there is just
 * one kernel buffer per buffer object.
 */
int drv_bo_from_format(struct bo *bo, uint32_t stride, uint32_t aligned_height, uint32_t format)
{

	size_t p, num_planes;
	uint32_t offset = 0;

	num_planes = drv_num_planes_from_format(format);
	assert(num_planes);

	/*
	 * HAL_PIXEL_FORMAT_YV12 requires that (see <system/graphics.h>):
	 *  - the aligned height is same as the buffer's height.
	 *  - the chroma stride is 16 bytes aligned, i.e., the luma's strides
	 *    is 32 bytes aligned.
	 */
	if (bo->format == DRM_FORMAT_YVU420_ANDROID) {
		assert(aligned_height == bo->height);
		assert(stride == ALIGN(stride, 32));
	}

	for (p = 0; p < num_planes; p++) {
		bo->strides[p] = subsample_stride(stride, format, p);
		bo->sizes[p] = drv_size_from_format(format, bo->strides[p], aligned_height, p);
		bo->offsets[p] = offset;
		offset += bo->sizes[p];
	}

	bo->total_size = offset;
	return 0;
}

int drv_dumb_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
		       uint32_t flags)
{
	int ret;
	size_t plane;
	uint32_t aligned_width, aligned_height;
	struct drm_mode_create_dumb create_dumb;

	aligned_width = width;
	aligned_height = height;
	if (format == DRM_FORMAT_YVU420_ANDROID) {
		/*
		 * Align width to 32 pixels, so chroma strides are 16 bytes as
		 * Android requires.
		 */
		aligned_width = ALIGN(width, 32);
	}

	if (format == DRM_FORMAT_YVU420_ANDROID || format == DRM_FORMAT_YVU420) {
		aligned_height = 3 * DIV_ROUND_UP(height, 2);
	}

	memset(&create_dumb, 0, sizeof(create_dumb));
	create_dumb.height = aligned_height;
	create_dumb.width = aligned_width;
	create_dumb.bpp = bpp_from_format(format, 0);
	create_dumb.flags = 0;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_MODE_CREATE_DUMB failed\n");
		return ret;
	}

	drv_bo_from_format(bo, create_dumb.pitch, height, format);

	for (plane = 0; plane < bo->num_planes; plane++)
		bo->handles[plane].u32 = create_dumb.handle;

	bo->total_size = create_dumb.size;
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
		fprintf(stderr, "drv: DRM_IOCTL_MODE_DESTROY_DUMB failed (handle=%x)\n",
			bo->handles[0].u32);
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
			fprintf(stderr, "drv: DRM_IOCTL_GEM_CLOSE failed (handle=%x) error %d\n",
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

		ret = drmIoctl(bo->drv->fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &prime_handle);

		if (ret) {
			fprintf(stderr, "drv: DRM_IOCTL_PRIME_FD_TO_HANDLE failed (fd=%u)\n",
				prime_handle.fd);

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

	return mmap(0, data->length, PROT_READ | PROT_WRITE, MAP_SHARED, bo->drv->fd,
		    map_dumb.offset);
}

uintptr_t drv_get_reference_count(struct driver *drv, struct bo *bo, size_t plane)
{
	void *count;
	uintptr_t num = 0;

	if (!drmHashLookup(drv->buffer_table, bo->handles[plane].u32, &count))
		num = (uintptr_t)(count);

	return num;
}

void drv_increment_reference_count(struct driver *drv, struct bo *bo, size_t plane)
{
	uintptr_t num = drv_get_reference_count(drv, bo, plane);

	/* If a value isn't in the table, drmHashDelete is a no-op */
	drmHashDelete(drv->buffer_table, bo->handles[plane].u32);
	drmHashInsert(drv->buffer_table, bo->handles[plane].u32, (void *)(num + 1));
}

void drv_decrement_reference_count(struct driver *drv, struct bo *bo, size_t plane)
{
	uintptr_t num = drv_get_reference_count(drv, bo, plane);

	drmHashDelete(drv->buffer_table, bo->handles[plane].u32);

	if (num > 0)
		drmHashInsert(drv->buffer_table, bo->handles[plane].u32, (void *)(num - 1));
}

uint32_t drv_log_base2(uint32_t value)
{
	int ret = 0;

	while (value >>= 1)
		++ret;

	return ret;
}

int drv_add_combination(struct driver *drv, uint32_t format, struct format_metadata *metadata,
			uint64_t usage)
{
	struct combinations *combos = &drv->backend->combos;
	if (combos->size >= combos->allocations) {
		struct combination *new_data;
		combos->allocations *= 2;
		new_data = realloc(combos->data, combos->allocations * sizeof(*combos->data));
		if (!new_data)
			return -ENOMEM;

		combos->data = new_data;
	}

	combos->data[combos->size].format = format;
	combos->data[combos->size].metadata.priority = metadata->priority;
	combos->data[combos->size].metadata.tiling = metadata->tiling;
	combos->data[combos->size].metadata.modifier = metadata->modifier;
	combos->data[combos->size].usage = usage;
	combos->size++;
	return 0;
}

int drv_add_combinations(struct driver *drv, const uint32_t *formats, uint32_t num_formats,
			 struct format_metadata *metadata, uint64_t usage)
{
	int ret;
	uint32_t i;
	for (i = 0; i < num_formats; i++) {
		ret = drv_add_combination(drv, formats[i], metadata, usage);
		if (ret)
			return ret;
	}

	return 0;
}

void drv_modify_combination(struct driver *drv, uint32_t format, struct format_metadata *metadata,
			    uint64_t usage)
{
	uint32_t i;
	struct combination *combo;
	/* Attempts to add the specified usage to an existing combination. */
	for (i = 0; i < drv->backend->combos.size; i++) {
		combo = &drv->backend->combos.data[i];
		if (combo->format == format && combo->metadata.tiling == metadata->tiling &&
		    combo->metadata.modifier == metadata->modifier)
			combo->usage |= usage;
	}
}

struct kms_item *drv_query_kms(struct driver *drv, uint32_t *num_items)
{
	uint64_t flag, usage;
	struct kms_item *items;
	uint32_t i, j, k, allocations, item_size;

	drmModePlanePtr plane;
	drmModePropertyPtr prop;
	drmModePlaneResPtr resources;
	drmModeObjectPropertiesPtr props;

	/* Start with a power of 2 number of allocations. */
	allocations = 2;
	item_size = 0;
	items = calloc(allocations, sizeof(*items));
	if (!items)
		goto out;

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
		goto out;

	for (i = 0; i < resources->count_planes; i++) {
		plane = drmModeGetPlane(drv->fd, resources->planes[i]);
		if (!plane)
			goto out;

		props = drmModeObjectGetProperties(drv->fd, plane->plane_id, DRM_MODE_OBJECT_PLANE);
		if (!props)
			goto out;

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

		for (j = 0; j < plane->count_formats; j++) {
			bool found = false;
			for (k = 0; k < item_size; k++) {
				if (items[k].format == plane->formats[j] &&
				    items[k].modifier == DRM_FORMAT_MOD_NONE) {
					items[k].usage |= usage;
					found = true;
					break;
				}
			}

			if (!found && item_size >= allocations) {
				struct kms_item *new_data = NULL;
				allocations *= 2;
				new_data = realloc(items, allocations * sizeof(*items));
				if (!new_data) {
					item_size = 0;
					goto out;
				}

				items = new_data;
			}

			if (!found) {
				items[item_size].format = plane->formats[j];
				items[item_size].modifier = DRM_FORMAT_MOD_NONE;
				items[item_size].usage = usage;
				item_size++;
			}
		}

		drmModeFreeObjectProperties(props);
		drmModeFreePlane(plane);
	}

	drmModeFreePlaneResources(resources);
out:
	if (items && item_size == 0) {
		free(items);
		items = NULL;
	}

	*num_items = item_size;
	return items;
}

int drv_modify_linear_combinations(struct driver *drv)
{
	uint32_t i, j, num_items;
	struct kms_item *items;
	struct combination *combo;

	/*
	 * All current drivers can scanout linear XRGB8888/ARGB8888 as a primary
	 * plane and as a cursor. Some drivers don't support
	 * drmModeGetPlaneResources, so add the combination here. Note that the
	 * kernel disregards the alpha component of ARGB unless it's an overlay
	 * plane.
	 */
	drv_modify_combination(drv, DRM_FORMAT_XRGB8888, &LINEAR_METADATA,
			       BO_USE_CURSOR | BO_USE_SCANOUT);
	drv_modify_combination(drv, DRM_FORMAT_ARGB8888, &LINEAR_METADATA,
			       BO_USE_CURSOR | BO_USE_SCANOUT);

	items = drv_query_kms(drv, &num_items);
	if (!items || !num_items)
		return 0;

	for (i = 0; i < num_items; i++) {
		for (j = 0; j < drv->backend->combos.size; j++) {
			combo = &drv->backend->combos.data[j];
			if (items[i].format == combo->format)
				combo->usage |= BO_USE_SCANOUT;
		}
	}

	free(items);
	return 0;
}
