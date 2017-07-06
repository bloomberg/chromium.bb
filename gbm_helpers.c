/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>
#include <stdio.h>

#include "drv.h"
#include "gbm.h"

uint64_t gbm_convert_flags(uint32_t flags)
{
	uint64_t usage = BO_USE_NONE;

	if (flags & GBM_BO_USE_SCANOUT)
		usage |= BO_USE_SCANOUT;
	if (flags & GBM_BO_USE_CURSOR)
		usage |= BO_USE_CURSOR;
	if (flags & GBM_BO_USE_CURSOR_64X64)
		usage |= BO_USE_CURSOR_64X64;
	if (flags & GBM_BO_USE_RENDERING)
		usage |= BO_USE_RENDERING;
	if (flags & GBM_BO_USE_TEXTURING)
		usage |= BO_USE_TEXTURE;
	if (flags & GBM_BO_USE_LINEAR)
		usage |= BO_USE_LINEAR;
	if (flags & GBM_BO_USE_CAMERA_WRITE)
		usage |= BO_USE_CAMERA_WRITE;
	if (flags & GBM_BO_USE_CAMERA_READ)
		usage |= BO_USE_CAMERA_READ;

	return usage;
}
