/* drmP.h -- Private header for Direct Rendering Manager -*- linux-c -*-
 * Created: Mon Jan  4 10:05:05 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All rights reserved.
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
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#ifndef _DRM_P_H_
#define _DRM_P_H_

#if defined(_KERNEL) || defined(__KERNEL__)

#define DRM_DEBUG_CODE 0	  /* Include debugging code (if > 1, then
				     also include looping detection. */

typedef struct drm_device drm_device_t;
typedef struct drm_file drm_file_t;

/* There's undoubtably more of this file to go into these OS dependent ones. */

#ifdef __FreeBSD__
#include "drm_os_freebsd.h"
#elif defined __NetBSD__
#include "drm_os_netbsd.h"
#endif

#include "drm.h"

/* Begin the DRM... */

#define DRM_HASH_SIZE	      16 /* Size of key hash table		  */
#define DRM_KERNEL_CONTEXT    0	 /* Change drm_resctx if changed	  */
#define DRM_RESERVED_CONTEXTS 1	 /* Change drm_resctx if changed	  */

#define DRM_FLAG_DEBUG	  0x01

#define DRM_MEM_DMA	   0
#define DRM_MEM_SAREA	   1
#define DRM_MEM_DRIVER	   2
#define DRM_MEM_MAGIC	   3
#define DRM_MEM_IOCTLS	   4
#define DRM_MEM_MAPS	   5
#define DRM_MEM_BUFS	   6
#define DRM_MEM_SEGS	   7
#define DRM_MEM_PAGES	   8
#define DRM_MEM_FILES	  9
#define DRM_MEM_QUEUES	  10
#define DRM_MEM_CMDS	  11
#define DRM_MEM_MAPPINGS  12
#define DRM_MEM_BUFLISTS  13
#define DRM_MEM_AGPLISTS  14
#define DRM_MEM_TOTALAGP  15
#define DRM_MEM_BOUNDAGP  16
#define DRM_MEM_CTXBITMAP 17
#define DRM_MEM_STUB	  18
#define DRM_MEM_SGLISTS	  19

#define DRM_MAX_CTXBITMAP (PAGE_SIZE * 8)

				/* Internal types and structures */
#define DRM_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define DRM_MIN(a,b) ((a)<(b)?(a):(b))
#define DRM_MAX(a,b) ((a)>(b)?(a):(b))

#define DRM_IF_VERSION(maj, min) (maj << 16 | min)

#define DRM_GET_PRIV_SAREA(_dev, _ctx, _map) do {	\
	(_map) = (_dev)->context_sareas[_ctx];		\
} while(0)


typedef struct drm_pci_id_list
{
	int vendor;
	int device;
	long driver_private;
	char *name;
} drm_pci_id_list_t;

typedef struct drm_ioctl_desc {
	int		     (*func)(DRM_IOCTL_ARGS);
	int		     auth_needed;
	int		     root_only;
} drm_ioctl_desc_t;

typedef struct drm_magic_entry {
	drm_magic_t	       magic;
	struct drm_file	       *priv;
	struct drm_magic_entry *next;
} drm_magic_entry_t;

typedef struct drm_magic_head {
	struct drm_magic_entry *head;
	struct drm_magic_entry *tail;
} drm_magic_head_t;

typedef struct drm_buf {
	int		  idx;	       /* Index into master buflist	     */
	int		  total;       /* Buffer size			     */
	int		  order;       /* log-base-2(total)		     */
	int		  used;	       /* Amount of buffer in use (for DMA)  */
	unsigned long	  offset;      /* Byte offset (used internally)	     */
	void		  *address;    /* Address of buffer		     */
	unsigned long	  bus_address; /* Bus address of buffer		     */
	struct drm_buf	  *next;       /* Kernel-only: used for free list    */
	__volatile__ int  pending;     /* On hardware DMA queue		     */
	DRMFILE		  filp;	       /* Unique identifier of holding process */
	int		  context;     /* Kernel queue for this buffer	     */
	enum {
		DRM_LIST_NONE	 = 0,
		DRM_LIST_FREE	 = 1,
		DRM_LIST_WAIT	 = 2,
		DRM_LIST_PEND	 = 3,
		DRM_LIST_PRIO	 = 4,
		DRM_LIST_RECLAIM = 5
	}		  list;	       /* Which list we're on		     */

	int		  dev_priv_size; /* Size of buffer private stoarge   */
	void		  *dev_private;  /* Per-buffer private storage       */
} drm_buf_t;

typedef struct drm_freelist {
	int		  initialized; /* Freelist in use		   */
	atomic_t	  count;       /* Number of free buffers	   */
	drm_buf_t	  *next;       /* End pointer			   */

	int		  low_mark;    /* Low water mark		   */
	int		  high_mark;   /* High water mark		   */
} drm_freelist_t;

typedef struct drm_buf_entry {
	int		  buf_size;
	int		  buf_count;
	drm_buf_t	  *buflist;
	int		  seg_count;
	int		  page_order;
	vm_offset_t	  *seglist;
	dma_addr_t	  *seglist_bus;

	drm_freelist_t	  freelist;
} drm_buf_entry_t;

typedef TAILQ_HEAD(drm_file_list, drm_file) drm_file_list_t;
struct drm_file {
	TAILQ_ENTRY(drm_file) link;
	int		  authenticated;
	int		  minor;
	pid_t		  pid;
	uid_t		  uid;
	int		  refs;
	drm_magic_t	  magic;
	unsigned long	  ioctl_count;
	void		 *driver_priv;
#ifdef DRIVER_FILE_FIELDS
	DRIVER_FILE_FIELDS;
#endif
};

typedef struct drm_lock_data {
	drm_hw_lock_t	  *hw_lock;	/* Hardware lock		   */
	DRMFILE		  filp;	        /* Unique identifier of holding process (NULL is kernel)*/
	int		  lock_queue;	/* Queue of blocked processes	   */
	unsigned long	  lock_time;	/* Time of last lock in jiffies	   */
} drm_lock_data_t;

/* This structure, in the drm_device_t, is always initialized while the device
 * is open.  dev->dma_lock protects the incrementing of dev->buf_use, which
 * when set marks that no further bufs may be allocated until device teardown
 * occurs (when the last open of the device has closed).  The high/low
 * watermarks of bufs are only touched by the X Server, and thus not
 * concurrently accessed, so no locking is needed.
 */
typedef struct drm_device_dma {
	drm_buf_entry_t	  bufs[DRM_MAX_ORDER+1];
	int		  buf_count;
	drm_buf_t	  **buflist;	/* Vector of pointers info bufs	   */
	int		  seg_count;
	int		  page_count;
	unsigned long	  *pagelist;
	unsigned long	  byte_count;
	enum {
		_DRM_DMA_USE_AGP = 0x01,
		_DRM_DMA_USE_SG  = 0x02
	} flags;

				/* DMA support */
	drm_buf_t	  *this_buffer;	/* Buffer being sent		   */
	drm_buf_t	  *next_buffer; /* Selected buffer to send	   */
} drm_device_dma_t;

typedef struct drm_agp_mem {
	void               *handle;
	unsigned long      bound; /* address */
	int                pages;
	struct drm_agp_mem *prev;
	struct drm_agp_mem *next;
} drm_agp_mem_t;

typedef struct drm_agp_head {
	device_t	   agpdev;
	struct agp_info    info;
	const char         *chipset;
	drm_agp_mem_t      *memory;
	unsigned long      mode;
	int                enabled;
	int                acquired;
	unsigned long      base;
   	int 		   mtrr;
	int		   cant_use_aperture;
	unsigned long	   page_mask;
} drm_agp_head_t;

typedef struct drm_sg_mem {
	unsigned long   handle;
	void            *virtual;
	int             pages;
	dma_addr_t	*busaddr;
} drm_sg_mem_t;

typedef struct drm_local_map {
	unsigned long	offset;	 /* Physical address (0 for SAREA)*/
	unsigned long	size;	 /* Physical size (bytes)	    */
	drm_map_type_t	type;	 /* Type of memory mapped		    */
	drm_map_flags_t flags;	 /* Flags				    */
	void		*handle; /* User-space: "Handle" to pass to mmap    */
				 /* Kernel-space: kernel-virtual address    */
	int		mtrr;	 /* Boolean: MTRR used */
				 /* Private data			    */
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
} drm_local_map_t;

typedef TAILQ_HEAD(drm_map_list, drm_map_list_entry) drm_map_list_t;
typedef struct drm_map_list_entry {
	TAILQ_ENTRY(drm_map_list_entry) link;
	drm_local_map_t	*map;
} drm_map_list_entry_t;

TAILQ_HEAD(drm_vbl_sig_list, drm_vbl_sig);
typedef struct drm_vbl_sig {
	TAILQ_ENTRY(drm_vbl_sig) link;
	unsigned int	sequence;
	int		signo;
	int		pid;
} drm_vbl_sig_t;

/** 
 * DRM device functions structure
 */
struct drm_device {
#ifdef __NetBSD__
	struct device	  device;	/* NetBSD's softc is an extension of struct device */
#endif

	/* Beginning of driver-config section */
	int	(*preinit)(struct drm_device *, unsigned long flags);
	int	(*postinit)(struct drm_device *, unsigned long flags);
	void	(*prerelease)(struct drm_device *, void *filp);
	void	(*pretakedown)(struct drm_device *);
	int	(*postcleanup)(struct drm_device *);
	int	(*presetup)(struct drm_device *);
	int	(*postsetup)(struct drm_device *);
	void	(*open_helper)(struct drm_device *, drm_file_t *);
	void	(*release)(struct drm_device *, void *filp);
	void	(*dma_ready)(struct drm_device *);
	int	(*dma_quiescent)(struct drm_device *);
	int	(*dma_flush_block_and_flush)(struct drm_device *, int context,
					     drm_lock_flags_t flags);
	int	(*dma_flush_unblock)(struct drm_device *, int context,
				     drm_lock_flags_t flags);
	int	(*context_ctor)(struct drm_device *dev, int context);
	int	(*context_dtor)(struct drm_device *dev, int context);
	int	(*kernel_context_switch)(struct drm_device *dev, int old,
					 int new);
	int	(*kernel_context_switch_unlock)(struct drm_device *dev);
	int	(*dma_schedule)(struct drm_device *dev, int locked);
	void	(*irq_preinstall)(drm_device_t *dev);
	void	(*irq_postinstall)(drm_device_t *dev);
	void	(*irq_uninstall)(drm_device_t *dev);
	void	(*irq_handler)(DRM_IRQ_ARGS);
	int	(*vblank_wait)(drm_device_t *dev, unsigned int *sequence);

	drm_ioctl_desc_t *driver_ioctls;
	int	max_driver_ioctl;

	int	dev_priv_size;

	int	driver_major;
	int	driver_minor;
	int	driver_patchlevel;
	const char *driver_name;	/* Simple driver name		   */
	const char *driver_desc;	/* Longer driver name		   */
	const char *driver_date;	/* Date of last major changes.	   */

	unsigned use_agp :1;
	unsigned require_agp :1;
	unsigned use_sg :1;
	unsigned use_dma :1;
	unsigned use_pci_dma :1;
	unsigned use_dma_queue :1;
	unsigned use_irq :1;
	unsigned use_vbl_irq :1;
	unsigned use_mtrr :1;
	unsigned use_ctxbitmap :1;
	/* End of driver-config section */

	char		  *unique;	/* Unique identifier: e.g., busid  */
	int		  unique_len;	/* Length of unique field	   */
#ifdef __FreeBSD__
	device_t	  device;	/* Device instance from newbus     */
#endif
	struct cdev	  *devnode;	/* Device number for mknod	   */
	int		  if_version;	/* Highest interface version set */

	int		  flags;	/* Flags to open(2)		   */

				/* Locks */
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
	struct mtx	  dma_lock;	/* protects dev->dma */
	struct mtx	  irq_lock; /* protects irq condition checks */
	struct mtx	  dev_lock;	/* protects everything else */
#endif
				/* Usage Counters */
	int		  open_count;	/* Outstanding files open	   */
	int		  buf_use;	/* Buffers in use -- cannot alloc  */

				/* Performance counters */
	unsigned long     counters;
	drm_stat_type_t   types[15];
	atomic_t          counts[15];

				/* Authentication */
	drm_file_list_t   files;
	drm_magic_head_t  magiclist[DRM_HASH_SIZE];

	/* Linked list of mappable regions. Protected by dev_lock */
	drm_map_list_t	  *maplist;

	drm_local_map_t	  **context_sareas;
	int		  max_context;

	drm_lock_data_t	  lock;		/* Information on hardware lock	   */

				/* DMA queues (contexts) */
	drm_device_dma_t  *dma;		/* Optional pointer for DMA support */

				/* Context support */
	int		  irq;		/* Interrupt used by board	   */
	int		  irq_enabled;	/* True if the irq handler is enabled */
#ifdef __FreeBSD__
	int		  irqrid;	/* Interrupt used by board */
	struct resource   *irqr;	/* Resource for interrupt used by board	   */
#elif defined(__NetBSD__)
	struct pci_attach_args  pa;
	pci_intr_handle_t	ih;
#endif
	void		  *irqh;	/* Handle from bus_setup_intr      */

	int		  pci_domain;
	int		  pci_bus;
	int		  pci_slot;
	int		  pci_func;

	atomic_t	  context_flag;	/* Context swapping flag	   */
	int		  last_context;	/* Last current context		   */
#if __FreeBSD_version >= 400005
	struct task       task;
#endif
   	int		  vbl_queue;	/* vbl wait channel */
   	atomic_t          vbl_received;

#ifdef __FreeBSD__
	struct sigio      *buf_sigio;	/* Processes waiting for SIGIO     */
#elif defined(__NetBSD__)
	pid_t		  buf_pgid;
#endif

				/* Sysctl support */
	struct drm_sysctl_info *sysctl;

	drm_agp_head_t    *agp;
	drm_sg_mem_t      *sg;  /* Scatter gather memory */
	atomic_t          *ctx_bitmap;
	void		  *dev_private;
	drm_local_map_t   *agp_buffer_map;
};

extern int	drm_flags;


/* Device setup support (drm_drv.c) */
#ifdef __FreeBSD__
int	drm_probe(device_t nbdev, drm_pci_id_list_t *idlist);
int	drm_attach(device_t nbdev, drm_pci_id_list_t *idlist);
int	drm_detach(device_t nbdev);
#elif defined(__NetBSD__)
int	drm_probe(struct pci_attach_args *pa, drm_pci_id_list_t *idlist);
int	drm_attach(struct pci_attach_args *pa, dev_t kdev, drm_pci_id_list_t *idlist);
#endif

/* Memory management support (drm_memory.c) */
void	drm_mem_init(void);
void	drm_mem_uninit(void);
void	*drm_alloc(size_t size, int area);
void	*drm_calloc(size_t nmemb, size_t size, int area);
void	*drm_realloc(void *oldpt, size_t oldsize, size_t size,
				   int area);
void	drm_free(void *pt, size_t size, int area);
void	*drm_ioremap(drm_device_t *dev, drm_local_map_t *map);
void	drm_ioremapfree(drm_local_map_t *map);
int	drm_mtrr_add(unsigned long offset, size_t size, int flags);
int	drm_mtrr_del(unsigned long offset, size_t size, int flags);

agp_memory *drm_alloc_agp(int pages, u32 type);
int	drm_free_agp(agp_memory *handle, int pages);
int	drm_bind_agp(agp_memory *handle, unsigned int start);
int	drm_unbind_agp(agp_memory *handle);

int	drm_context_switch(drm_device_t *dev, int old, int new);
int	drm_context_switch_complete(drm_device_t *dev, int new);

int	drm_ctxbitmap_init(drm_device_t *dev);
void	drm_ctxbitmap_cleanup(drm_device_t *dev);
void	drm_ctxbitmap_free(drm_device_t *dev, int ctx_handle);
int	drm_ctxbitmap_next(drm_device_t *dev);

/* Locking IOCTL support (drm_lock.c) */
int	drm_lock_take(__volatile__ unsigned int *lock,
				    unsigned int context);
int	drm_lock_transfer(drm_device_t *dev,
					__volatile__ unsigned int *lock,
					unsigned int context);
int	drm_lock_free(drm_device_t *dev,
				    __volatile__ unsigned int *lock,
				    unsigned int context);

/* Buffer management support (drm_bufs.c) */
int	drm_order(unsigned long size);

/* DMA support (drm_dma.c) */
int	drm_dma_setup(drm_device_t *dev);
void	drm_dma_takedown(drm_device_t *dev);
void	drm_free_buffer(drm_device_t *dev, drm_buf_t *buf);
void	drm_reclaim_buffers(drm_device_t *dev, DRMFILE filp);

/* IRQ support (drm_irq.c) */
int	drm_irq_install(drm_device_t *dev);
int	drm_irq_uninstall(drm_device_t *dev);
irqreturn_t drm_irq_handler(DRM_IRQ_ARGS);
void	drm_driver_irq_preinstall(drm_device_t *dev);
void	drm_driver_irq_postinstall(drm_device_t *dev);
void	drm_driver_irq_uninstall(drm_device_t *dev);
int	drm_vblank_wait(drm_device_t *dev, unsigned int *vbl_seq);
void	drm_vbl_send_signals(drm_device_t *dev);

/* AGP/GART support (drm_agpsupport.c) */
int	drm_device_is_agp(drm_device_t *dev);
drm_agp_head_t *drm_agp_init(void);
void	drm_agp_uninit(void);
void	drm_agp_do_release(void);
agp_memory *drm_agp_allocate_memory(size_t pages, u32 type);
int	drm_agp_free_memory(agp_memory *handle);
int	drm_agp_bind_memory(agp_memory *handle, off_t start);
int	drm_agp_unbind_memory(agp_memory *handle);

/* Scatter Gather Support (drm_scatter.c) */
void	drm_sg_cleanup(drm_sg_mem_t *entry);

/* ATI PCIGART support (ati_pcigart.c) */
int	drm_ati_pcigart_init(drm_device_t *dev, unsigned long *addr,
			     dma_addr_t *bus_addr);
int	drm_ati_pcigart_cleanup(drm_device_t *dev, unsigned long addr,
				dma_addr_t bus_addr);

/* Locking IOCTL support (drm_drv.c) */
int	drm_lock(DRM_IOCTL_ARGS);
int	drm_unlock(DRM_IOCTL_ARGS);
int	drm_version(DRM_IOCTL_ARGS);
int	drm_setversion(DRM_IOCTL_ARGS);

/* Misc. IOCTL support (drm_ioctl.c) */
int	drm_irq_by_busid(DRM_IOCTL_ARGS);
int	drm_getunique(DRM_IOCTL_ARGS);
int	drm_setunique(DRM_IOCTL_ARGS);
int	drm_getmap(DRM_IOCTL_ARGS);
int	drm_getclient(DRM_IOCTL_ARGS);
int	drm_getstats(DRM_IOCTL_ARGS);
int	drm_noop(DRM_IOCTL_ARGS);

/* Context IOCTL support (drm_context.c) */
int	drm_resctx(DRM_IOCTL_ARGS);
int	drm_addctx(DRM_IOCTL_ARGS);
int	drm_modctx(DRM_IOCTL_ARGS);
int	drm_getctx(DRM_IOCTL_ARGS);
int	drm_switchctx(DRM_IOCTL_ARGS);
int	drm_newctx(DRM_IOCTL_ARGS);
int	drm_rmctx(DRM_IOCTL_ARGS);
int	drm_setsareactx(DRM_IOCTL_ARGS);
int	drm_getsareactx(DRM_IOCTL_ARGS);

/* Drawable IOCTL support (drm_drawable.c) */
int	drm_adddraw(DRM_IOCTL_ARGS);
int	drm_rmdraw(DRM_IOCTL_ARGS);

/* Authentication IOCTL support (drm_auth.c) */
int	drm_getmagic(DRM_IOCTL_ARGS);
int	drm_authmagic(DRM_IOCTL_ARGS);

/* Buffer management support (drm_bufs.c) */
int	drm_addmap(DRM_IOCTL_ARGS);
int	drm_rmmap(DRM_IOCTL_ARGS);
int	drm_addbufs(DRM_IOCTL_ARGS);
int	drm_infobufs(DRM_IOCTL_ARGS);
int	drm_markbufs(DRM_IOCTL_ARGS);
int	drm_freebufs(DRM_IOCTL_ARGS);
int	drm_mapbufs(DRM_IOCTL_ARGS);

/* IRQ support (drm_irq.c) */
int	drm_control(DRM_IOCTL_ARGS);
int	drm_wait_vblank(DRM_IOCTL_ARGS);

/* AGP/GART support (drm_agpsupport.c) */
int	drm_agp_acquire(DRM_IOCTL_ARGS);
int	drm_agp_release(DRM_IOCTL_ARGS);
int	drm_agp_enable(DRM_IOCTL_ARGS);
int	drm_agp_info(DRM_IOCTL_ARGS);
int	drm_agp_alloc(DRM_IOCTL_ARGS);
int	drm_agp_free(DRM_IOCTL_ARGS);
int	drm_agp_unbind(DRM_IOCTL_ARGS);
int	drm_agp_bind(DRM_IOCTL_ARGS);

/* Scatter Gather Support (drm_scatter.c) */
int	drm_sg_alloc(DRM_IOCTL_ARGS);
int	drm_sg_free(DRM_IOCTL_ARGS);

/* consistent PCI memory functions (drm_pci.c) */
void	*drm_pci_alloc(drm_device_t *dev, size_t size, size_t align,
		       dma_addr_t maxaddr, dma_addr_t *busaddr);
void	drm_pci_free(drm_device_t *dev, size_t size, void *vaddr,
		     dma_addr_t busaddr);

/* Inline replacements for DRM_IOREMAP macros */
static __inline__ void drm_core_ioremap(struct drm_local_map *map, struct drm_device *dev)
{
	map->handle = drm_ioremap(dev, map);
}
#if 0
static __inline__ void drm_core_ioremap_nocache(struct drm_map *map, struct drm_device *dev)
{
	map->handle = drm_ioremap_nocache(dev, map);
}
#endif
static __inline__ void drm_core_ioremapfree(struct drm_local_map *map, struct drm_device *dev)
{
	if ( map->handle && map->size )
		drm_ioremapfree(map);
}

static __inline__ struct drm_local_map *drm_core_findmap(struct drm_device *dev, unsigned long offset)
{
	drm_map_list_entry_t *listentry;
	TAILQ_FOREACH(listentry, dev->maplist, link) {
		if ( listentry->map->offset == offset ) {
			return listentry->map;
		}
	}
	return NULL;
}

static __inline__ void drm_core_dropmap(struct drm_map *map)
{
}

#endif /* __KERNEL__ */
#endif /* _DRM_P_H_ */
