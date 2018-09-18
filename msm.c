/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_MSM

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

#define MESA_LLVMPIPE_TILE_ORDER 6
#define MESA_LLVMPIPE_TILE_SIZE (1 << MESA_LLVMPIPE_TILE_ORDER)

static const uint32_t render_target_formats[] = { DRM_FORMAT_ABGR8888, DRM_FORMAT_ARGB8888,
						  DRM_FORMAT_RGB565, DRM_FORMAT_XBGR8888,
						  DRM_FORMAT_XRGB8888 };

static const uint32_t supported_formats[] = { DRM_FORMAT_NV12, DRM_FORMAT_R8, DRM_FORMAT_YVU420,
					      DRM_FORMAT_YVU420_ANDROID };

static int msm_init(struct driver *drv)
{
	drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
			     &LINEAR_METADATA, BO_USE_RENDER_MASK);

	drv_add_combinations(drv, supported_formats, ARRAY_SIZE(supported_formats),
			     &LINEAR_METADATA, BO_USE_TEXTURE_MASK | BO_USE_HW_VIDEO_DECODER);

	/* Android CTS tests require this. */
	drv_add_combination(drv, DRM_FORMAT_BGR888, &LINEAR_METADATA, BO_USE_SW_MASK);

	return drv_modify_linear_combinations(drv);
}

static int msm_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
			 uint64_t flags)
{
	width = ALIGN(width, MESA_LLVMPIPE_TILE_SIZE);
	height = ALIGN(height, MESA_LLVMPIPE_TILE_SIZE);

	/*
	 * The extra 12KB at the end are a requirement of the Venus codec driver.
	 * Since |height| will be multiplied by 3/2 in drv_dumb_bo_create,
	 * we multiply this padding by 2/3 here.
	 */
	if (bo->format == DRM_FORMAT_NV12)
		height += 2 * DIV_ROUND_UP(0x3000, 3 * width);

	return drv_dumb_bo_create(bo, width, height, format, flags);
}

const struct backend backend_msm = {
	.name = "msm",
	.init = msm_init,
	.bo_create = msm_bo_create,
	.bo_destroy = drv_dumb_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = drv_dumb_bo_map,
	.bo_unmap = drv_bo_munmap,
};

#endif /* DRV_MSM */
