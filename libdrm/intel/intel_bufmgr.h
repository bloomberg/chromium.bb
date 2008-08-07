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

#ifndef INTEL_BUFMGR_GEM_H
#define INTEL_BUFMGR_GEM_H

#include "dri_bufmgr.h"

/**
 * Intel-specific bufmgr bits that follow immediately after the
 * generic bufmgr structure.
 */
struct intel_bufmgr {
    /**
     * Add relocation entry in reloc_buf, which will be updated with the
     * target buffer's real offset on on command submission.
     *
     * Relocations remain in place for the lifetime of the buffer object.
     *
     * \param reloc_buf Buffer to write the relocation into.
     * \param read_domains GEM read domains which the buffer will be read into
     *	      by the command that this relocation is part of.
     * \param write_domains GEM read domains which the buffer will be dirtied
     *	      in by the command that this relocation is part of.
     * \param delta Constant value to be added to the relocation target's
     *	       offset.
     * \param offset Byte offset within batch_buf of the relocated pointer.
     * \param target Buffer whose offset should be written into the relocation
     *	     entry.
     */
    int (*emit_reloc)(dri_bo *reloc_buf,
		      uint32_t read_domains, uint32_t write_domain,
		      uint32_t delta, uint32_t offset, dri_bo *target);
    /**
     * Pin a buffer to the aperture and fix the offset until unpinned
     *
     * \param buf Buffer to pin
     * \param alignment Required alignment for aperture, in bytes
     */
    int (*pin) (dri_bo *buf, uint32_t alignment);
    /**
     * Unpin a buffer from the aperture, allowing it to be removed
     *
     * \param buf Buffer to unpin
     */
    int (*unpin) (dri_bo *buf);
    /**
     * Ask that the buffer be placed in tiling mode
     *
     * \param buf Buffer to set tiling mode for
     * \param tiling_mode desired, and returned tiling mode
     */
    int (*set_tiling) (dri_bo *bo, uint32_t *tiling_mode);
    /**
     * Create a visible name for a buffer which can be used by other apps
     *
     * \param buf Buffer to create a name for
     * \param name Returned name
     */
    int (*flink) (dri_bo *buf, uint32_t *name);
};

/* intel_bufmgr_gem.c */
dri_bufmgr *intel_bufmgr_gem_init(int fd, int batch_size);
dri_bo *intel_bo_gem_create_from_name(dri_bufmgr *bufmgr, const char *name,
				      unsigned int handle);
void intel_bufmgr_gem_enable_reuse(dri_bufmgr *bufmgr);

/* intel_bufmgr_fake.c */
dri_bufmgr *intel_bufmgr_fake_init(unsigned long low_offset, void *low_virtual,
				   unsigned long size,
				   unsigned int (*fence_emit)(void *private),
				   int (*fence_wait)(void *private,
						     unsigned int cookie),
				   void *driver_priv);
dri_bo *intel_bo_fake_alloc_static(dri_bufmgr *bufmgr, const char *name,
				   unsigned long offset, unsigned long size,
				   void *virtual);

void intel_bufmgr_fake_contended_lock_take(dri_bufmgr *bufmgr);
void intel_bo_fake_disable_backing_store(dri_bo *bo,
					 void (*invalidate_cb)(dri_bo *bo,
							       void *ptr),
					 void *ptr);
void intel_bufmgr_fake_evict_all(dri_bufmgr *bufmgr);

int intel_bo_emit_reloc(dri_bo *reloc_buf,
			uint32_t read_domains, uint32_t write_domain,
			uint32_t delta, uint32_t offset, dri_bo *target_buf);

int intel_bo_pin(dri_bo *buf, uint32_t alignment);

int intel_bo_unpin(dri_bo *buf);

int intel_bo_set_tiling(dri_bo *buf, uint32_t *tiling_mode);

int intel_bo_flink(dri_bo *buf, uint32_t *name);

#endif /* INTEL_BUFMGR_GEM_H */

