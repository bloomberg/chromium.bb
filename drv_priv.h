/*
 * Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DRV_PRIV_H
#define DRV_PRIV_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#include "drv.h"

struct bo
{
	struct driver *drv;
	uint32_t width;
	uint32_t height;
	uint32_t format;
	uint32_t tiling;
	size_t num_planes;
	union bo_handle handles[DRV_MAX_PLANES];
	uint32_t offsets[DRV_MAX_PLANES];
	uint32_t sizes[DRV_MAX_PLANES];
	uint32_t strides[DRV_MAX_PLANES];
	uint64_t format_modifiers[DRV_MAX_PLANES];
	void *priv;
};

struct driver {
	int fd;
	struct backend *backend;
	void *priv;
	void *buffer_table;
	pthread_mutex_t table_lock;
};

struct backend
{
	char *name;
	int (*init)(struct driver *drv);
	void (*close)(struct driver *drv);
	int (*bo_create)(struct bo *bo, uint32_t width, uint32_t height,
			 drv_format_t format, uint32_t flags);
	int (*bo_destroy)(struct bo *bo);
	struct format_supported {
		drv_format_t format;
		uint64_t usage;
	}
	format_list[18];
};

#endif
