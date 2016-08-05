/*
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_MEDIATEK

#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <mediatek_drm.h>
#include <stdio.h>
#include "drv_priv.h"
#include "helpers.h"

static int drv_mediatek_bo_create(struct bo *bo,
				  uint32_t width, uint32_t height,
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

const struct backend backend_mediatek =
{
	.name = "mediatek",
	.bo_create = drv_mediatek_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.format_list = {
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING},
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR},
	}
};

#endif
