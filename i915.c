/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_I915

#include <assert.h>
#include <errno.h>
#include <i915_drm.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

#define I915_CACHELINE_SIZE 64
#define I915_CACHELINE_MASK (I915_CACHELINE_SIZE - 1)

static const uint32_t scanout_render_formats[] = { DRM_FORMAT_ABGR2101010, DRM_FORMAT_ABGR8888,
						   DRM_FORMAT_ARGB2101010, DRM_FORMAT_ARGB8888,
						   DRM_FORMAT_RGB565,	   DRM_FORMAT_XBGR2101010,
						   DRM_FORMAT_XBGR8888,	   DRM_FORMAT_XRGB2101010,
						   DRM_FORMAT_XRGB8888 };

static const uint32_t render_formats[] = { DRM_FORMAT_ABGR16161616F };

static const uint32_t texture_only_formats[] = { DRM_FORMAT_R8, DRM_FORMAT_NV12, DRM_FORMAT_P010,
						 DRM_FORMAT_YVU420, DRM_FORMAT_YVU420_ANDROID };

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

static uint64_t unset_flags(uint64_t current_flags, uint64_t mask)
{
	uint64_t value = current_flags & ~mask;
	return value;
}

static int i915_add_combinations(struct driver *drv)
{
	struct format_metadata metadata;
	uint64_t render, scanout_and_render, texture_only;

	scanout_and_render = BO_USE_RENDER_MASK | BO_USE_SCANOUT;
	render = BO_USE_RENDER_MASK;
	texture_only = BO_USE_TEXTURE_MASK;
	uint64_t linear_mask = BO_USE_RENDERSCRIPT | BO_USE_LINEAR | BO_USE_PROTECTED |
			       BO_USE_SW_READ_OFTEN | BO_USE_SW_WRITE_OFTEN;

	metadata.tiling = I915_TILING_NONE;
	metadata.priority = 1;
	metadata.modifier = DRM_FORMAT_MOD_LINEAR;

	drv_add_combinations(drv, scanout_render_formats, ARRAY_SIZE(scanout_render_formats),
			     &metadata, scanout_and_render);

	drv_add_combinations(drv, render_formats, ARRAY_SIZE(render_formats), &metadata, render);

	drv_add_combinations(drv, texture_only_formats, ARRAY_SIZE(texture_only_formats), &metadata,
			     texture_only);

	drv_modify_linear_combinations(drv);
	/*
	 * Chrome uses DMA-buf mmap to write to YV12 buffers, which are then accessed by the
	 * Video Encoder Accelerator (VEA). It could also support NV12 potentially in the future.
	 */
	drv_modify_combination(drv, DRM_FORMAT_YVU420, &metadata, BO_USE_HW_VIDEO_ENCODER);
	/* IPU3 camera ISP supports only NV12 output. */
	drv_modify_combination(drv, DRM_FORMAT_NV12, &metadata,
			       BO_USE_HW_VIDEO_ENCODER | BO_USE_HW_VIDEO_DECODER |
				   BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE | BO_USE_SCANOUT);

	/* Android CTS tests require this. */
	drv_add_combination(drv, DRM_FORMAT_BGR888, &metadata, BO_USE_SW_MASK);

	/*
	 * R8 format is used for Android's HAL_PIXEL_FORMAT_BLOB and is used for JPEG snapshots
	 * from camera.
	 */
	drv_modify_combination(drv, DRM_FORMAT_R8, &metadata,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE);

	render = unset_flags(render, linear_mask);
	scanout_and_render = unset_flags(scanout_and_render, linear_mask);

	metadata.tiling = I915_TILING_X;
	metadata.priority = 2;
	metadata.modifier = I915_FORMAT_MOD_X_TILED;

	drv_add_combinations(drv, render_formats, ARRAY_SIZE(render_formats), &metadata, render);
	drv_add_combinations(drv, scanout_render_formats, ARRAY_SIZE(scanout_render_formats),
			     &metadata, scanout_and_render);

	metadata.tiling = I915_TILING_Y;
	metadata.priority = 3;
	metadata.modifier = I915_FORMAT_MOD_Y_TILED;

	scanout_and_render =
	    unset_flags(scanout_and_render, BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY);
/* Support y-tiled NV12 and P010 for libva */
#ifdef I915_SCANOUT_Y_TILED
	drv_add_combination(drv, DRM_FORMAT_NV12, &metadata,
			    BO_USE_TEXTURE | BO_USE_HW_VIDEO_DECODER | BO_USE_SCANOUT);
#else
	drv_add_combination(drv, DRM_FORMAT_NV12, &metadata,
			    BO_USE_TEXTURE | BO_USE_HW_VIDEO_DECODER);
#endif
	scanout_and_render = unset_flags(scanout_and_render, BO_USE_SCANOUT);
	drv_add_combination(drv, DRM_FORMAT_P010, &metadata,
			    BO_USE_TEXTURE | BO_USE_HW_VIDEO_DECODER);

	drv_add_combinations(drv, render_formats, ARRAY_SIZE(render_formats), &metadata, render);
	drv_add_combinations(drv, scanout_render_formats, ARRAY_SIZE(scanout_render_formats),
			     &metadata, scanout_and_render);
	return 0;
}

static int i915_align_dimensions(struct bo *bo, uint32_t tiling, uint32_t *stride,
				 uint32_t *aligned_height)
{
	struct i915_device *i915 = bo->drv->priv;
	uint32_t horizontal_alignment;
	uint32_t vertical_alignment;

	switch (tiling) {
	default:
	case I915_TILING_NONE:
		/*
		 * The Intel GPU doesn't need any alignment in linear mode,
		 * but libva requires the allocation stride to be aligned to
		 * 16 bytes and height to 4 rows. Further, we round up the
		 * horizontal alignment so that row start on a cache line (64
		 * bytes).
		 */
		horizontal_alignment = 64;
		vertical_alignment = 4;
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

	*aligned_height = ALIGN(*aligned_height, vertical_alignment);
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
		drv_log("Failed to get I915_PARAM_CHIPSET_ID\n");
		free(i915);
		return -EINVAL;
	}

	i915->gen = i915_get_gen(device_id);

	memset(&get_param, 0, sizeof(get_param));
	get_param.param = I915_PARAM_HAS_LLC;
	get_param.value = &i915->has_llc;
	ret = drmIoctl(drv->fd, DRM_IOCTL_I915_GETPARAM, &get_param);
	if (ret) {
		drv_log("Failed to get I915_PARAM_HAS_LLC\n");
		free(i915);
		return -EINVAL;
	}

	drv->priv = i915;

	return i915_add_combinations(drv);
}

static int i915_bo_from_format(struct bo *bo, uint32_t width, uint32_t height, uint32_t format)
{
	uint32_t offset;
	size_t plane;
	int ret, pagesize;

	offset = 0;
	pagesize = getpagesize();
	for (plane = 0; plane < drv_num_planes_from_format(format); plane++) {
		uint32_t stride = drv_stride_from_format(format, width, plane);
		uint32_t plane_height = drv_height_from_format(format, height, plane);

		if (bo->meta.tiling != I915_TILING_NONE)
			assert(IS_ALIGNED(offset, pagesize));

		ret = i915_align_dimensions(bo, bo->meta.tiling, &stride, &plane_height);
		if (ret)
			return ret;

		bo->meta.strides[plane] = stride;
		bo->meta.sizes[plane] = stride * plane_height;
		bo->meta.offsets[plane] = offset;
		offset += bo->meta.sizes[plane];
	}

	bo->meta.total_size = ALIGN(offset, pagesize);

	return 0;
}

static int i915_bo_compute_metadata(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
				    uint64_t use_flags, const uint64_t *modifiers, uint32_t count)
{
	static const uint64_t modifier_order[] = {
		I915_FORMAT_MOD_Y_TILED_CCS,
		I915_FORMAT_MOD_Y_TILED,
		I915_FORMAT_MOD_X_TILED,
		DRM_FORMAT_MOD_LINEAR,
	};
	uint64_t modifier;

	if (modifiers) {
		modifier =
		    drv_pick_modifier(modifiers, count, modifier_order, ARRAY_SIZE(modifier_order));
	} else {
		struct combination *combo = drv_get_combination(bo->drv, format, use_flags);
		if (!combo)
			return -EINVAL;
		modifier = combo->metadata.modifier;
	}

	switch (modifier) {
	case DRM_FORMAT_MOD_LINEAR:
		bo->meta.tiling = I915_TILING_NONE;
		break;
	case I915_FORMAT_MOD_X_TILED:
		bo->meta.tiling = I915_TILING_X;
		break;
	case I915_FORMAT_MOD_Y_TILED:
	case I915_FORMAT_MOD_Y_TILED_CCS:
		bo->meta.tiling = I915_TILING_Y;
		break;
	}

	bo->meta.format_modifiers[0] = modifier;

	if (format == DRM_FORMAT_YVU420_ANDROID) {
		/*
		 * We only need to be able to use this as a linear texture,
		 * which doesn't put any HW restrictions on how we lay it
		 * out. The Android format does require the stride to be a
		 * multiple of 16 and expects the Cr and Cb stride to be
		 * ALIGN(Y_stride / 2, 16), which we can make happen by
		 * aligning to 32 bytes here.
		 */
		uint32_t stride = ALIGN(width, 32);
		drv_bo_from_format(bo, stride, height, format);
	} else if (modifier == I915_FORMAT_MOD_Y_TILED_CCS) {
		/*
		 * For compressed surfaces, we need a color control surface
		 * (CCS). Color compression is only supported for Y tiled
		 * surfaces, and for each 32x16 tiles in the main surface we
		 * need a tile in the control surface.  Y tiles are 128 bytes
		 * wide and 32 lines tall and we use that to first compute the
		 * width and height in tiles of the main surface. stride and
		 * height are already multiples of 128 and 32, respectively:
		 */
		uint32_t stride = drv_stride_from_format(format, width, 0);
		uint32_t width_in_tiles = DIV_ROUND_UP(stride, 128);
		uint32_t height_in_tiles = DIV_ROUND_UP(height, 32);
		uint32_t size = width_in_tiles * height_in_tiles * 4096;
		uint32_t offset = 0;

		bo->meta.strides[0] = width_in_tiles * 128;
		bo->meta.sizes[0] = size;
		bo->meta.offsets[0] = offset;
		offset += size;

		/*
		 * Now, compute the width and height in tiles of the control
		 * surface by dividing and rounding up.
		 */
		uint32_t ccs_width_in_tiles = DIV_ROUND_UP(width_in_tiles, 32);
		uint32_t ccs_height_in_tiles = DIV_ROUND_UP(height_in_tiles, 16);
		uint32_t ccs_size = ccs_width_in_tiles * ccs_height_in_tiles * 4096;

		/*
		 * With stride and height aligned to y tiles, offset is
		 * already a multiple of 4096, which is the required alignment
		 * of the CCS.
		 */
		bo->meta.strides[1] = ccs_width_in_tiles * 128;
		bo->meta.sizes[1] = ccs_size;
		bo->meta.offsets[1] = offset;
		offset += ccs_size;

		bo->meta.num_planes = 2;
		bo->meta.total_size = offset;
	} else {
		i915_bo_from_format(bo, width, height, format);
	}
	return 0;
}

static int i915_bo_create_from_metadata(struct bo *bo)
{
	int ret;
	size_t plane;
	struct drm_i915_gem_create gem_create;
	struct drm_i915_gem_set_tiling gem_set_tiling;

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = bo->meta.total_size;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_CREATE, &gem_create);
	if (ret) {
		drv_log("DRM_IOCTL_I915_GEM_CREATE failed (size=%llu)\n", gem_create.size);
		return -errno;
	}

	for (plane = 0; plane < bo->meta.num_planes; plane++)
		bo->handles[plane].u32 = gem_create.handle;

	memset(&gem_set_tiling, 0, sizeof(gem_set_tiling));
	gem_set_tiling.handle = bo->handles[0].u32;
	gem_set_tiling.tiling_mode = bo->meta.tiling;
	gem_set_tiling.stride = bo->meta.strides[0];

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_SET_TILING, &gem_set_tiling);
	if (ret) {
		struct drm_gem_close gem_close;
		memset(&gem_close, 0, sizeof(gem_close));
		gem_close.handle = bo->handles[0].u32;
		drmIoctl(bo->drv->fd, DRM_IOCTL_GEM_CLOSE, &gem_close);

		drv_log("DRM_IOCTL_I915_GEM_SET_TILING failed with %d\n", errno);
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
		drv_gem_bo_destroy(bo);
		drv_log("DRM_IOCTL_I915_GEM_GET_TILING failed.\n");
		return ret;
	}

	bo->meta.tiling = gem_get_tiling.tiling_mode;
	return 0;
}

static void *i915_bo_map(struct bo *bo, struct vma *vma, size_t plane, uint32_t map_flags)
{
	int ret;
	void *addr;

	if (bo->meta.format_modifiers[0] == I915_FORMAT_MOD_Y_TILED_CCS)
		return MAP_FAILED;

	if (bo->meta.tiling == I915_TILING_NONE) {
		struct drm_i915_gem_mmap gem_map;
		memset(&gem_map, 0, sizeof(gem_map));

		/* TODO(b/118799155): We don't seem to have a good way to
		 * detect the use cases for which WC mapping is really needed.
		 * The current heuristic seems overly coarse and may be slowing
		 * down some other use cases unnecessarily.
		 *
		 * For now, care must be taken not to use WC mappings for
		 * Renderscript and camera use cases, as they're
		 * performance-sensitive. */
		if ((bo->meta.use_flags & BO_USE_SCANOUT) &&
		    !(bo->meta.use_flags &
		      (BO_USE_RENDERSCRIPT | BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE)))
			gem_map.flags = I915_MMAP_WC;

		gem_map.handle = bo->handles[0].u32;
		gem_map.offset = 0;
		gem_map.size = bo->meta.total_size;

		ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_MMAP, &gem_map);
		if (ret) {
			drv_log("DRM_IOCTL_I915_GEM_MMAP failed\n");
			return MAP_FAILED;
		}

		addr = (void *)(uintptr_t)gem_map.addr_ptr;
	} else {
		struct drm_i915_gem_mmap_gtt gem_map;
		memset(&gem_map, 0, sizeof(gem_map));

		gem_map.handle = bo->handles[0].u32;

		ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_MMAP_GTT, &gem_map);
		if (ret) {
			drv_log("DRM_IOCTL_I915_GEM_MMAP_GTT failed\n");
			return MAP_FAILED;
		}

		addr = mmap(0, bo->meta.total_size, drv_get_prot(map_flags), MAP_SHARED,
			    bo->drv->fd, gem_map.offset);
	}

	if (addr == MAP_FAILED) {
		drv_log("i915 GEM mmap failed\n");
		return addr;
	}

	vma->length = bo->meta.total_size;
	return addr;
}

static int i915_bo_invalidate(struct bo *bo, struct mapping *mapping)
{
	int ret;
	struct drm_i915_gem_set_domain set_domain;

	memset(&set_domain, 0, sizeof(set_domain));
	set_domain.handle = bo->handles[0].u32;
	if (bo->meta.tiling == I915_TILING_NONE) {
		set_domain.read_domains = I915_GEM_DOMAIN_CPU;
		if (mapping->vma->map_flags & BO_MAP_WRITE)
			set_domain.write_domain = I915_GEM_DOMAIN_CPU;
	} else {
		set_domain.read_domains = I915_GEM_DOMAIN_GTT;
		if (mapping->vma->map_flags & BO_MAP_WRITE)
			set_domain.write_domain = I915_GEM_DOMAIN_GTT;
	}

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
	if (ret) {
		drv_log("DRM_IOCTL_I915_GEM_SET_DOMAIN with %d\n", ret);
		return ret;
	}

	return 0;
}

static int i915_bo_flush(struct bo *bo, struct mapping *mapping)
{
	struct i915_device *i915 = bo->drv->priv;
	if (!i915->has_llc && bo->meta.tiling == I915_TILING_NONE)
		i915_clflush(mapping->vma->addr, mapping->vma->length);

	return 0;
}

static uint32_t i915_resolve_format(struct driver *drv, uint32_t format, uint64_t use_flags)
{
	switch (format) {
	case DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/* KBL camera subsystem requires NV12. */
		if (use_flags & (BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE))
			return DRM_FORMAT_NV12;
		/*HACK: See b/28671744 */
		return DRM_FORMAT_XBGR8888;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		/*
		 * KBL camera subsystem requires NV12. Our other use cases
		 * don't care:
		 * - Hardware video supports NV12,
		 * - USB Camera HALv3 supports NV12,
		 * - USB Camera HALv1 doesn't use this format.
		 * Moreover, NV12 is preferred for video, due to overlay
		 * support on SKL+.
		 */
		return DRM_FORMAT_NV12;
	default:
		return format;
	}
}

const struct backend backend_i915 = {
	.name = "i915",
	.init = i915_init,
	.close = i915_close,
	.bo_compute_metadata = i915_bo_compute_metadata,
	.bo_create_from_metadata = i915_bo_create_from_metadata,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_import = i915_bo_import,
	.bo_map = i915_bo_map,
	.bo_unmap = drv_bo_munmap,
	.bo_invalidate = i915_bo_invalidate,
	.bo_flush = i915_bo_flush,
	.resolve_format = i915_resolve_format,
};

#endif
