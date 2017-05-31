/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CROS_GRALLOC_TYPES_H
#define CROS_GRALLOC_TYPES_H

typedef enum {
	CROS_GRALLOC_ERROR_NONE = 0,
	CROS_GRALLOC_ERROR_BAD_DESCRIPTOR = 1,
	CROS_GRALLOC_ERROR_BAD_HANDLE = 2,
	CROS_GRALLOC_ERROR_BAD_VALUE = 3,
	CROS_GRALLOC_ERROR_NOT_SHARED = 4,
	CROS_GRALLOC_ERROR_NO_RESOURCES = 5,
	CROS_GRALLOC_ERROR_UNDEFINED = 6,
	CROS_GRALLOC_ERROR_UNSUPPORTED = 7,
} cros_gralloc_error_t;

struct cros_gralloc_buffer_descriptor {
	uint32_t width;
	uint32_t height;
	uint32_t consumer_usage;
	uint32_t producer_usage;
	uint32_t droid_format;
	uint32_t drm_format;
	uint64_t drv_usage;
};

#endif
