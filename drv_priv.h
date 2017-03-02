/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DRV_PRIV_H
#define DRV_PRIV_H

#include <pthread.h>
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
	size_t total_size;
	void *priv;
};

struct driver {
	int fd;
	struct backend *backend;
	void *priv;
	void *buffer_table;
	void *map_table;
	pthread_mutex_t driver_lock;
};

struct map_info {
	void *addr;
	size_t length;
	uint32_t handle;
	int32_t refcount;
	void *priv;
};

struct kms_item {
	uint32_t format;
	uint64_t modifier;
	uint64_t usage;
};

struct format_metadata {
	uint32_t priority;
	uint32_t tiling;
	uint64_t modifier;
};

struct combination {
	uint32_t format;
	struct format_metadata metadata;
	uint64_t usage;
};

struct combinations {
	struct combination *data;
	uint32_t size;
	uint32_t allocations;
};

struct backend
{
	char *name;
	int (*init)(struct driver *drv);
	void (*close)(struct driver *drv);
	int (*bo_create)(struct bo *bo, uint32_t width, uint32_t height,
			 uint32_t format, uint32_t flags);
	int (*bo_create_with_modifiers)(struct bo *bo,
					uint32_t width, uint32_t height,
					uint32_t format,
					const uint64_t *modifiers,
					uint32_t count);
	int (*bo_destroy)(struct bo *bo);
	int (*bo_import)(struct bo *bo, struct drv_import_fd_data *data);
	void* (*bo_map)(struct bo *bo, struct map_info *data, size_t plane);
	int (*bo_unmap)(struct bo *bo, struct map_info *data);
	uint32_t (*resolve_format)(uint32_t format);
	struct combinations combos;
};

#endif
