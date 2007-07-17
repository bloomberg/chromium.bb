
/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.			
 *																			*
 * All Rights Reserved.														*
 *																			*
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the	
 * "Software"), to deal in the Software without restriction, including	
 * without limitation on the rights to use, copy, modify, merge,	
 * publish, distribute, sublicense, and/or sell copies of the Software,	
 * and to permit persons to whom the Software is furnished to do so,	
 * subject to the following conditions:					
 *																			*
 * The above copyright notice and this permission notice (including the	
 * next paragraph) shall be included in all copies or substantial	
 * portions of the Software.						
 *																			*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,	
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF	
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND		
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR			
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,		
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,		
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER			
 * DEALINGS IN THE SOFTWARE.												
 ***************************************************************************/

#ifndef _XGI_DRV_H_
#define _XGI_DRV_H_

#include "xgi_drm.h"

#define XGI_MAJOR_VERSION   0
#define XGI_MINOR_VERSION   7
#define XGI_PATCHLEVEL      5

#define XGI_DRV_VERSION     "0.7.5"

#ifndef XGI_DRV_NAME
#define XGI_DRV_NAME        "xgi"
#endif

/*
 * xgi reserved major device number, Set this to 0 to
 * request dynamic major number allocation.
 */
#ifndef XGI_DEV_MAJOR
#define XGI_DEV_MAJOR   0
#endif

#ifndef XGI_MAX_DEVICES
#define XGI_MAX_DEVICES 1
#endif

/* Jong 06/06/2006 */
/* #define XGI_DEBUG */

#ifndef PCI_VENDOR_ID_XGI
/*
#define PCI_VENDOR_ID_XGI       0x1023
*/
#define PCI_VENDOR_ID_XGI       0x18CA

#endif

#ifndef PCI_DEVICE_ID_XP5
#define PCI_DEVICE_ID_XP5       0x2200
#endif

#ifndef PCI_DEVICE_ID_XG47
#define PCI_DEVICE_ID_XG47      0x0047
#endif

/* Macros to make printk easier */
#define XGI_ERROR(fmt, arg...) \
    printk(KERN_ERR "[" XGI_DRV_NAME ":%s] *ERROR* " fmt, __FUNCTION__, ##arg)

#define XGI_MEM_ERROR(area, fmt, arg...) \
    printk(KERN_ERR "[" XGI_DRV_NAME ":%s] *ERROR* " fmt, __FUNCTION__, ##arg)

/* #define XGI_DEBUG */

#ifdef XGI_DEBUG
#define XGI_INFO(fmt, arg...) \
    printk(KERN_ALERT "[" XGI_DRV_NAME ":%s] " fmt, __FUNCTION__, ##arg)
/*    printk(KERN_INFO "[" XGI_DRV_NAME ":%s] " fmt, __FUNCTION__, ##arg) */
#else
#define XGI_INFO(fmt, arg...)   do { } while (0)
#endif

/* device name length; must be atleast 8 */
#define XGI_DEVICE_NAME_LENGTH      40

/* need a fake device number for control device; just to flag it for msgs */
#define XGI_CONTROL_DEVICE_NUMBER   100

struct xgi_aperture {
	unsigned long base;
	unsigned int size;
	void *vbase;
};

struct xgi_info {
	struct pci_dev *dev;
	int flags;
	int device_number;

	/* physical characteristics */
	struct xgi_aperture mmio;
	struct xgi_aperture fb;
	struct xgi_aperture pcie;

	/* look up table parameters */
	u32 *lut_base;
	unsigned int lutPageSize;
	unsigned int lutPageOrder;
	bool isLUTInLFB;
	unsigned int sdfbPageSize;

	u32 pcie_config;
	u32 pcie_status;

	atomic_t use_count;

	/* keep track of any pending bottom halfes */
	struct tasklet_struct tasklet;

	spinlock_t info_lock;

	struct semaphore info_sem;
	struct semaphore fb_sem;
	struct semaphore pcie_sem;
};

struct xgi_ioctl_post_vbios {
	unsigned int bus;
	unsigned int slot;
};

enum PcieOwner {
	PCIE_2D = 0,
	/*
	   PCIE_3D should not begin with 1,
	   2D alloc pcie memory will use owner 1.
	 */
	PCIE_3D = 11,		/*vetex buf */
	PCIE_3D_CMDLIST = 12,
	PCIE_3D_SCRATCHPAD = 13,
	PCIE_3D_TEXTURE = 14,
	PCIE_INVALID = 0x7fffffff
};

struct xgi_mem_pid {
	struct list_head list;
	enum xgi_mem_location location;
	unsigned long bus_addr;
	unsigned long pid;
};


/*
 * flags
 */
#define XGI_FLAG_OPEN            0x0001
#define XGI_FLAG_NEEDS_POSTING   0x0002
#define XGI_FLAG_WAS_POSTED      0x0004
#define XGI_FLAG_CONTROL         0x0010
#define XGI_FLAG_MAP_REGS_EARLY  0x0200

/* mmap(2) offsets */

#define IS_IO_OFFSET(info, offset, length) \
            (((offset) >= (info)->mmio.base) \
            && (((offset) + (length)) <= (info)->mmio.base + (info)->mmio.size))

/* Jong 06/14/2006 */
/* (info)->fb.base is a base address for physical (bus) address space */
/* what's the definition of offest? on  physical (bus) address space or HW address space */
/* Jong 06/15/2006; use HW address space */
#define IS_FB_OFFSET(info, offset, length) \
            (((offset) >= 0) \
            && (((offset) + (length)) <= (info)->fb.size))
#if 0
#define IS_FB_OFFSET(info, offset, length) \
            (((offset) >= (info)->fb.base) \
            && (((offset) + (length)) <= (info)->fb.base + (info)->fb.size))
#endif

#define IS_PCIE_OFFSET(info, offset, length) \
            (((offset) >= (info)->pcie.base) \
            && (((offset) + (length)) <= (info)->pcie.base + (info)->pcie.size))

extern int xgi_fb_heap_init(struct xgi_info * info);
extern void xgi_fb_heap_cleanup(struct xgi_info * info);

extern void xgi_fb_alloc(struct xgi_info * info, struct xgi_mem_alloc * alloc,
			 pid_t pid);
extern void xgi_fb_free(struct xgi_info * info, unsigned long offset);
extern void xgi_mem_collect(struct xgi_info * info, unsigned int *pcnt);

extern int xgi_pcie_heap_init(struct xgi_info * info);
extern void xgi_pcie_heap_cleanup(struct xgi_info * info);

extern void xgi_pcie_alloc(struct xgi_info * info, 
			   struct xgi_mem_alloc * alloc, pid_t pid);
extern void xgi_pcie_free(struct xgi_info * info, unsigned long offset);
extern struct xgi_pcie_block *xgi_find_pcie_block(struct xgi_info * info,
						    unsigned long address);
extern void *xgi_find_pcie_virt(struct xgi_info * info, unsigned long address);

extern void xgi_test_rwinkernel(struct xgi_info * info, unsigned long address);

#endif
