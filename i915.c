/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_I915

#include <errno.h>
#include <i915_drm.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

#define I915_CACHELINE_SIZE 64
#define I915_CACHELINE_MASK (I915_CACHELINE_SIZE - 1)

static const uint32_t render_target_formats[] = { DRM_FORMAT_ARGB1555, DRM_FORMAT_ABGR8888,
						  DRM_FORMAT_ARGB8888, DRM_FORMAT_RGB565,
						  DRM_FORMAT_XBGR8888, DRM_FORMAT_XRGB1555,
						  DRM_FORMAT_XRGB8888 };

static const uint32_t tileable_texture_source_formats[] = { DRM_FORMAT_GR88, DRM_FORMAT_R8,
							    DRM_FORMAT_UYVY, DRM_FORMAT_YUYV };

static const uint32_t texture_source_formats[] = { DRM_FORMAT_YVU420, DRM_FORMAT_YVU420_ANDROID };

struct i915_device {
	uint32_t gen;
	int32_t has_llc;
};

static uint32_t i915_get_gen(int device_id)
{
	const uint16_t gen3_ids[] = { 0x2582, 0x2592, 0x2772, 0x27A2, 0x27AE,
				      0x29C2, 0x29B2, 0x29D2, 0xA001, 0xA011 };
	unsigned i;
	for (i = 0; i < ARRAY_SIZE(gen3_ids); i++)
		if (gen3_ids[i] == device_id)
			return 3;

	return 4;
}

static int i915_add_kms_item(struct driver *drv, const struct kms_item *item)
{
	uint32_t i;
	struct combination *combo;

	/*
	 * Older hardware can't scanout Y-tiled formats. Newer devices can, and
	 * report this functionality via format modifiers.
	 */
	for (i = 0; i < drv->backend->combos.size; i++) {
		combo = &drv->backend->combos.data[i];
		if (combo->format == item->format) {
			if ((combo->metadata.tiling == I915_TILING_Y &&
			     item->modifier == I915_FORMAT_MOD_Y_TILED) ||
			    (combo->metadata.tiling == I915_TILING_X &&
			     item->modifier == I915_FORMAT_MOD_X_TILED)) {
				combo->metadata.modifier = item->modifier;
				combo->usage |= item->usage;
			} else if (combo->metadata.tiling != I915_TILING_Y) {
				combo->usage |= item->usage;
			}
		}
	}

	return 0;
}

static int i915_add_combinations(struct driver *drv)
{
	int ret;
	uint32_t i, num_items;
	struct kms_item *items;
	struct format_metadata metadata;
	uint64_t render_flags, texture_flags;

	render_flags = BO_USE_RENDER_MASK;
	texture_flags = BO_USE_TEXTURE_MASK;

	metadata.tiling = I915_TILING_NONE;
	metadata.priority = 1;
	metadata.modifier = DRM_FORMAT_MOD_NONE;

	ret = drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
				   &metadata, render_flags);
	if (ret)
		return ret;

	ret = drv_add_combinations(drv, texture_source_formats, ARRAY_SIZE(texture_source_formats),
				   &metadata, texture_flags);
	if (ret)
		return ret;

	ret = drv_add_combinations(drv, tileable_texture_source_formats,
				   ARRAY_SIZE(tileable_texture_source_formats), &metadata,
				   texture_flags);
	if (ret)
		return ret;

	drv_modify_combination(drv, DRM_FORMAT_XRGB8888, &metadata, BO_USE_CURSOR | BO_USE_SCANOUT);
	drv_modify_combination(drv, DRM_FORMAT_ARGB8888, &metadata, BO_USE_CURSOR | BO_USE_SCANOUT);

	render_flags &= ~BO_USE_SW_WRITE_OFTEN;
	render_flags &= ~BO_USE_SW_READ_OFTEN;
	render_flags &= ~BO_USE_LINEAR;

	texture_flags &= ~BO_USE_SW_WRITE_OFTEN;
	texture_flags &= ~BO_USE_SW_READ_OFTEN;
	texture_flags &= ~BO_USE_LINEAR;

	metadata.tiling = I915_TILING_X;
	metadata.priority = 2;

	ret = drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
				   &metadata, render_flags);
	if (ret)
		return ret;

	ret = drv_add_combinations(drv, tileable_texture_source_formats,
				   ARRAY_SIZE(tileable_texture_source_formats), &metadata,
				   texture_flags);
	if (ret)
		return ret;

	metadata.tiling = I915_TILING_Y;
	metadata.priority = 3;

	ret = drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
				   &metadata, render_flags);
	if (ret)
		return ret;

	ret = drv_add_combinations(drv, tileable_texture_source_formats,
				   ARRAY_SIZE(tileable_texture_source_formats), &metadata,
				   texture_flags);
	if (ret)
		return ret;

	items = drv_query_kms(drv, &num_items);
	if (!items || !num_items)
		return 0;

	for (i = 0; i < num_items; i++) {
		ret = i915_add_kms_item(drv, &items[i]);
		if (ret) {
			free(items);
			return ret;
		}
	}

	free(items);
	return 0;
}

static int i915_align_dimensions(struct bo *bo, uint32_t tiling, uint32_t *stride,
				 uint32_t *aligned_height)
{
	struct i915_device *i915 = bo->drv->priv;
	uint32_t horizontal_alignment = 4;
	uint32_t vertical_alignment = 4;

	switch (tiling) {
	default:
	case I915_TILING_NONE:
		horizontal_alignment = 64;
		break;

	case I915_TILING_X:
		horizontal_alignment = 512;
		vertical_alignment = 8;
		break;

	case I915_TILING_Y:
		if (i915->gen == 3) {
			horizontal_alignment = 512;
			vertical_alignment = 8;
		} else {
			horizontal_alignment = 128;
			vertical_alignment = 32;
		}
		break;
	}

	*aligned_height = ALIGN(bo->height, vertical_alignment);
	if (i915->gen > 3) {
		*stride = ALIGN(*stride, horizontal_alignment);
	} else {
		while (*stride > horizontal_alignment)
			horizontal_alignment <<= 1;

		*stride = horizontal_alignment;
	}

	if (i915->gen <= 3 && *stride > 8192)
		return -EINVAL;

	return 0;
}

static void i915_clflush(void *start, size_t size)
{
	void *p = (void *)(((uintptr_t)start) & ~I915_CACHELINE_MASK);
	void *end = (void *)((uintptr_t)start + size);

	__builtin_ia32_mfence();
	while (p < end) {
		__builtin_ia32_clflush(p);
		p = (void *)((uintptr_t)p + I915_CACHELINE_SIZE);
	}
}

static int i915_init(struct driver *drv)
{
	int ret;
	int device_id;
	struct i915_device *i915;
	drm_i915_getparam_t get_param;

	i915 = calloc(1, sizeof(*i915));
	if (!i915)
		return -ENOMEM;

	memset(&get_param, 0, sizeof(get_param));
	get_param.param = I915_PARAM_CHIPSET_ID;
	get_param.value = &device_id;
	ret = drmIoctl(drv->fd, DRM_IOCTL_I915_GETPARAM, &get_param);
	if (ret) {
		fprintf(stderr, "drv: Failed to get I915_PARAM_CHIPSET_ID\n");
		free(i915);
		return -EINVAL;
	}

	i915->gen = i915_get_gen(device_id);

	memset(&get_param, 0, sizeof(get_param));
	get_param.param = I915_PARAM_HAS_LLC;
	get_param.value = &i915->has_llc;
	ret = drmIoctl(drv->fd, DRM_IOCTL_I915_GETPARAM, &get_param);
	if (ret) {
		fprintf(stderr, "drv: Failed to get I915_PARAM_HAS_LLC\n");
		free(i915);
		return -EINVAL;
	}

	drv->priv = i915;

	return i915_add_combinations(drv);
}

static int i915_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
			  uint32_t flags)
{
	int ret;
	size_t plane;
	uint32_t stride;
	struct drm_i915_gem_create gem_create;
	struct drm_i915_gem_set_tiling gem_set_tiling;

	if (flags & (BO_USE_CURSOR | BO_USE_LINEAR | BO_USE_SW_READ_OFTEN | BO_USE_SW_WRITE_OFTEN))
		bo->tiling = I915_TILING_NONE;
	else if (flags & BO_USE_SCANOUT)
		bo->tiling = I915_TILING_X;
	else
		bo->tiling = I915_TILING_Y;

	if (format == DRM_FORMAT_YVU420 || format == DRM_FORMAT_YVU420_ANDROID)
		bo->tiling = I915_TILING_NONE;

	stride = drv_stride_from_format(format, width, 0);

	ret = i915_align_dimensions(bo, bo->tiling, &stride, &height);
	if (ret)
		return ret;

	/*
	 * Align the Y plane to 128 bytes so the chroma planes would be aligned
	 * to 64 byte boundaries. This is an Intel HW requirement.
	 */
	if (format == DRM_FORMAT_YVU420)
		stride = ALIGN(stride, 128);

	/*
	 * HAL_PIXEL_FORMAT_YV12 requires that the buffer's height not be aligned.
	 */
	if (format == DRM_FORMAT_YVU420_ANDROID)
		height = bo->height;

	drv_bo_from_format(bo, stride, height, format);

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = bo->total_size;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_CREATE, &gem_create);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_I915_GEM_CREATE failed (size=%llu)\n",
			gem_create.size);
		return ret;
	}

	for (plane = 0; plane < bo->num_planes; plane++)
		bo->handles[plane].u32 = gem_create.handle;

	memset(&gem_set_tiling, 0, sizeof(gem_set_tiling));
	gem_set_tiling.handle = bo->handles[0].u32;
	gem_set_tiling.tiling_mode = bo->tiling;
	gem_set_tiling.stride = bo->strides[0];

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_SET_TILING, &gem_set_tiling);
	if (ret) {
		struct drm_gem_close gem_close;
		memset(&gem_close, 0, sizeof(gem_close));
		gem_close.handle = bo->handles[0].u32;
		drmIoctl(bo->drv->fd, DRM_IOCTL_GEM_CLOSE, &gem_close);

		fprintf(stderr, "drv: DRM_IOCTL_I915_GEM_SET_TILING failed with %d", errno);
		return -errno;
	}

	return 0;
}

static void i915_close(struct driver *drv)
{
	free(drv->priv);
	drv->priv = NULL;
}

static int i915_bo_import(struct bo *bo, struct drv_import_fd_data *data)
{
	int ret;
	struct drm_i915_gem_get_tiling gem_get_tiling;

	ret = drv_prime_bo_import(bo, data);
	if (ret)
		return ret;

	/* TODO(gsingh): export modifiers and get rid of backdoor tiling. */
	memset(&gem_get_tiling, 0, sizeof(gem_get_tiling));
	gem_get_tiling.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_GET_TILING, &gem_get_tiling);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_I915_GEM_GET_TILING failed.");
		return ret;
	}

	bo->tiling = gem_get_tiling.tiling_mode;
	return 0;
}

static void *i915_bo_map(struct bo *bo, struct map_info *data, size_t plane)
{
	int ret;
	void *addr;
	struct drm_i915_gem_set_domain set_domain;

	memset(&set_domain, 0, sizeof(set_domain));
	set_domain.handle = bo->handles[0].u32;
	if (bo->tiling == I915_TILING_NONE) {
		struct drm_i915_gem_mmap gem_map;
		memset(&gem_map, 0, sizeof(gem_map));

		gem_map.handle = bo->handles[0].u32;
		gem_map.offset = 0;
		gem_map.size = bo->total_size;

		ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_MMAP, &gem_map);
		if (ret) {
			fprintf(stderr, "drv: DRM_IOCTL_I915_GEM_MMAP failed\n");
			return MAP_FAILED;
		}

		addr = (void *)(uintptr_t)gem_map.addr_ptr;
		set_domain.read_domains = I915_GEM_DOMAIN_CPU;
		set_domain.write_domain = I915_GEM_DOMAIN_CPU;

	} else {
		struct drm_i915_gem_mmap_gtt gem_map;
		memset(&gem_map, 0, sizeof(gem_map));

		gem_map.handle = bo->handles[0].u32;

		ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_MMAP_GTT, &gem_map);
		if (ret) {
			fprintf(stderr, "drv: DRM_IOCTL_I915_GEM_MMAP_GTT failed\n");
			return MAP_FAILED;
		}

		addr = mmap(0, bo->total_size, PROT_READ | PROT_WRITE, MAP_SHARED, bo->drv->fd,
			    gem_map.offset);

		set_domain.read_domains = I915_GEM_DOMAIN_GTT;
		set_domain.write_domain = I915_GEM_DOMAIN_GTT;
	}

	if (addr == MAP_FAILED) {
		fprintf(stderr, "drv: i915 GEM mmap failed\n");
		return addr;
	}

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_I915_GEM_SET_DOMAIN failed\n");
		return MAP_FAILED;
	}

	data->length = bo->total_size;
	return addr;
}

static int i915_bo_unmap(struct bo *bo, struct map_info *data)
{
	struct i915_device *i915 = bo->drv->priv;
	if (!i915->has_llc && bo->tiling == I915_TILING_NONE)
		i915_clflush(data->addr, data->length);

	return munmap(data->addr, data->length);
}

static uint32_t i915_resolve_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/*HACK: See b/28671744 */
		return DRM_FORMAT_XBGR8888;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		return DRM_FORMAT_YVU420;
	default:
		return format;
	}
}

struct backend backend_i915 = {
	.name = "i915",
	.init = i915_init,
	.close = i915_close,
	.bo_create = i915_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_import = i915_bo_import,
	.bo_map = i915_bo_map,
	.bo_unmap = i915_bo_unmap,
	.resolve_format = i915_resolve_format,
};

#endif
