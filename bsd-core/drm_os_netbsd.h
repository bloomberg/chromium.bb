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
/* For TIOCSPGRP/TIOCGPGRP */
#include <sys/ttycom.h>

#include <uvm/uvm.h>

#include <machine/pmap.h>
#include <machine/bus.h>
#include <sys/resourcevar.h>
#include <machine/sysarch.h>
#include <machine/mtrr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
 
#include "drmvar.h"

#define __REALLY_HAVE_AGP	__HAVE_AGP

#define __REALLY_HAVE_MTRR	0
#define __REALLY_HAVE_SG	0

#if __REALLY_HAVE_AGP
#include <dev/pci/agpvar.h>
#include <sys/agpio.h>
#endif

#define device_t struct device *
extern struct cfdriver DRM(_cd);

#if DRM_DEBUG
#undef  DRM_DEBUG_CODE
#define DRM_DEBUG_CODE 2
#endif
#undef DRM_DEBUG

#define DRM_TIME_SLICE	      (hz/20)  /* Time slice for GLXContexts	  */

#define DRM_DEV_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP)
#define DRM_DEV_UID	0
#define DRM_DEV_GID	0
#define CDEV_MAJOR	90

#define DRM_CURPROC		curproc
#define DRM_STRUCTPROC		struct proc
#define DRM_SPINTYPE		struct simplelock
#define DRM_SPININIT(l,name)	simple_lock_init(&l)
#define DRM_SPINUNINIT(l)
#define DRM_SPINLOCK(l)		simple_lock(l)
#define DRM_SPINUNLOCK(u)	simple_unlock(u);
#define DRM_CURRENTPID		curproc->p_pid

#define DRM_IOCTL_ARGS		dev_t kdev, u_long cmd, caddr_t data, int flags, DRM_STRUCTPROC *p
#define DRM_LOCK		lockmgr(&dev->dev_lock, LK_EXCLUSIVE, NULL)
#define DRM_UNLOCK 		lockmgr(&dev->dev_lock, LK_RELEASE, NULL)
#define DRM_SUSER(p)		suser(p->p_ucred, &p->p_acflag)
#define DRM_TASKQUEUE_ARGS	void *dev, int pending
#define DRM_IRQ_ARGS		void *device
#define DRM_DEVICE		drm_device_t *dev = device_lookup(&DRM(_cd), minor(kdev))
#define DRM_MALLOC(size)	malloc( size, DRM(M_DRM), M_NOWAIT )
#define DRM_FREE(pt)		free( pt, DRM(M_DRM) )
#define DRM_VTOPHYS(addr)	vtophys(addr)
#define DRM_READ8(addr)	*((volatile char *)(addr))
#define DRM_READ32(addr)	*((volatile long *)(addr))
#define DRM_WRITE8(addr, val)	*((volatile char *)(addr)) = (val)
#define DRM_WRITE32(addr, val)	*((volatile long *)(addr)) = (val)
#define DRM_AGP_FIND_DEVICE()

#define DRM_PRIV					\
	drm_file_t	*priv	= (drm_file_t *) DRM(find_file_by_proc)(dev, p); \
	if (!priv) {						\
		DRM_DEBUG("can't find authenticator\n");	\
		return EINVAL;					\
	}

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
		drm_map_t *map = listentry->map;		\
		if (map->type == _DRM_SHM &&			\
			map->flags & _DRM_CONTAINS_LOCK) {	\
			dev_priv->sarea = map;			\
			break;					\
		}						\
	}							\
} while (0)

#define return DRM_ERR(v)	return v;
#define DRM_ERR(v)		v

#define DRM_COPY_TO_USER_IOCTL(arg1, arg2, arg3) \
	*arg1 = arg2
#define DRM_COPY_FROM_USER_IOCTL(arg1, arg2, arg3) \
	arg1 = *arg2
#define DRM_COPY_TO_USER(arg1, arg2, arg3) \
	copyout(arg2, arg1, arg3)
#define DRM_COPY_FROM_USER(arg1, arg2, arg3) \
	copyin(arg2, arg1, arg3)

#define DRM_READMEMORYBARRIER \
{												\
   	int xchangeDummy;									\
	DRM_DEBUG("%s\n", __FUNCTION__);							\
   	__asm__ volatile(" push %%eax ; xchg %%eax, %0 ; pop %%eax" : : "m" (xchangeDummy));	\
   	__asm__ volatile(" push %%eax ; push %%ebx ; push %%ecx ; push %%edx ;"			\
			 " movl $0,%%eax ; cpuid ; pop %%edx ; pop %%ecx ; pop %%ebx ;"		\
			 " pop %%eax" : /* no outputs */ :  /* no inputs */ );			\
} while (0);

#define DRM_WRITEMEMORYBARRIER DRM_READMEMORYBARRIER

#define DRM_WAKEUP(w) wakeup(w)
#define DRM_WAKEUP_INT(w) wakeup(w)

#define PAGE_ALIGN(addr) (((addr)+PAGE_SIZE-1)&PAGE_MASK)

typedef struct drm_chipinfo
{
	int vendor;
	int device;
	int supported;
	char *name;
} drm_chipinfo_t;

typedef u_int32_t dma_addr_t;
typedef volatile u_int32_t atomic_t;
typedef u_int32_t cycles_t;
typedef u_int32_t spinlock_t;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
typedef dev_type_ioctl(d_ioctl_t);
typedef vaddr_t vm_offset_t;

#define atomic_set(p, v)	(*(p) = (v))
#define atomic_read(p)		(*(p))
#define atomic_inc(p)		atomic_add_int(p, 1)
#define atomic_dec(p)		atomic_subtract_int(p, 1)
#define atomic_add(n, p)	atomic_add_int(p, n)
#define atomic_sub(n, p)	atomic_subtract_int(p, n)

/* FIXME: Is NetBSD's kernel non-reentrant? */
#define atomic_add_int(p, v)      *(p) += v
#define atomic_subtract_int(p, v) *(p) -= v
#define atomic_set_int(p, bits)   *(p) |= (bits)
#define atomic_clear_int(p, bits) *(p) &= ~(bits)

/* Fake this */
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

/* Redefinitions to make templating easy */
#define wait_queue_head_t	atomic_t
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
			printf("[" DRM_NAME ":%s] " fmt , __FUNCTION__,## arg); \
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
extern dev_type_ioctl(DRM(lock));
extern dev_type_ioctl(DRM(unlock));
extern dev_type_open(DRM(open));
extern dev_type_close(DRM(close));
extern dev_type_read(DRM(read));
extern dev_type_write(DRM(write));
extern dev_type_poll(DRM(poll));
extern dev_type_mmap(DRM(mmap));
extern int		DRM(open_helper)(dev_t kdev, int flags, int fmt, 
					 DRM_STRUCTPROC *p, drm_device_t *dev);
extern drm_file_t	*DRM(find_file_by_proc)(drm_device_t *dev, 
					 DRM_STRUCTPROC *p);

/* Misc. IOCTL support (drm_ioctl.h) */
extern dev_type_ioctl(DRM(irq_busid));
extern dev_type_ioctl(DRM(getunique));
extern dev_type_ioctl(DRM(setunique));
extern dev_type_ioctl(DRM(getmap));
extern dev_type_ioctl(DRM(getclient));
extern dev_type_ioctl(DRM(getstats));

/* Context IOCTL support (drm_context.h) */
extern dev_type_ioctl(DRM(resctx));
extern dev_type_ioctl(DRM(addctx));
extern dev_type_ioctl(DRM(modctx));
extern dev_type_ioctl(DRM(getctx));
extern dev_type_ioctl(DRM(switchctx));
extern dev_type_ioctl(DRM(newctx));
extern dev_type_ioctl(DRM(rmctx));
extern dev_type_ioctl(DRM(setsareactx));
extern dev_type_ioctl(DRM(getsareactx));

/* Drawable IOCTL support (drm_drawable.h) */
extern dev_type_ioctl(DRM(adddraw));
extern dev_type_ioctl(DRM(rmdraw));

/* Authentication IOCTL support (drm_auth.h) */
extern dev_type_ioctl(DRM(getmagic));
extern dev_type_ioctl(DRM(authmagic));

/* Locking IOCTL support (drm_lock.h) */
extern dev_type_ioctl(DRM(block));
extern dev_type_ioctl(DRM(unblock));
extern dev_type_ioctl(DRM(finish));

/* Buffer management support (drm_bufs.h) */
extern dev_type_ioctl(DRM(addmap));
extern dev_type_ioctl(DRM(rmmap));
#if __HAVE_DMA
extern dev_type_ioctl(DRM(addbufs_agp));
extern dev_type_ioctl(DRM(addbufs_pci));
extern dev_type_ioctl(DRM(addbufs_sg));
extern dev_type_ioctl(DRM(addbufs));
extern dev_type_ioctl(DRM(infobufs));
extern dev_type_ioctl(DRM(markbufs));
extern dev_type_ioctl(DRM(freebufs));
extern dev_type_ioctl(DRM(mapbufs));
#endif

/* DMA support (drm_dma.h) */
#if __HAVE_DMA
extern dev_type_ioctl(DRM(control));
#endif

/* AGP/GART support (drm_agpsupport.h) */
#if __REALLY_HAVE_AGP
extern dev_type_ioctl(DRM(agp_acquire));
extern dev_type_ioctl(DRM(agp_release));
extern dev_type_ioctl(DRM(agp_enable));
extern dev_type_ioctl(DRM(agp_info));
extern dev_type_ioctl(DRM(agp_alloc));
extern dev_type_ioctl(DRM(agp_free));
extern dev_type_ioctl(DRM(agp_unbind));
extern dev_type_ioctl(DRM(agp_bind));
#endif

/* Scatter Gather Support (drm_scatter.h) */
#if __HAVE_SG
extern dev_type_ioctl(DRM(sg_alloc));
extern dev_type_ioctl(DRM(sg_free));
#endif

/* SysCtl Support (drm_sysctl.h) */
extern int		DRM(sysctl_init)(drm_device_t *dev);
extern int		DRM(sysctl_cleanup)(drm_device_t *dev);
