/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_MSM

#include <errno.h>
#include <msm_drm.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

#define DEFAULT_ALIGNMENT 64
#define BUFFER_SIZE_ALIGN 4096

#define VENUS_STRIDE_ALIGN 128
#define VENUS_SCANLINE_ALIGN 16
#define NV12_LINEAR_PADDING (12 * 1024)

static const uint32_t render_target_formats[] = { DRM_FORMAT_ABGR8888, DRM_FORMAT_ARGB8888,
						  DRM_FORMAT_RGB565, DRM_FORMAT_XBGR8888,
						  DRM_FORMAT_XRGB8888 };

static const uint32_t texture_source_formats[] = { DRM_FORMAT_NV12, DRM_FORMAT_R8,
						   DRM_FORMAT_YVU420, DRM_FORMAT_YVU420_ANDROID };

static void msm_calculate_linear_layout(struct bo *bo)
{
	uint32_t width, height;

	width = bo->width;
	height = bo->height;

	/* NV12 format requires extra padding with platform
	 * specific alignments for venus driver
	 */
	if (bo->format == DRM_FORMAT_NV12) {
		uint32_t y_stride, uv_stride, y_scanline, uv_scanline, y_plane, uv_plane, size;
		y_stride = ALIGN(width, VENUS_STRIDE_ALIGN);
		uv_stride = ALIGN(width, VENUS_STRIDE_ALIGN);
		y_scanline = ALIGN(height, VENUS_SCANLINE_ALIGN * 2);
		uv_scanline = ALIGN(DIV_ROUND_UP(height, 2), VENUS_SCANLINE_ALIGN);
		y_plane = y_stride * y_scanline;
		uv_plane = uv_stride * uv_scanline;

		bo->strides[0] = y_stride;
		bo->sizes[0] = y_plane;
		bo->offsets[1] = y_plane;
		bo->strides[1] = uv_stride;
		size = y_plane + uv_plane + NV12_LINEAR_PADDING;
		bo->total_size = ALIGN(size, BUFFER_SIZE_ALIGN);
		bo->sizes[1] = bo->total_size - bo->sizes[0];
	} else {
		uint32_t stride, alignw, alignh;

		alignw = ALIGN(width, DEFAULT_ALIGNMENT);

		/* HAL_PIXEL_FORMAT_YV12 requires that the buffer's height not be aligned. */
		if (bo->format == DRM_FORMAT_YVU420_ANDROID) {
			alignh = height;
		} else {
			alignh = ALIGN(height, DEFAULT_ALIGNMENT);
		}

		stride = drv_stride_from_format(bo->format, alignw, 0);

		/* Calculate size and assign stride, size, offset to each plane based on format */
		drv_bo_from_format(bo, stride, alignh, bo->format);
	}
}

static int msm_init(struct driver *drv)
{
	drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
			     &LINEAR_METADATA, BO_USE_RENDER_MASK);

	drv_add_combinations(drv, texture_source_formats, ARRAY_SIZE(texture_source_formats),
			     &LINEAR_METADATA, BO_USE_TEXTURE_MASK | BO_USE_HW_VIDEO_DECODER);

	/* Android CTS tests require this. */
	drv_add_combination(drv, DRM_FORMAT_BGR888, &LINEAR_METADATA, BO_USE_SW_MASK);

	return drv_modify_linear_combinations(drv);
}

/* msm_bo_create will create linear buffers for now */
static int msm_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
			 uint64_t flags)
{
	struct drm_msm_gem_new req;
	int ret;
	size_t i;

	msm_calculate_linear_layout(bo);

	memset(&req, 0, sizeof(req));
	req.flags = MSM_BO_WC | MSM_BO_SCANOUT;
	req.size = bo->total_size;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MSM_GEM_NEW, &req);
	if (ret) {
		drv_log("DRM_IOCTL_MSM_GEM_NEW failed with %s\n", strerror(errno));
		return ret;
	}

	/*
	 * Though we use only one plane, we need to set handle for
	 * all planes to pass kernel checks
	 */
	for (i = 0; i < bo->num_planes; i++) {
		bo->handles[i].u32 = req.handle;
	}

	return 0;
}

static void *msm_bo_map(struct bo *bo, struct vma *vma, size_t plane, uint32_t map_flags)
{
	int ret;
	struct drm_msm_gem_info req;

	memset(&req, 0, sizeof(req));
	req.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MSM_GEM_INFO, &req);
	if (ret) {
		drv_log("DRM_IOCLT_MSM_GEM_INFO failed with %s\n", strerror(errno));
		return MAP_FAILED;
	}
	vma->length = bo->total_size;

	return mmap(0, bo->total_size, drv_get_prot(map_flags), MAP_SHARED, bo->drv->fd,
		    req.offset);
}

const struct backend backend_msm = {
	.name = "msm",
	.init = msm_init,
	.bo_create = msm_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = msm_bo_map,
	.bo_unmap = drv_bo_munmap,
};
#endif /* DRV_MSM */
