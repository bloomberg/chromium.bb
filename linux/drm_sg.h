/**
 * \file drm_sg.h 
 * IOCTLs to manage scatter/gather memory
 *
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


#ifndef _DRM_SCATTER_H_
#define _DRM_SCATTER_H_


/**
 * Scatter/gather memory entry.
 *
 * This structure is used by sg_alloc() sg_free() to store all the necessary
 * information about a scatter/gather memory block.
 * 
 * Also one instance of this structure is used to hold the user-space
 * allocation done via the sg_alloc_ioctl() and sg_free_ioctl() ioctl's.
 */
typedef struct drm_sg_mem {
	unsigned long   handle;
	void            *virtual;	/**< virtual address */
	int             pages;
	struct page     **pagelist;
	dma_addr_t	*busaddr;	/**< bus address */
} drm_sg_mem_t;


/** \name Prototypes */
/*@{*/
extern void DRM(sg_free)( drm_sg_mem_t *entry );
extern drm_sg_mem_t * DRM(sg_alloc)( unsigned long size );
extern int DRM(sg_alloc_ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
extern int DRM(sg_free_ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
extern void DRM(sg_cleanup)(drm_device_t *dev);
/*@}*/


#endif
