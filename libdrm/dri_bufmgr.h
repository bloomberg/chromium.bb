/**************************************************************************
 * 
 * Copyright © 2007 Intel Corporation
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND., USA
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * 
 **************************************************************************/
/*
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 *          Keith Whitwell <keithw-at-tungstengraphics-dot-com>
 *	    Eric Anholt <eric@anholt.net>
 */

#ifndef _DRI_BUFMGR_H_
#define _DRI_BUFMGR_H_
#include <xf86drm.h>

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

/**
 * Context for a buffer manager instance.
 *
 * Contains public methods followed by private storage for the buffer manager.
 */
struct _dri_bufmgr {
   /**
    * Allocate a buffer object.
    *
    * Buffer objects are not necessarily initially mapped into CPU virtual
    * address space or graphics device aperture.  They must be mapped using
    * bo_map() to be used by the CPU, and validated for use using bo_validate()
    * to be used from the graphics device.
    */
   dri_bo *(*bo_alloc)(dri_bufmgr *bufmgr_ctx, const char *name,
		       unsigned long size, unsigned int alignment);

   /** Takes a reference on a buffer object */
   void (*bo_reference)(dri_bo *bo);

   /**
    * Releases a reference on a buffer object, freeing the data if
    * rerefences remain.
    */
   void (*bo_unreference)(dri_bo *bo);

   /**
    * Maps the buffer into userspace.
    *
    * This function will block waiting for any existing execution on the
    * buffer to complete, first.  The resulting mapping is available at
    * buf->virtual.
    */
   int (*bo_map)(dri_bo *buf, int write_enable);

   /** Reduces the refcount on the userspace mapping of the buffer object. */
   int (*bo_unmap)(dri_bo *buf);

   /**
    * Write data into an object.
    *
    * This is an optional function, if missing,
    * dri_bo will map/memcpy/unmap.
    */
   int (*bo_subdata) (dri_bo *buf, unsigned long offset,
		      unsigned long size, const void *data);

   /**
    * Read data from an object
    *
    * This is an optional function, if missing,
    * dri_bo will map/memcpy/unmap.
    */
   int (*bo_get_subdata) (dri_bo *bo, unsigned long offset,
			  unsigned long size, void *data);

   /**
    * Waits for rendering to an object by the GPU to have completed.
    *
    * This is not required for any access to the BO by bo_map, bo_subdata, etc.
    * It is merely a way for the driver to implement glFinish.
    */
   void (*bo_wait_rendering) (dri_bo *bo);

   /**
    * Tears down the buffer manager instance.
    */
   void (*destroy)(dri_bufmgr *bufmgr);

   /**
    * Processes the relocations, either in userland or by converting the list
    * for use in batchbuffer submission.
    *
    * Kernel-based implementations will return a pointer to the arguments
    * to be handed with batchbuffer submission to the kernel.  The userland
    * implementation performs the buffer validation and emits relocations
    * into them the appopriate order.
    *
    * \param batch_buf buffer at the root of the tree of relocations
    * \return argument to be completed and passed to the execbuffers ioctl
    *   (if any).
    */
   void *(*process_relocs)(dri_bo *batch_buf);

   void (*post_submit)(dri_bo *batch_buf);

   int (*check_aperture_space)(dri_bo **bo_array, int count);
   int debug; /**< Enables verbose debugging printouts */
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

void *dri_process_relocs(dri_bo *batch_buf);
void dri_post_process_relocs(dri_bo *batch_buf);
void dri_post_submit(dri_bo *batch_buf);
int dri_bufmgr_check_aperture_space(dri_bo **bo_array, int count);

#endif
