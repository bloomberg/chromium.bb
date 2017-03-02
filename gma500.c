/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

static const uint32_t supported_formats[] = {
	DRM_FORMAT_RGBX8888
};

static int gma500_init(struct driver *drv)
{
	return drv_add_linear_combinations(drv, supported_formats,
					   ARRAY_SIZE(supported_formats));
}

struct backend backend_gma500 =
{
	.name = "gma500",
	.init = gma500_init,
	.bo_create = drv_dumb_bo_create,
	.bo_destroy = drv_dumb_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = drv_dumb_bo_map,
};
