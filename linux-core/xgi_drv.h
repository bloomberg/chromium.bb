
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
	U32 base;		// pcie base is different from fb base
	U32 size;
	u8 *vbase;
};

struct xgi_screen_info {
	U32 scrn_start;
	U32 scrn_xres;
	U32 scrn_yres;
	U32 scrn_bpp;
	U32 scrn_pitch;
};

struct xgi_sarea_info {
	U32 bus_addr;
	U32 size;
};

struct xgi_info {
	struct pci_dev *dev;
	int flags;
	int device_number;
	int bus;		/* PCI config info */
	int slot;
	int vendor_id;
	U32 device_id;
	u8 revision_id;

	/* physical characteristics */
	struct xgi_aperture mmio;
	struct xgi_aperture fb;
	struct xgi_aperture pcie;
	struct xgi_screen_info scrn_info;
	struct xgi_sarea_info sarea_info;

	/* look up table parameters */
	U32 *lut_base;
	unsigned int lutPageSize;
	unsigned int lutPageOrder;
	bool isLUTInLFB;
	unsigned int sdfbPageSize;

	U32 pcie_config;
	U32 pcie_status;
	U32 irq;

	atomic_t use_count;

	/* keep track of any pending bottom halfes */
	struct tasklet_struct tasklet;

	spinlock_t info_lock;

	struct semaphore info_sem;
	struct semaphore fb_sem;
	struct semaphore pcie_sem;
};

struct xgi_ioctl_post_vbios {
	U32 bus;
	U32 slot;
};

enum xgi_mem_location {
	NON_LOCAL = 0,
	LOCAL = 1,
	INVALID = 0x7fffffff
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

struct xgi_mem_req {
	enum xgi_mem_location location;
	unsigned long size;
	unsigned long is_front;
	enum PcieOwner owner;
	unsigned long pid;
};

struct xgi_mem_alloc {
	enum xgi_mem_location location;
	unsigned long size;
	unsigned long bus_addr;
	unsigned long hw_addr;
	unsigned long pid;
};

struct xgi_chip_info {
	U32 device_id;
	char device_name[32];
	U32 vendor_id;
	U32 curr_display_mode;	//Singe, DualView(Contained), MHS
	U32 fb_size;
	U32 sarea_bus_addr;
	U32 sarea_size;
};

struct xgi_opengl_cmd {
	U32 cmd;
};

struct xgi_mmio_info {
	struct xgi_opengl_cmd cmd_head;
	void *mmioBase;
	int size;
};

typedef enum {
	BTYPE_2D = 0,
	BTYPE_3D = 1,
	BTYPE_FLIP = 2,
	BTYPE_CTRL = 3,
	BTYPE_NONE = 0x7fffffff
} BATCH_TYPE;

struct xgi_cmd_info {
	BATCH_TYPE _firstBeginType;
	U32 _firstBeginAddr;
	U32 _firstSize;
	U32 _curDebugID;
	U32 _lastBeginAddr;
	U32 _beginCount;
};

struct xgi_state_info {
	U32 _fromState;
	U32 _toState;
};

struct cpu_info {
	U32 _eax;
	U32 _ebx;
	U32 _ecx;
	U32 _edx;
};

struct xgi_mem_pid {
	struct list_head list;
	enum xgi_mem_location location;
	unsigned long bus_addr;
	unsigned long pid;
};

/*
 * Ioctl definitions
 */

#define XGI_IOCTL_MAGIC             'x'	/* use 'x' as magic number */

#define XGI_IOCTL_BASE              0
#define XGI_ESC_DEVICE_INFO         (XGI_IOCTL_BASE + 0)
#define XGI_ESC_POST_VBIOS          (XGI_IOCTL_BASE + 1)

#define XGI_ESC_FB_INIT             (XGI_IOCTL_BASE + 2)
#define XGI_ESC_FB_ALLOC            (XGI_IOCTL_BASE + 3)
#define XGI_ESC_FB_FREE             (XGI_IOCTL_BASE + 4)
#define XGI_ESC_PCIE_INIT           (XGI_IOCTL_BASE + 5)
#define XGI_ESC_PCIE_ALLOC          (XGI_IOCTL_BASE + 6)
#define XGI_ESC_PCIE_FREE           (XGI_IOCTL_BASE + 7)
#define XGI_ESC_SUBMIT_CMDLIST      (XGI_IOCTL_BASE + 8)
#define XGI_ESC_PUT_SCREEN_INFO     (XGI_IOCTL_BASE + 9)
#define XGI_ESC_GET_SCREEN_INFO     (XGI_IOCTL_BASE + 10)
#define XGI_ESC_GE_RESET            (XGI_IOCTL_BASE + 11)
#define XGI_ESC_SAREA_INFO          (XGI_IOCTL_BASE + 12)
#define XGI_ESC_DUMP_REGISTER       (XGI_IOCTL_BASE + 13)
#define XGI_ESC_DEBUG_INFO          (XGI_IOCTL_BASE + 14)
#define XGI_ESC_TEST_RWINKERNEL     (XGI_IOCTL_BASE + 16)
#define XGI_ESC_STATE_CHANGE        (XGI_IOCTL_BASE + 17)
#define XGI_ESC_MMIO_INFO           (XGI_IOCTL_BASE + 18)
#define XGI_ESC_PCIE_CHECK          (XGI_IOCTL_BASE + 19)
#define XGI_ESC_CPUID               (XGI_IOCTL_BASE + 20)
#define XGI_ESC_MEM_COLLECT          (XGI_IOCTL_BASE + 21)

#define XGI_IOCTL_DEVICE_INFO       _IOR(XGI_IOCTL_MAGIC, XGI_ESC_DEVICE_INFO, struct xgi_chip_info)
#define XGI_IOCTL_POST_VBIOS        _IO(XGI_IOCTL_MAGIC, XGI_ESC_POST_VBIOS)

#define XGI_IOCTL_FB_INIT           _IO(XGI_IOCTL_MAGIC, XGI_ESC_FB_INIT)
#define XGI_IOCTL_FB_ALLOC          _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_FB_ALLOC, struct xgi_mem_req)
#define XGI_IOCTL_FB_FREE           _IOW(XGI_IOCTL_MAGIC, XGI_ESC_FB_FREE, unsigned long)

#define XGI_IOCTL_PCIE_INIT         _IO(XGI_IOCTL_MAGIC, XGI_ESC_PCIE_INIT)
#define XGI_IOCTL_PCIE_ALLOC        _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_PCIE_ALLOC, struct xgi_mem_req)
#define XGI_IOCTL_PCIE_FREE         _IOW(XGI_IOCTL_MAGIC, XGI_ESC_PCIE_FREE, unsigned long)

#define XGI_IOCTL_PUT_SCREEN_INFO   _IOW(XGI_IOCTL_MAGIC, XGI_ESC_PUT_SCREEN_INFO, struct xgi_screen_info)
#define XGI_IOCTL_GET_SCREEN_INFO   _IOR(XGI_IOCTL_MAGIC, XGI_ESC_GET_SCREEN_INFO, struct xgi_screen_info)

#define XGI_IOCTL_GE_RESET          _IO(XGI_IOCTL_MAGIC, XGI_ESC_GE_RESET)
#define XGI_IOCTL_SAREA_INFO        _IOW(XGI_IOCTL_MAGIC, XGI_ESC_SAREA_INFO, struct xgi_sarea_info)
#define XGI_IOCTL_DUMP_REGISTER     _IO(XGI_IOCTL_MAGIC, XGI_ESC_DUMP_REGISTER)
#define XGI_IOCTL_DEBUG_INFO        _IO(XGI_IOCTL_MAGIC, XGI_ESC_DEBUG_INFO)
#define XGI_IOCTL_MMIO_INFO         _IOR(XGI_IOCTL_MAGIC, XGI_ESC_MMIO_INFO, struct xgi_mmio_info)

#define XGI_IOCTL_SUBMIT_CMDLIST	_IOWR(XGI_IOCTL_MAGIC, XGI_ESC_SUBMIT_CMDLIST, struct xgi_cmd_info)
#define XGI_IOCTL_TEST_RWINKERNEL	_IOWR(XGI_IOCTL_MAGIC, XGI_ESC_TEST_RWINKERNEL, unsigned long)
#define XGI_IOCTL_STATE_CHANGE      _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_STATE_CHANGE, struct xgi_state_info)

#define XGI_IOCTL_PCIE_CHECK        _IO(XGI_IOCTL_MAGIC, XGI_ESC_PCIE_CHECK)
#define XGI_IOCTL_CPUID             _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_CPUID, struct cpu_info)
#define XGI_IOCTL_MAXNR          30

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

extern void xgi_fb_alloc(struct xgi_info * info, struct xgi_mem_req * req,
			 struct xgi_mem_alloc * alloc);
extern void xgi_fb_free(struct xgi_info * info, unsigned long offset);
extern void xgi_mem_collect(struct xgi_info * info, unsigned int *pcnt);

extern int xgi_pcie_heap_init(struct xgi_info * info);
extern void xgi_pcie_heap_cleanup(struct xgi_info * info);

extern void xgi_pcie_alloc(struct xgi_info * info, unsigned long size,
			   enum PcieOwner owner, struct xgi_mem_alloc * alloc);
extern void xgi_pcie_free(struct xgi_info * info, unsigned long offset);
extern void xgi_pcie_heap_check(void);
extern struct xgi_pcie_block *xgi_find_pcie_block(struct xgi_info * info,
						    unsigned long address);
extern void *xgi_find_pcie_virt(struct xgi_info * info, unsigned long address);

extern void xgi_read_pcie_mem(struct xgi_info * info, struct xgi_mem_req * req);
extern void xgi_write_pcie_mem(struct xgi_info * info, struct xgi_mem_req * req);

extern void xgi_test_rwinkernel(struct xgi_info * info, unsigned long address);

#endif
