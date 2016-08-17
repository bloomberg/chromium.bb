/*
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_MEDIATEK

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <mediatek_drm.h>

#include "drv_priv.h"
#include "helpers.h"

static int mediatek_bo_create(struct bo *bo, uint32_t width, uint32_t height,
			      uint32_t format, uint32_t flags)
{
	size_t size;
	struct drm_mtk_gem_create gem_create;
	int ret;

	bo->strides[0] = drv_stride_from_format(format, width);
	size = height * bo->strides[0];

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = size;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MTK_GEM_CREATE, &gem_create);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_MTK_GEM_CREATE failed "
				"(size=%zu)\n", size);
		return ret;
	}

	bo->handles[0].u32 = gem_create.handle;
	bo->sizes[0] = size;
	bo->offsets[0] = 0;

	return 0;
}

static void *mediatek_bo_map(struct bo *bo)
{
	int ret;
	struct drm_mtk_gem_map_off gem_map;

	memset(&gem_map, 0, sizeof(gem_map));
	gem_map.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MTK_GEM_MAP_OFFSET, &gem_map);
	if (ret) {
		fprintf(stderr,"drv: DRM_IOCTL_MTK_GEM_MAP_OFFSET failed\n");
		return MAP_FAILED;
	}

	return mmap(0, bo->sizes[0], PROT_READ | PROT_WRITE, MAP_SHARED,
		    bo->drv->fd, gem_map.offset);
}

const struct backend backend_mediatek =
{
	.name = "mediatek",
	.bo_create = mediatek_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_map = mediatek_bo_map,
	.format_list = {
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR |
				      DRV_BO_USE_RENDERING | DRV_BO_USE_HW_TEXTURE |
				      DRV_BO_USE_HW_RENDER | DRV_BO_USE_HW_2D |
				      DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR |
				      DRV_BO_USE_LINEAR | DRV_BO_USE_SW_READ_OFTEN |
				      DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR |
				      DRV_BO_USE_RENDERING | DRV_BO_USE_HW_TEXTURE |
				      DRV_BO_USE_HW_RENDER | DRV_BO_USE_HW_2D |
				      DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR |
				      DRV_BO_USE_LINEAR | DRV_BO_USE_SW_READ_OFTEN |
				      DRV_BO_USE_SW_WRITE_OFTEN},
	}
};

#endif
