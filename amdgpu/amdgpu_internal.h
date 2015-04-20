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
 */

#ifndef _AMDGPU_INTERNAL_H_
#define _AMDGPU_INTERNAL_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <pthread.h>
#include "xf86atomic.h"
#include "amdgpu.h"
#include "util_double_list.h"

#define AMDGPU_CS_MAX_RINGS 8

struct amdgpu_bo_va_hole {
	struct list_head list;
	uint64_t offset;
	uint64_t size;
};

struct amdgpu_bo_va_mgr {
	/* the start virtual address */
	uint64_t va_offset;
	struct list_head va_holes;
	pthread_mutex_t bo_va_mutex;
	uint32_t va_alignment;
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
	/** List of buffer virtual memory ranges. Protected by bo_table_mutex. */
	struct util_hash_table *bo_vas;
	/** This protects all hash tables. */
	pthread_mutex_t bo_table_mutex;
	struct amdgpu_bo_va_mgr vamgr;
	struct drm_amdgpu_info_device dev_info;
	struct amdgpu_gpu_info info;
};

struct amdgpu_bo {
	atomic_t refcount;
	struct amdgpu_device *dev;

	uint64_t alloc_size;
	uint64_t virtual_mc_base_address;

	uint32_t handle;
	uint32_t flink_name;

	pthread_mutex_t cpu_access_mutex;
	void *cpu_ptr;
	int cpu_map_count;
};

/*
 * There are three mutexes.
 * To avoid deadlock, only hold the mutexes in this order:
 * sequence_mutex -> pendings_mutex -> pool_mutex.
*/
struct amdgpu_context {
	/** Mutex for accessing fences and to maintain command submissions
	    and pending lists in good sequence. */
	pthread_mutex_t sequence_mutex;
	/** Buffer for user fences */
	struct amdgpu_ib *fence_ib;
	/** The newest expired fence for the ring of the ip blocks. */
	uint64_t expired_fences[AMDGPU_HW_IP_NUM][AMDGPU_HW_IP_INSTANCE_MAX_COUNT][AMDGPU_CS_MAX_RINGS];
	/** Mutex for accessing pendings list. */
	pthread_mutex_t pendings_mutex;
	/** Pending IBs. */
	struct list_head pendings[AMDGPU_HW_IP_NUM][AMDGPU_HW_IP_INSTANCE_MAX_COUNT][AMDGPU_CS_MAX_RINGS];
	/** Freed IBs not yet in pool */
	struct list_head freed;
	/** Mutex for accessing free ib pool. */
	pthread_mutex_t pool_mutex;
	/** Internal free IB pools. */
	struct list_head ib_pools[AMDGPU_CS_IB_SIZE_NUM];
	/* context id*/
	uint32_t id;
};

struct amdgpu_ib {
	struct list_head list_node;
	amdgpu_bo_handle buf_handle;
	void *cpu;
	uint64_t virtual_mc_base_address;
	enum amdgpu_cs_ib_size ib_size;
	uint64_t cs_handle;
};

/**
 * Functions.
 */

void amdgpu_device_free_internal(amdgpu_device_handle dev);

void amdgpu_bo_free_internal(amdgpu_bo_handle bo);

void amdgpu_vamgr_init(struct amdgpu_device *dev);

uint64_t amdgpu_vamgr_find_va(struct amdgpu_bo_va_mgr *mgr,
                               uint64_t size, uint64_t alignment);

void amdgpu_vamgr_free_va(struct amdgpu_bo_va_mgr *mgr, uint64_t va,
                           uint64_t size);

int amdgpu_query_gpu_info_init(amdgpu_device_handle dev);

uint64_t amdgpu_cs_calculate_timeout(uint64_t timeout);

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

/**
 * Assignment between two amdgpu_device pointers with reference counting.
 *
 * Usage:
 *    struct amdgpu_device *dst = ... , *src = ...;
 *
 *    dst = src;
 *    // No reference counting. Only use this when you need to move
 *    // a reference from one pointer to another.
 *
 *    amdgpu_device_reference(&dst, src);
 *    // Reference counters are updated. dst is decremented and src is
 *    // incremented. dst is freed if its reference counter is 0.
 */
void amdgpu_device_reference(struct amdgpu_device **dst,
			     struct amdgpu_device *src);
#endif
