/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GBM_PRIV_H
#define GBM_PRIV_H

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include "gbm.h"

struct gbm_device
{
	int fd;
	struct gbm_driver *driver;
	void *priv;
};

struct gbm_surface
{
};

struct gbm_bo
{
	struct gbm_device *gbm;
	uint32_t width;
	uint32_t height;
	uint32_t size;
	uint32_t stride;
	uint32_t format;
	uint32_t tiling;
	union gbm_bo_handle handle;
	void *priv;
	void *user_data;
	void (*destroy_user_data)(struct gbm_bo *, void *);
};

struct gbm_driver
{
	char *name;
	int (*init)(struct gbm_device *gbm);
	void (*close)(struct gbm_device *gbm);
	int (*bo_create)(struct gbm_bo *bo, uint32_t width, uint32_t height, uint32_t format, uint32_t flags);
	int (*bo_destroy)(struct gbm_bo *bo);
	struct format_supported {
		uint32_t format;
		enum gbm_bo_flags usage;
	}
	format_list[10];
};

#endif
