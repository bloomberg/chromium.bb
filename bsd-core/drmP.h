/* drmP.h -- Private header for Direct Rendering Manager -*- c -*-
 * Created: Mon Jan  4 10:05:05 1999 by faith@precisioninsight.com
 * Revised: Tue Oct 12 08:51:07 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * $PI: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/drmP.h,v 1.58 1999/08/30 13:05:00 faith Exp $
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/bsd/drm/kernel/drmP.h,v 1.3 2001/03/06 16:45:26 dawes Exp $
 * 
 */

#ifndef _DRM_P_H_
#define _DRM_P_H_

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/filio.h>
#include <sys/sysctl.h>
#include <sys/select.h>
#include <sys/bus.h>
#if __FreeBSD_version >= 400005
#include <sys/taskqueue.h>
#endif

#if __FreeBSD_version >= 400006
#define DRM_AGP
#endif

#ifdef DRM_AGP
#include <pci/agpvar.h>
#endif

#include "drm.h"

typedef u_int32_t atomic_t;
typedef u_int32_t cycles_t;
typedef u_int32_t spinlock_t;
#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		atomic_add_int(p, 1)
#define atomic_dec(p)		atomic_subtract_int(p, 1)
#define atomic_add(n, p)	atomic_add_int(p, n)
#define atomic_sub(n, p)	atomic_subtract_int(p, n)

/* The version number here is a guess */
#if __FreeBSD_version >= 500010
#define callout_init(a)		callout_init(a, 0)
#endif

/* Fake this */
static __inline u_int32_t
test_and_set_bit(int b, volatile u_int32_t *p)
{
	int s = splhigh();
	u_int32_t m = 1<<b;
	u_int32_t r = *p & m;
	*p |= m;
	splx(s);
	return r;
}

static __inline void
clear_bit(int b, volatile u_int32_t *p)
{
    atomic_clear_int(p + (b >> 5), 1 << (b & 0x1f));
}

static __inline void
set_bit(int b, volatile u_int32_t *p)
{
    atomic_set_int(p + (b >> 5), 1 << (b & 0x1f));
}

static __inline int
test_bit(int b, volatile u_int32_t *p)
{
    return p[b >> 5] & (1 << (b & 0x1f));
}

static __inline int
find_first_zero_bit(volatile u_int32_t *p, int max)
{
    int b;

    for (b = 0; b < max; b += 32) {
	if (p[b >> 5] != ~0) {
	    for (;;) {
		if ((p[b >> 5] & (1 << (b & 0x1f))) == 0)
		    return b;
		b++;
	    }
	}
    }
    return max;
}

#define spldrm()		spltty()

#define memset(p, v, s)		bzero(p, s)

/*
 * Fake out the module macros for versions of FreeBSD where they don't
 * exist.
 */
#if __FreeBSD_version < 400002

#define MODULE_VERSION(a,b)		struct __hack
#define MODULE_DEPEND(a,b,c,d,e)	struct __hack

#endif

#define DRM_DEBUG_CODE 0	  /* Include debugging code (if > 1, then
				     also include looping detection. */
#define DRM_DMA_HISTOGRAM 1	  /* Make histogram of DMA latency. */

#define DRM_HASH_SIZE	      16 /* Size of key hash table		  */
#define DRM_KERNEL_CONTEXT    0	 /* Change drm_resctx if changed	  */
#define DRM_RESERVED_CONTEXTS 1	 /* Change drm_resctx if changed	  */
#define DRM_LOOPING_LIMIT     5000000
#define DRM_BSZ		      1024 /* Buffer size for /dev/drm? output	  */
#define DRM_TIME_SLICE	      (hz/20) /* Time slice for GLXContexts       */
#define DRM_LOCK_SLICE	      1	/* Time slice for lock, in jiffies	  */

#define DRM_FLAG_DEBUG	  0x01
#define DRM_FLAG_NOCTX	  0x02

#define DRM_MEM_DMA	  0
#define DRM_MEM_SAREA	  1
#define DRM_MEM_DRIVER	  2
#define DRM_MEM_MAGIC	  3
#define DRM_MEM_IOCTLS	  4
#define DRM_MEM_MAPS	  5
#define DRM_MEM_VMAS	  6
#define DRM_MEM_BUFS	  7
#define DRM_MEM_SEGS	  8
#define DRM_MEM_PAGES	  9
#define DRM_MEM_FILES	 10
#define DRM_MEM_QUEUES	 11
#define DRM_MEM_CMDS	 12
#define DRM_MEM_MAPPINGS 13
#define DRM_MEM_BUFLISTS 14
#define DRM_MEM_AGPLISTS  15
#define DRM_MEM_TOTALAGP  16
#define DRM_MEM_BOUNDAGP  17
#define DRM_MEM_CTXBITMAP 18

#define DRM_MAX_CTXBITMAP (PAGE_SIZE * 8)

				/* Backward compatibility section */
#ifndef _PAGE_PWT
				/* The name of _PAGE_WT was changed to
				   _PAGE_PWT in Linux 2.2.6 */
#define _PAGE_PWT _PAGE_WT
#endif

#define __drm_dummy_lock(lock) (*(__volatile__ unsigned int *)lock)
#define _DRM_CAS(lock,old,new,__ret)				       \
	do {							       \
		int __dummy;	/* Can't mark eax as clobbered */      \
		__asm__ __volatile__(				       \
			"lock ; cmpxchg %4,%1\n\t"		       \
			"setnz %0"				       \
			: "=d" (__ret),				       \
			  "=m" (__drm_dummy_lock(lock)),	       \
			  "=a" (__dummy)			       \
			: "2" (old),				       \
			  "r" (new));				       \
	} while (0)



				/* Macros to make printk easier */
#define DRM_ERROR(fmt, arg...) \
	printf("error: " "[" DRM_NAME ":" __FUNCTION__ "] *ERROR* " fmt , ##arg)
#define DRM_MEM_ERROR(area, fmt, arg...) \
	printf("error: " "[" DRM_NAME ":" __FUNCTION__ ":%s] *ERROR* " fmt , \
	       drm_mem_stats[area].name , ##arg)
#define DRM_INFO(fmt, arg...)  printf("info: " "[" DRM_NAME "] " fmt , ##arg)

#if DRM_DEBUG_CODE
#define DRM_DEBUG(fmt, arg...)						  \
	do {								  \
		if (drm_flags&DRM_FLAG_DEBUG)				  \
			printf("[" DRM_NAME ":" __FUNCTION__ "] " fmt ,	  \
			       ##arg);					  \
	} while (0)
#else
#define DRM_DEBUG(fmt, arg...)		 do { } while (0)
#endif

#define DRM_PROC_LIMIT (PAGE_SIZE-80)

#define DRM_SYSCTL_PRINT(fmt, arg...)		\
  snprintf(buf, sizeof(buf), fmt, ##arg);	\
  error = SYSCTL_OUT(req, buf, strlen(buf));	\
  if (error) return error;

#define DRM_SYSCTL_PRINT_RET(ret, fmt, arg...)	\
  snprintf(buf, sizeof(buf), fmt, ##arg);	\
  error = SYSCTL_OUT(req, buf, strlen(buf));	\
  if (error) { ret; return error; }

				/* Internal types and structures */
#define DRM_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define DRM_MIN(a,b) ((a)<(b)?(a):(b))
#define DRM_MAX(a,b) ((a)>(b)?(a):(b))

#define DRM_LEFTCOUNT(x) (((x)->rp + (x)->count - (x)->wp) % ((x)->count + 1))
#define DRM_BUFCOUNT(x) ((x)->count - DRM_LEFTCOUNT(x))
#define DRM_WAITCOUNT(dev,idx) DRM_BUFCOUNT(&dev->queuelist[idx]->waitlist)

typedef struct drm_ioctl_desc {
	d_ioctl_t	     *func;
	int		     auth_needed;
	int		     root_only;
} drm_ioctl_desc_t;

typedef struct drm_devstate {
	pid_t		  owner;	/* X server pid holding x_lock */
	
} drm_devstate_t;

typedef struct drm_magic_entry {
	drm_magic_t	       magic;
	struct drm_file	       *priv;
	struct drm_magic_entry *next;
} drm_magic_entry_t;

typedef struct drm_magic_head {
       struct drm_magic_entry *head;
       struct drm_magic_entry *tail;
} drm_magic_head_t;

typedef struct drm_vma_entry {
	struct vm_area_struct *vma;
	struct drm_vma_entry  *next;
	pid_t		      pid;
} drm_vma_entry_t;

typedef struct drm_buf {
	int		  idx;	       /* Index into master buflist	     */
	int		  total;       /* Buffer size			     */
	int		  order;       /* log-base-2(total)		     */
	int		  used;	       /* Amount of buffer in use (for DMA)  */
	unsigned long	  offset;      /* Byte offset (used internally)	     */
	void		  *address;    /* Address of buffer		     */
	unsigned long	  bus_address; /* Bus address of buffer		     */
	struct drm_buf	  *next;       /* Kernel-only: used for free list    */
	__volatile__ int  waiting;     /* On kernel DMA queue		     */
	__volatile__ int  pending;     /* On hardware DMA queue		     */
	int		  dma_wait;    /* Processes waiting		     */
	pid_t		  pid;	       /* PID of holding process	     */
	int		  context;     /* Kernel queue for this buffer	     */
	int		  while_locked;/* Dispatch this buffer while locked  */
	enum {
		DRM_LIST_NONE	 = 0,
		DRM_LIST_FREE	 = 1,
		DRM_LIST_WAIT	 = 2,
		DRM_LIST_PEND	 = 3,
		DRM_LIST_PRIO	 = 4,
		DRM_LIST_RECLAIM = 5
	}		  list;	       /* Which list we're on		     */

	void *dev_private;
	int dev_priv_size;

#if DRM_DMA_HISTOGRAM
	struct timespec	  time_queued;	   /* Queued to kernel DMA queue     */
	struct timespec	  time_dispatched; /* Dispatched to hardware	     */
	struct timespec	  time_completed;  /* Completed by hardware	     */
	struct timespec	  time_freed;	   /* Back on freelist		     */
#endif
} drm_buf_t;

#if DRM_DMA_HISTOGRAM
#define DRM_DMA_HISTOGRAM_SLOTS		  9
#define DRM_DMA_HISTOGRAM_INITIAL	 10
#define DRM_DMA_HISTOGRAM_NEXT(current)	 ((current)*10)
typedef struct drm_histogram {
	atomic_t	  total;
	
	atomic_t	  queued_to_dispatched[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  dispatched_to_completed[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  completed_to_freed[DRM_DMA_HISTOGRAM_SLOTS];
	
	atomic_t	  queued_to_completed[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  queued_to_freed[DRM_DMA_HISTOGRAM_SLOTS];
	
	atomic_t	  dma[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  schedule[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  ctx[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  lacq[DRM_DMA_HISTOGRAM_SLOTS];
	atomic_t	  lhld[DRM_DMA_HISTOGRAM_SLOTS];
} drm_histogram_t;
#endif

				/* bufs is one longer than it has to be */
typedef struct drm_waitlist {
	int		  count;	/* Number of possible buffers	   */
	drm_buf_t	  **bufs;	/* List of pointers to buffers	   */
	drm_buf_t	  **rp;		/* Read pointer			   */
	drm_buf_t	  **wp;		/* Write pointer		   */
	drm_buf_t	  **end;	/* End pointer			   */
	spinlock_t	  read_lock;
	spinlock_t	  write_lock;
} drm_waitlist_t;

typedef struct drm_freelist {
	int		  initialized; /* Freelist in use		   */
	atomic_t	  count;       /* Number of free buffers	   */
	drm_buf_t	  *next;       /* End pointer			   */
	
	int		  waiting;     /* Processes waiting on free bufs   */
	int		  low_mark;    /* Low water mark		   */
	int		  high_mark;   /* High water mark		   */
	atomic_t	  wfh;	       /* If waiting for high mark	   */
	struct simplelock	  lock;	       /* hope this doesn't need to be linux compatible */
} drm_freelist_t;

typedef struct drm_buf_entry {
	int		  buf_size;
	int		  buf_count;
	drm_buf_t	  *buflist;
	int		  seg_count;
	int		  page_order;
	unsigned long	  *seglist;

	drm_freelist_t	  freelist;
} drm_buf_entry_t;

typedef struct drm_hw_lock {
	__volatile__ unsigned int lock;
	char			  padding[60]; /* Pad to cache line */
} drm_hw_lock_t;

typedef TAILQ_HEAD(drm_file_list, drm_file) drm_file_list_t;
typedef struct drm_file {
	TAILQ_ENTRY(drm_file) link;
	int		  authenticated;
	int		  minor;
	pid_t		  pid;
	uid_t		  uid;
	int		  refs;
	drm_magic_t	  magic;
	unsigned long	  ioctl_count;
	struct drm_device *devXX;
} drm_file_t;


typedef struct drm_queue {
	atomic_t	  use_count;	/* Outstanding uses (+1)	    */
	atomic_t	  finalization;	/* Finalization in progress	    */
	atomic_t	  block_count;	/* Count of processes waiting	    */
	atomic_t	  block_read;	/* Queue blocked for reads	    */
	int		  read_queue;	/* Processes waiting on block_read  */
	atomic_t	  block_write;	/* Queue blocked for writes	    */
	int		  write_queue;	/* Processes waiting on block_write */
	atomic_t	  total_queued;	/* Total queued statistic	    */
	atomic_t	  total_flushed;/* Total flushes statistic	    */
	atomic_t	  total_locks;	/* Total locks statistics	    */
	drm_ctx_flags_t	  flags;	/* Context preserving and 2D-only   */
	drm_waitlist_t	  waitlist;	/* Pending buffers		    */
	int		  flush_queue;	/* Processes waiting until flush    */
} drm_queue_t;

typedef struct drm_lock_data {
	drm_hw_lock_t	  *hw_lock;	/* Hardware lock		   */
	pid_t		  pid;		/* PID of lock holder (0=kernel)   */
	int		  lock_queue;	/* Queue of blocked processes	   */
	unsigned long	  lock_time;	/* Time of last lock in jiffies	   */
} drm_lock_data_t;

typedef struct drm_device_dma {
				/* Performance Counters */
	atomic_t	  total_prio;	/* Total DRM_DMA_PRIORITY	   */
	atomic_t	  total_bytes;	/* Total bytes DMA'd		   */
	atomic_t	  total_dmas;	/* Total DMA buffers dispatched	   */
	
	atomic_t	  total_missed_dma;  /* Missed drm_do_dma	    */
	atomic_t	  total_missed_lock; /* Missed lock in drm_do_dma   */
	atomic_t	  total_missed_free; /* Missed drm_free_this_buffer */
	atomic_t	  total_missed_sched;/* Missed drm_dma_schedule	    */

	atomic_t	  total_tried;	/* Tried next_buffer		    */
	atomic_t	  total_hit;	/* Sent next_buffer		    */
	atomic_t	  total_lost;	/* Lost interrupt		    */

	drm_buf_entry_t	  bufs[DRM_MAX_ORDER+1];
	int		  buf_count;
	drm_buf_t	  **buflist;	/* Vector of pointers info bufs	   */
	int		  seg_count; 
	int		  page_count;
	vm_offset_t	  *pagelist;
	unsigned long	  byte_count;
	enum {
	   _DRM_DMA_USE_AGP = 0x01
	} flags;

				/* DMA support */
	drm_buf_t	  *this_buffer;	/* Buffer being sent		   */
	drm_buf_t	  *next_buffer; /* Selected buffer to send	   */
	drm_queue_t	  *next_queue;	/* Queue from which buffer selected*/
	int		   waiting;	/* Processes waiting on free bufs  */
} drm_device_dma_t;

#ifdef DRM_AGP

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
   	int 		   agp_mtrr;
} drm_agp_head_t;

#endif

typedef struct drm_device {
	const char	  *name;	/* Simple driver name		   */
	char		  *unique;	/* Unique identifier: e.g., busid  */
	int		  unique_len;	/* Length of unique field	   */
	device_t	  device;	/* Device instance from newbus     */
	dev_t		  devnode;	/* Device number for mknod	   */
	char		  *devname;	/* For /proc/interrupts		   */
	
	int		  blocked;	/* Blocked due to VC switch?	   */
	int		  flags;	/* Flags to open(2)		   */
	int		  writable;	/* Opened with FWRITE		   */
	struct proc_dir_entry *root;	/* Root for this device's entries  */

				/* Locks */
	struct simplelock count_lock;	/* For inuse, open_count, buf_use  */
	struct lock	  dev_lock;	/* For others			   */

				/* Usage Counters */
	int		  open_count;	/* Outstanding files open	   */
	atomic_t	  ioctl_count;	/* Outstanding IOCTLs pending	   */
	atomic_t	  vma_count;	/* Outstanding vma areas open	   */
	int		  buf_use;	/* Buffers in use -- cannot alloc  */
	atomic_t	  buf_alloc;	/* Buffer allocation in progress   */

				/* Performance Counters */
	atomic_t	  total_open;
	atomic_t	  total_close;
	atomic_t	  total_ioctl;
	atomic_t	  total_irq;	/* Total interruptions		   */
	atomic_t	  total_ctx;	/* Total context switches	   */
	
	atomic_t	  total_locks;
	atomic_t	  total_unlocks;
	atomic_t	  total_contends;
	atomic_t	  total_sleeps;

				/* Authentication */
	drm_file_list_t   files;
	drm_magic_head_t  magiclist[DRM_HASH_SIZE];

				/* Memory management */
	drm_map_t	  **maplist;	/* Vector of pointers to regions   */
	int		  map_count;	/* Number of mappable regions	   */

	drm_vma_entry_t	  *vmalist;	/* List of vmas (for debugging)	   */
	drm_lock_data_t	  lock;		/* Information on hardware lock	   */

				/* DMA queues (contexts) */
	int		  queue_count;	/* Number of active DMA queues	   */
	int		  queue_reserved; /* Number of reserved DMA queues */
	int		  queue_slots;	/* Actual length of queuelist	   */
	drm_queue_t	  **queuelist;	/* Vector of pointers to DMA queues */
	drm_device_dma_t  *dma;		/* Optional pointer for DMA support */

				/* Context support */
	struct resource   *irq;		/* Interrupt used by board	   */
	void		  *irqh;	/* Handle from bus_setup_intr      */
	__volatile__ long  context_flag; /* Context swapping flag	   */
	__volatile__ long  interrupt_flag;/* Interruption handler flag	   */
	__volatile__ long  dma_flag;	 /* DMA dispatch flag		   */
	struct callout	  timer;	/* Timer for delaying ctx switch   */
	int		  context_wait; /* Processes waiting on ctx switch */
	int		  last_checked;	/* Last context checked for DMA	   */
	int		  last_context;	/* Last current context		   */
	int		  last_switch;	/* Time at last context switch  */
#if __FreeBSD_version >= 400005
	struct task	  task;
#endif
	struct timespec	  ctx_start;
	struct timespec	  lck_start;
#if DRM_DMA_HISTOGRAM
	drm_histogram_t	  histo;
#endif
	
				/* Callback to X server for context switch
				   and for heavy-handed reset. */
	char		  buf[DRM_BSZ]; /* Output buffer		   */
	char		  *buf_rp;	/* Read pointer			   */
	char		  *buf_wp;	/* Write pointer		   */
	char		  *buf_end;	/* End pointer			   */
	struct sigio	  *buf_sigio;   /* Processes waiting for SIGIO	   */
	struct selinfo    buf_sel;	/* Workspace for select/poll       */
	int		  buf_readers;	/* Processes waiting to read	   */
	int		  buf_writers;	/* Processes waiting to ctx switch */
	int		  buf_selecting; /* True if poll sleeper           */

				/* Sysctl support */
	struct drm_sysctl_info *sysctl;

#ifdef DRM_AGP
	drm_agp_head_t    *agp;
#endif
	u_int32_t	  *ctx_bitmap;
	void		  *dev_private;
} drm_device_t;


				/* Internal function definitions */

				/* Misc. support (init.c) */
extern int	     drm_flags;
extern void	     drm_parse_options(char *s);


				/* Device support (fops.c) */
extern drm_file_t *drm_find_file_by_proc(drm_device_t *dev, struct proc *p);
extern int drm_open_helper(dev_t kdev, int flags, int fmt, struct proc *p,
			   drm_device_t *dev);
extern d_close_t     drm_close;
extern d_read_t	     drm_read;
extern d_write_t     drm_write;
extern d_poll_t	     drm_poll;
extern int	     drm_fsetown(dev_t kdev, u_long cmd, caddr_t data,
				 int flags, struct proc *p);
extern int	     drm_fgetown(dev_t kdev, u_long cmd, caddr_t data,
				 int flags, struct proc *p);
extern int	     drm_write_string(drm_device_t *dev, const char *s);

#if 0
				/* Mapping support (vm.c) */
extern unsigned long drm_vm_nopage(struct vm_area_struct *vma,
				   unsigned long address,
				   int write_access);
extern unsigned long drm_vm_shm_nopage(struct vm_area_struct *vma,
				       unsigned long address,
				       int write_access);
extern unsigned long drm_vm_dma_nopage(struct vm_area_struct *vma,
				       unsigned long address,
				       int write_access);
extern void	     drm_vm_open(struct vm_area_struct *vma);
extern void	     drm_vm_close(struct vm_area_struct *vma);
extern int	     drm_mmap_dma(struct file *filp,
				  struct vm_area_struct *vma);
#endif
extern d_mmap_t	     drm_mmap;

				/* Proc support (proc.c) */
extern int	     drm_sysctl_init(drm_device_t *dev);
extern int	     drm_sysctl_cleanup(drm_device_t *dev);

				/* Memory management support (memory.c) */
extern void	     drm_mem_init(void);

#if __FreeBSD_version < 411000
#define DRM_SYSCTL_HANDLER_ARGS SYSCTL_HANDLER_ARGS
#else
#define DRM_SYSCTL_HANDLER_ARGS (SYSCTL_HANDLER_ARGS)
#endif
extern int	     drm_mem_info DRM_SYSCTL_HANDLER_ARGS;
extern void	     *drm_alloc(size_t size, int area);
extern void	     *drm_realloc(void *oldpt, size_t oldsize, size_t size,
				  int area);
extern char	     *drm_strdup(const char *s, int area);
extern void	     drm_strfree(char *s, int area);
extern void	     drm_free(void *pt, size_t size, int area);
extern unsigned long drm_alloc_pages(int order, int area);
extern void	     drm_free_pages(unsigned long address, int order,
				    int area);
extern void	     *drm_ioremap(unsigned long offset, unsigned long size);
extern void	     drm_ioremapfree(void *pt, unsigned long size);

#ifdef DRM_AGP
extern void	    *drm_alloc_agp(int pages, u_int32_t type);
extern int           drm_free_agp(void *handle, int pages);
extern int           drm_bind_agp(void *handle, unsigned int start);
extern int           drm_unbind_agp(void *handle);
#endif

				/* Buffer management support (bufs.c) */
extern int	     drm_order(unsigned long size);
extern d_ioctl_t     drm_addmap;
extern d_ioctl_t     drm_addbufs;
extern d_ioctl_t     drm_infobufs;
extern d_ioctl_t     drm_markbufs;
extern d_ioctl_t     drm_freebufs;
extern d_ioctl_t     drm_mapbufs;


				/* Buffer list management support (lists.c) */
extern int	     drm_waitlist_create(drm_waitlist_t *bl, int count);
extern int	     drm_waitlist_destroy(drm_waitlist_t *bl);
extern int	     drm_waitlist_put(drm_waitlist_t *bl, drm_buf_t *buf);
extern drm_buf_t     *drm_waitlist_get(drm_waitlist_t *bl);

extern int	     drm_freelist_create(drm_freelist_t *bl, int count);
extern int	     drm_freelist_destroy(drm_freelist_t *bl);
extern int	     drm_freelist_put(drm_device_t *dev, drm_freelist_t *bl,
				      drm_buf_t *buf);
extern drm_buf_t     *drm_freelist_get(drm_freelist_t *bl, int block);

				/* DMA support (gen_dma.c) */
extern void	     drm_dma_setup(drm_device_t *dev);
extern void	     drm_dma_takedown(drm_device_t *dev);
extern void	     drm_free_buffer(drm_device_t *dev, drm_buf_t *buf);
extern void	     drm_reclaim_buffers(drm_device_t *dev, pid_t pid);
extern int	     drm_context_switch(drm_device_t *dev, int old, int new);
extern int	     drm_context_switch_complete(drm_device_t *dev, int new);
extern void	     drm_wakeup(drm_device_t *dev, drm_buf_t *buf);
extern void	     drm_clear_next_buffer(drm_device_t *dev);
extern int	     drm_select_queue(drm_device_t *dev,
				      void (*wrapper)(void *));
extern int	     drm_dma_enqueue(drm_device_t *dev, drm_dma_t *dma);
extern int	     drm_dma_get_buffers(drm_device_t *dev, drm_dma_t *dma);
#if DRM_DMA_HISTOGRAM
extern int	     drm_histogram_slot(struct timespec *ts);
extern void	     drm_histogram_compute(drm_device_t *dev, drm_buf_t *buf);
#endif


				/* Misc. IOCTL support (ioctl.c) */
extern d_ioctl_t     drm_irq_busid;
extern d_ioctl_t     drm_getunique;
extern d_ioctl_t     drm_setunique;


				/* Context IOCTL support (context.c) */
extern d_ioctl_t     drm_resctx;
extern d_ioctl_t     drm_addctx;
extern d_ioctl_t     drm_modctx;
extern d_ioctl_t     drm_getctx;
extern d_ioctl_t     drm_switchctx;
extern d_ioctl_t     drm_newctx;
extern d_ioctl_t     drm_rmctx;


				/* Drawable IOCTL support (drawable.c) */
extern d_ioctl_t     drm_adddraw;
extern d_ioctl_t     drm_rmdraw;


				/* Authentication IOCTL support (auth.c) */
extern int	     drm_add_magic(drm_device_t *dev, drm_file_t *priv,
				   drm_magic_t magic);
extern int	     drm_remove_magic(drm_device_t *dev, drm_magic_t magic);
extern d_ioctl_t     drm_getmagic;
extern d_ioctl_t     drm_authmagic;


				/* Locking IOCTL support (lock.c) */
extern d_ioctl_t     drm_block;
extern d_ioctl_t     drm_unblock;
extern int	     drm_lock_take(__volatile__ unsigned int *lock,
				   unsigned int context);
extern int	     drm_lock_transfer(drm_device_t *dev,
				       __volatile__ unsigned int *lock,
				       unsigned int context);
extern int	     drm_lock_free(drm_device_t *dev,
				   __volatile__ unsigned int *lock,
				   unsigned int context);
extern d_ioctl_t     drm_finish;
extern int	     drm_flush_unblock(drm_device_t *dev, int context,
				       drm_lock_flags_t flags);
extern int	     drm_flush_block_and_flush(drm_device_t *dev, int context,
					       drm_lock_flags_t flags);

				/* Context Bitmap support (ctxbitmap.c) */
extern int	     drm_ctxbitmap_init(drm_device_t *dev);
extern void	     drm_ctxbitmap_cleanup(drm_device_t *dev);
extern int	     drm_ctxbitmap_next(drm_device_t *dev);
extern void	     drm_ctxbitmap_free(drm_device_t *dev, int ctx_handle);

#ifdef DRM_AGP
				/* AGP/GART support (agpsupport.c) */
extern drm_agp_head_t *drm_agp_init(void);
extern d_ioctl_t     drm_agp_acquire;
extern d_ioctl_t     drm_agp_release;
extern d_ioctl_t     drm_agp_enable;
extern d_ioctl_t     drm_agp_info;
extern d_ioctl_t     drm_agp_alloc;
extern d_ioctl_t     drm_agp_free;
extern d_ioctl_t     drm_agp_unbind;
extern d_ioctl_t     drm_agp_bind;
#endif
#endif
#endif
