/*
 * Copyright © 2008 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

/**
 * @file intel_bufmgr.h
 *
 * Public definitions of Intel-specific bufmgr functions.
 */

#ifndef INTEL_BUFMGR_H
#define INTEL_BUFMGR_H

#include <stdint.h>

typedef struct _dri_bufmgr dri_bufmgr;
typedef struct _dri_bo dri_bo;

struct _dri_bo {
    /**
     * Size in bytes of the buffer object.
     *
     * The size may be larger than the size originally requested for the
     * allocation, such as being aligned to page size.
     */
    unsigned long size;
    /**
     * Card virtual address (offset from the beginning of the aperture) for the
     * object.  Only valid while validated.
     */
    unsigned long offset;
    /**
     * Virtual address for accessing the buffer data.  Only valid while mapped.
     */
    void *virtual;

    /** Buffer manager context associated with this buffer object */
    dri_bufmgr *bufmgr;
    /**
     * MM-specific handle for accessing object
     */
    int handle;
};

dri_bo *dri_bo_alloc(dri_bufmgr *bufmgr, const char *name, unsigned long size,
		     unsigned int alignment);
void dri_bo_reference(dri_bo *bo);
void dri_bo_unreference(dri_bo *bo);
int dri_bo_map(dri_bo *buf, int write_enable);
int dri_bo_unmap(dri_bo *buf);

int dri_bo_subdata(dri_bo *bo, unsigned long offset,
		   unsigned long size, const void *data);
int dri_bo_get_subdata(dri_bo *bo, unsigned long offset,
		       unsigned long size, void *data);
void dri_bo_wait_rendering(dri_bo *bo);

void dri_bufmgr_set_debug(dri_bufmgr *bufmgr, int enable_debug);
void dri_bufmgr_destroy(dri_bufmgr *bufmgr);
int dri_bo_exec(dri_bo *bo, int used,
		drm_clip_rect_t *cliprects, int num_cliprects,
		int DR4);
int dri_bufmgr_check_aperture_space(dri_bo **bo_array, int count);

int dri_bo_emit_reloc(dri_bo *reloc_buf,
		      uint32_t read_domains, uint32_t write_domain,
		      uint32_t delta, uint32_t offset, dri_bo *target_buf);
int dri_bo_pin(dri_bo *buf, uint32_t alignment);
int dri_bo_unpin(dri_bo *buf);
int dri_bo_set_tiling(dri_bo *buf, uint32_t *tiling_mode);
int dri_bo_flink(dri_bo *buf, uint32_t *name);

/* intel_bufmgr_gem.c */
dri_bufmgr *intel_bufmgr_gem_init(int fd, int batch_size);
dri_bo *intel_bo_gem_create_from_name(dri_bufmgr *bufmgr, const char *name,
				      unsigned int handle);
void intel_bufmgr_gem_enable_reuse(dri_bufmgr *bufmgr);

/* intel_bufmgr_fake.c */
dri_bufmgr *intel_bufmgr_fake_init(int fd,
				   unsigned long low_offset, void *low_virtual,
				   unsigned long size,
				   volatile unsigned int *last_dispatch);
void intel_bufmgr_fake_set_last_dispatch(dri_bufmgr *bufmgr,
					 volatile unsigned int *last_dispatch);
void intel_bufmgr_fake_set_exec_callback(dri_bufmgr *bufmgr,
					 int (*exec)(dri_bo *bo,
						     unsigned int used,
						     void *priv),
					 void *priv);
void intel_bufmgr_fake_set_fence_callback(dri_bufmgr *bufmgr,
					  unsigned int (*emit)(void *priv),
					  void (*wait)(unsigned int fence,
						       void *priv),
					  void *priv);
dri_bo *intel_bo_fake_alloc_static(dri_bufmgr *bufmgr, const char *name,
				   unsigned long offset, unsigned long size,
				   void *virtual);
void intel_bo_fake_disable_backing_store(dri_bo *bo,
					 void (*invalidate_cb)(dri_bo *bo,
							       void *ptr),
					 void *ptr);

void intel_bufmgr_fake_contended_lock_take(dri_bufmgr *bufmgr);
void intel_bufmgr_fake_evict_all(dri_bufmgr *bufmgr);

#endif /* INTEL_BUFMGR_H */

