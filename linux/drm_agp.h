/**
 * \file drm_agp.h 
 * DRM support for AGP/GART backend
 *    
 * \author Rickard E. (Rik) Faith <faith@valinux.com>
 * \author Gareth Hughes <gareth@valinux.com>
 */

/*
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _DRM_AGP_H_
#define _DRM_AGP_H_


/** 
 * AGP memory entry.  Stored as a doubly linked list.
 */
typedef struct drm_agp_mem {
	unsigned long      handle;	/**< handle */
	agp_memory         *memory;	
	unsigned long      bound;	/**< address */
	int                pages;
	struct drm_agp_mem *prev;	/**< previous entry */
	struct drm_agp_mem *next;	/**< next entry */
} drm_agp_mem_t;

/**
 * AGP data.
 *
 * \sa DRM(agp_init)() and drm_device::agp.
 */
typedef struct drm_agp_head {
	agp_kern_info      agp_info;	/**< AGP device information */
	drm_agp_mem_t      *memory;	/**< memory entries */
	unsigned long      mode;	/**< AGP mode */
	int                enabled;	/**< whether the AGP bus as been enabled */
	int                acquired;	/**< whether the AGP device has been acquired */
	unsigned long      base;
   	int 		   agp_mtrr;
	int		   cant_use_aperture;
	unsigned long	   page_mask;
} drm_agp_head_t;


/** \name Prototypes */
/*@{*/

extern int DRM(agp_acquire)(void);
extern void DRM(agp_release)(void);
extern void DRM(agp_enable)(unsigned mode);
extern agp_memory *DRM(agp_allocate_memory)(size_t pages, u32 type);
extern int DRM(agp_free_memory)(agp_memory *handle);
extern int DRM(agp_bind_memory)(agp_memory *handle, off_t start);
extern int DRM(agp_unbind_memory)(agp_memory *handle);

extern int DRM(agp_acquire_ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
extern int DRM(agp_release_ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
extern int DRM(agp_enable_ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
extern int DRM(agp_info_ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
extern int DRM(agp_alloc_ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
extern int DRM(agp_free_ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
extern int DRM(agp_bind_ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
extern int DRM(agp_unbind_ioctl)(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

extern void DRM(agp_init)(void);
extern void DRM(agp_cleanup)(void);
extern void DRM(agp_init_dev)(drm_device_t *dev);
extern void DRM(agp_cleanup_dev)(drm_device_t *dev);

/*@}*/

#endif
