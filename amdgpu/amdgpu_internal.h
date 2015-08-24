/*
 * Copyright © 2014 Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _AMDGPU_INTERNAL_H_
#define _AMDGPU_INTERNAL_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <pthread.h>

#include "libdrm_macros.h"
#include "xf86atomic.h"
#include "amdgpu.h"
#include "util_double_list.h"

#define AMDGPU_CS_MAX_RINGS 8
/* do not use below macro if b is not power of 2 aligned value */
#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define ROUND_UP(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define ROUND_DOWN(x, y) ((x) & ~__round_mask(x, y))

#define AMDGPU_INVALID_VA_ADDRESS	0xffffffffffffffff

struct amdgpu_bo_va_hole {
	struct list_head list;
	uint64_t offset;
	uint64_t size;
};

struct amdgpu_bo_va_mgr {
	/* the start virtual address */
	uint64_t va_offset;
	uint64_t va_max;
	struct list_head va_holes;
	pthread_mutex_t bo_va_mutex;
	uint32_t va_alignment;
};

struct amdgpu_va {
	amdgpu_device_handle dev;
	uint64_t address;
	uint64_t size;
	enum amdgpu_gpu_va_range range;
	struct amdgpu_bo_va_mgr *vamgr;
};

struct amdgpu_device {
	atomic_t refcount;
	int fd;
	int flink_fd;
	unsigned major_version;
	unsigned minor_version;

	/** List of buffer handles. Protected by bo_table_mutex. */
	struct util_hash_table *bo_handles;
	/** List of buffer GEM flink names. Protected by bo_table_mutex. */
	struct util_hash_table *bo_flink_names;
	/** This protects all hash tables. */
	pthread_mutex_t bo_table_mutex;
	struct drm_amdgpu_info_device dev_info;
	struct amdgpu_gpu_info info;
	/** The global VA manager for the whole virtual address space */
	struct amdgpu_bo_va_mgr *vamgr;
	/** The VA manager for the 32bit address space */
	struct amdgpu_bo_va_mgr *vamgr_32;
};

struct amdgpu_bo {
	atomic_t refcount;
	struct amdgpu_device *dev;

	uint64_t alloc_size;

	uint32_t handle;
	uint32_t flink_name;

	pthread_mutex_t cpu_access_mutex;
	void *cpu_ptr;
	int cpu_map_count;
};

struct amdgpu_bo_list {
	struct amdgpu_device *dev;

	uint32_t handle;
};

struct amdgpu_context {
	struct amdgpu_device *dev;
	/* context id*/
	uint32_t id;
};

/**
 * Functions.
 */

drm_private void amdgpu_bo_free_internal(amdgpu_bo_handle bo);

drm_private void amdgpu_vamgr_init(struct amdgpu_bo_va_mgr *mgr, uint64_t start,
		       uint64_t max, uint64_t alignment);

drm_private void amdgpu_vamgr_deinit(struct amdgpu_bo_va_mgr *mgr);

drm_private uint64_t
amdgpu_vamgr_find_va(struct amdgpu_bo_va_mgr *mgr, uint64_t size,
		     uint64_t alignment, uint64_t base_required);

drm_private void
amdgpu_vamgr_free_va(struct amdgpu_bo_va_mgr *mgr, uint64_t va, uint64_t size);

drm_private int amdgpu_query_gpu_info_init(amdgpu_device_handle dev);

drm_private uint64_t amdgpu_cs_calculate_timeout(uint64_t timeout);

/**
 * Inline functions.
 */

/**
 * Increment src and decrement dst as if we were updating references
 * for an assignment between 2 pointers of some objects.
 *
 * \return  true if dst is 0
 */
static inline bool update_references(atomic_t *dst, atomic_t *src)
{
	if (dst != src) {
		/* bump src first */
		if (src) {
			assert(atomic_read(src) > 0);
			atomic_inc(src);
		}
		if (dst) {
			assert(atomic_read(dst) > 0);
			return atomic_dec_and_test(dst);
		}
	}
	return false;
}

/**
 * Assignment between two amdgpu_bo pointers with reference counting.
 *
 * Usage:
 *    struct amdgpu_bo *dst = ... , *src = ...;
 *
 *    dst = src;
 *    // No reference counting. Only use this when you need to move
 *    // a reference from one pointer to another.
 *
 *    amdgpu_bo_reference(&dst, src);
 *    // Reference counters are updated. dst is decremented and src is
 *    // incremented. dst is freed if its reference counter is 0.
 */
static inline void amdgpu_bo_reference(struct amdgpu_bo **dst,
					struct amdgpu_bo *src)
{
	if (update_references(&(*dst)->refcount, &src->refcount))
		amdgpu_bo_free_internal(*dst);
	*dst = src;
}

#endif
