/**
 * \file drm_os_netbsd.h
 * OS-specific #defines for NetBSD
 * 
 * \author Eric Anholt <anholt@FreeBSD.org>
 * \author Erik Reid <reide@canuck.com>
 */

/*
 * Copyright 2003 Eric Anholt
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
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
#include <sys/device.h>
#include <sys/mman.h>
#include <uvm/uvm.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/lkm.h>
/* For TIOCSPGRP/TIOCGPGRP */
#include <sys/ttycom.h>
#include <sys/endian.h>

#include <uvm/uvm.h>

#include <machine/pmap.h>
#include <machine/bus.h>
#include <sys/resourcevar.h>
#include <machine/sysarch.h>
#include <machine/mtrr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
 
#define __REALLY_HAVE_AGP	__HAVE_AGP

#define __REALLY_HAVE_MTRR	1
#define __REALLY_HAVE_SG	0

#if __REALLY_HAVE_AGP
#include <dev/pci/agpvar.h>
#include <sys/agpio.h>
#endif

#include <opt_drm.h>

#if DRM_DEBUG
#undef  DRM_DEBUG_CODE
#define DRM_DEBUG_CODE 2
#endif
#undef DRM_DEBUG

#if DRM_LINUX
#undef DRM_LINUX	/* FIXME: Linux compat has not been ported yet */
#endif

typedef drm_device_t *device_t;

extern struct cfdriver DRM(cd);

#define DRM_TIME_SLICE	      (hz/20)  /* Time slice for GLXContexts	  */

#define DRM_DEV_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)
#define DRM_DEV_UID	0
#define DRM_DEV_GID	0
#define CDEV_MAJOR	34

#define DRM_CURPROC		curproc
#define DRM_STRUCTPROC		struct proc
#define DRM_SPINTYPE		struct simplelock
#define DRM_SPININIT(l,name)
#define DRM_SPINUNINIT(l)
#define DRM_SPINLOCK(l)	
#define DRM_SPINUNLOCK(u)
#define DRM_CURRENTPID		curproc->p_pid

/* Currently our DRMFILE (filp) is a void * which is actually the pid
 * of the current process.  It should be a per-open unique pointer, but
 * code for that is not yet written */
#define DRMFILE			void *
#define DRM_IOCTL_ARGS		dev_t kdev, u_long cmd, caddr_t data, int flags, DRM_STRUCTPROC *p, DRMFILE filp
#define DRM_LOCK()
#define DRM_UNLOCK()
#define DRM_SUSER(p)		suser(p->p_ucred, &p->p_acflag)
#define DRM_TASKQUEUE_ARGS	void *dev, int pending
#define DRM_IRQ_ARGS		void *arg
typedef int			irqreturn_t;
#define IRQ_NONE		/* FIXME */
#define IRQ_HANDLED		/* FIXME */
#define DRM_DEVICE		drm_device_t *dev = device_lookup(&DRM(cd), minor(kdev))
/* XXX Not sure if this is the 'right' version.. */
#if __NetBSD_Version__ >= 106140000
MALLOC_DECLARE(DRM(M_DRM));
#else
/* XXX Make sure this works */
extern const int DRM(M_DRM) = M_DEVBUF;
#endif /* __NetBSD_Version__ */
#define DRM_MALLOC(size)	malloc( size, DRM(M_DRM), M_NOWAIT )
#define DRM_FREE(pt,size)		free( pt, DRM(M_DRM) )

#define DRM_READ8(map, offset)		bus_space_read_1(  (map)->iot, (map)->ioh, (offset) )
#define DRM_READ32(map, offset)		bus_space_read_4(  (map)->iot, (map)->ioh, (offset) )
#define DRM_WRITE8(map, offset, val)	bus_space_write_1( (map)->iot, (map)->ioh, (offset), (val) )
#define DRM_WRITE32(map, offset, val)	bus_space_write_4( (map)->iot, (map)->ioh, (offset), (val) )

#define DRM_MTRR_WC	MTRR_TYPE_WC

#define DRM_AGP_FIND_DEVICE()	agp_find_device(0)

#define DRM_GET_PRIV_WITH_RETURN(_priv, _filp)			\
do {								\
	if (_filp != (DRMFILE)DRM_CURRENTPID) {			\
		DRM_ERROR("filp doesn't match curproc\n");	\
		return EINVAL;					\
	}							\
	_priv = DRM(find_file_by_proc)(dev, DRM_CURPROC);	\
	if (_priv == NULL) {					\
		DRM_ERROR("can't find authenticator\n");	\
		return EINVAL;					\
	}							\
} while (0)

#define LOCK_TEST_WITH_RETURN(dev, filp)				\
do {									\
	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock) ||		\
	     dev->lock.filp != filp) {					\
		DRM_ERROR("%s called without lock held\n",		\
			   __FUNCTION__);				\
		return EINVAL;						\
	}								\
} while (0)

#define DRM_UDELAY( udelay )					\
do {								\
	struct timeval tv1, tv2;				\
	microtime(&tv1);					\
	do {							\
		microtime(&tv2);				\
	}							\
	while (((tv2.tv_sec-tv1.tv_sec)*1000000 + tv2.tv_usec - tv1.tv_usec) < udelay ); \
} while (0)

#define DRM_GETSAREA()					\
do {								\
	drm_map_list_entry_t *listentry;			\
	TAILQ_FOREACH(listentry, dev->maplist, link) {		\
		drm_local_map_t *map = listentry->map;		\
		if (map->type == _DRM_SHM &&			\
			map->flags & _DRM_CONTAINS_LOCK) {	\
			dev_priv->sarea = map;			\
			break;					\
		}						\
	}							\
} while (0)

#define DRM_HZ hz

#define DRM_WAIT_ON( ret, queue, timeout, condition )		\
while (!condition) {						\
	int s = spldrm();					\
	ret = tsleep( (void *)&(queue), PZERO | PCATCH, "drmwtq", (timeout) ); \
	if ( ret )						\
		return ret;					\
	splx(s);					\
}

#define DRM_ERR(v)		v

#define DRM_COPY_TO_USER_IOCTL(arg1, arg2, arg3) \
	*arg1 = arg2
#define DRM_COPY_FROM_USER_IOCTL(arg1, arg2, arg3) \
	arg1 = *arg2
#define DRM_COPY_TO_USER(arg1, arg2, arg3) \
	copyout(arg2, arg1, arg3)
#define DRM_COPY_FROM_USER(arg1, arg2, arg3) \
	copyin(arg2, arg1, arg3)
/* Macros for userspace access with checking readability once */
/* FIXME: can't find equivalent functionality for nocheck yet.
 * It'll be slower than linux, but should be correct.
 */
#define DRM_VERIFYAREA_READ( uaddr, size )		\
	(!uvm_useracc((caddr_t)uaddr, size, VM_PROT_READ))
#define DRM_COPY_FROM_USER_UNCHECKED(arg1, arg2, arg3) 	\
	copyin(arg2, arg1, arg3)
#define DRM_COPY_TO_USER_UNCHECKED(arg1, arg2, arg3)	\
	copyout(arg2, arg1, arg3)
#define DRM_GET_USER_UNCHECKED(val, uaddr)			\
	((val) = fuword(uaddr), 0)

/* DRM_READMEMORYBARRIER() prevents reordering of reads.
 * DRM_WRITEMEMORYBARRIER() prevents reordering of writes.
 * DRM_MEMORYBARRIER() prevents reordering of reads and writes.
 */
#if defined(__i386__)
#define DRM_READMEMORYBARRIER()		__asm __volatile( \
					"lock; addl $0,0(%%esp)" : : : "memory");
#define DRM_WRITEMEMORYBARRIER()	__asm __volatile("" : : : "memory");
#define DRM_MEMORYBARRIER()		__asm __volatile( \
					"lock; addl $0,0(%%esp)" : : : "memory");
#elif defined(__alpha__)
#define DRM_READMEMORYBARRIER()		alpha_mb();
#define DRM_WRITEMEMORYBARRIER()	alpha_wmb();
#define DRM_MEMORYBARRIER()		alpha_mb();
#endif

#define DRM_WAKEUP(w) wakeup((void *)w)
#define DRM_WAKEUP_INT(w) wakeup(w)
#define DRM_INIT_WAITQUEUE( queue )  do {} while (0)

#define PAGE_ALIGN(addr) (((addr)+PAGE_SIZE-1)&PAGE_MASK)

#define cpu_to_le32(x) htole32(x)
#define le32_to_cpu(x) le32toh(x)

typedef u_int32_t dma_addr_t;
typedef volatile long atomic_t;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
typedef dev_type_ioctl(d_ioctl_t);
typedef vaddr_t vm_offset_t;

/* FIXME */
#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		(*(p) += 1)
#define atomic_dec(p)		(*(p) -= 1)
#define atomic_add(n, p)	(*(p) += (n))
#define atomic_sub(n, p)	(*(p) -= (n))

/* FIXME */
#define atomic_add_int(p, v)      *(p) += v
#define atomic_subtract_int(p, v) *(p) -= v
#define atomic_set_int(p, bits)   *(p) |= (bits)
#define atomic_clear_int(p, bits) *(p) &= ~(bits)

/* Fake this */

static __inline int
atomic_cmpset_int(__volatile__ int *dst, int old, int new)
{
	int s = splhigh();
	if (*dst==old) {
		*dst = new;
		splx(s);
		return 1;
	}
	splx(s);
	return 0;
}

static __inline atomic_t
test_and_set_bit(int b, atomic_t *p)
{
	int s = splhigh();
	unsigned int m = 1<<b;
	unsigned int r = *p & m;
	*p |= m;
	splx(s);
	return r;
}

static __inline void
clear_bit(int b, atomic_t *p)
{
    atomic_clear_int(p + (b >> 5), 1 << (b & 0x1f));
}

static __inline void
set_bit(int b, atomic_t *p)
{
    atomic_set_int(p + (b >> 5), 1 << (b & 0x1f));
}

static __inline int
test_bit(int b, atomic_t *p)
{
    return p[b >> 5] & (1 << (b & 0x1f));
}

static __inline int
find_first_zero_bit(atomic_t *p, int max)
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
#define jiffies			hardclock_ticks

/* Redefinitions to make templating easy */
#define wait_queue_head_t	int
#define agp_memory		void

				/* Macros to make printf easier */
#define DRM_ERROR(fmt, arg...) \
do { \
	printf("error: [" DRM_NAME ":%s] *ERROR* ", __func__ ); \
	printf( fmt,## arg ); \
} while (0)

#define DRM_MEM_ERROR(area, fmt, arg...) \
	printf("error: [" DRM_NAME ":%s:%s] *ERROR* " fmt , \
		__func__, DRM(mem_stats)[area].name ,## arg)
#define DRM_INFO(fmt, arg...)  printf("info: " "[" DRM_NAME "] " fmt ,## arg)

#if DRM_DEBUG_CODE
#define DRM_DEBUG(fmt, arg...)						  \
	do {								  \
		if (DRM(flags) & DRM_FLAG_DEBUG)			  \
			printf("[" DRM_NAME ":%s] " fmt , __FUNCTION__ ,## arg); \
	} while (0)
#else
#define DRM_DEBUG(fmt, arg...)		 do { } while (0)
#endif

#define DRM_FIND_MAP(dest, o)						\
	do {								\
		drm_map_list_entry_t *listentry;			\
		TAILQ_FOREACH(listentry, dev->maplist, link) {		\
			if ( listentry->map->offset == o ) {		\
				dest = listentry->map;			\
				break;					\
			}						\
		}							\
	} while (0)

/* Internal functions */

/* drm_drv.h */
extern dev_type_ioctl(DRM(ioctl));
extern dev_type_open(DRM(open));
extern dev_type_close(DRM(close));
extern dev_type_read(DRM(read));
extern dev_type_poll(DRM(poll));
extern dev_type_mmap(DRM(mmap));
extern int		DRM(open_helper)(dev_t kdev, int flags, int fmt, 
					 DRM_STRUCTPROC *p, drm_device_t *dev);
extern drm_file_t	*DRM(find_file_by_proc)(drm_device_t *dev, 
					 DRM_STRUCTPROC *p);

extern int		DRM(sysctl_init)(drm_device_t *dev);
extern int		DRM(sysctl_cleanup)(drm_device_t *dev);
