/* tdfx.c -- tdfx driver -*- c -*-
 * Created: Thu Oct  7 10:38:32 1999 by faith@precisioninsight.com
 *
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Daryll Strauss <daryll@valinux.com>
 *
 */

#include "drmP.h"
#include "tdfx_drv.h"

#include <pci/pcivar.h>

MODULE_DEPEND(tdfx, drm, 1, 1, 1);
#ifdef DRM_AGP
MODULE_DEPEND(tdfx, agp, 1, 1, 1);
#endif

#define TDFX_NAME	 "tdfx"
#define TDFX_DESC	 "tdfx"
#define TDFX_DATE	 "19991009"
#define TDFX_MAJOR	 1
#define TDFX_MINOR	 0
#define TDFX_PATCHLEVEL  0

static int tdfx_init(device_t nbdev);
static void tdfx_cleanup(device_t nbdev);

drm_ctx_t	              tdfx_res_ctx;

static int tdfx_probe(device_t dev)
{
	const char *s = 0;

	switch (pci_get_devid(dev)) {
	case 0x0003121a:
		s = "3Dfx Voodoo Banshee graphics accelerator";
		break;
		
	case 0x0005121a:
		s = "3Dfx Voodoo 3 graphics accelerator";
		break;
	}

	if (s) {
		device_set_desc(dev, s);
		return 0;
	}

	return ENXIO;
}

static int tdfx_attach(device_t dev)
{
	tdfx_init(dev);
	return 0;
}

static int tdfx_detach(device_t dev)
{
	tdfx_cleanup(dev);
	return 0;
}

static device_method_t tdfx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tdfx_probe),
	DEVMETHOD(device_attach,	tdfx_attach),
	DEVMETHOD(device_detach,	tdfx_detach),

	{ 0, 0 }
};

static driver_t tdfx_driver = {
	"drm",
	tdfx_methods,
	sizeof(drm_device_t),
};

static devclass_t tdfx_devclass;
#define TDFX_SOFTC(unit) \
	((drm_device_t *) devclass_get_softc(tdfx_devclass, unit))

DRIVER_MODULE(if_tdfx, pci, tdfx_driver, tdfx_devclass, 0, 0);

#define CDEV_MAJOR	145
				/* tdfx_drv.c */
static d_open_t tdfx_open;
static d_close_t tdfx_close;
static d_ioctl_t tdfx_version;
static d_ioctl_t tdfx_ioctl;
static d_ioctl_t tdfx_lock;
static d_ioctl_t tdfx_unlock;

static struct cdevsw tdfx_cdevsw = {
	/* open */	tdfx_open,
	/* close */	tdfx_close,
	/* read */	drm_read,
	/* write */	drm_write,
	/* ioctl */	tdfx_ioctl,
	/* poll */	drm_poll,
	/* mmap */	drm_mmap,
	/* strategy */	nostrategy,
	/* name */	"tdfx",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_TRACKCLOSE,
	/* bmaj */	-1
};

static drm_ioctl_desc_t	      tdfx_ioctls[] = {
	[DRM_IOCTL_NR(DRM_IOCTL_VERSION)]    = { tdfx_version,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_UNIQUE)] = { drm_getunique,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAGIC)]  = { drm_getmagic,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_IRQ_BUSID)]  = { drm_irq_busid,	  0, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_SET_UNIQUE)] = { drm_setunique,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_BLOCK)]	     = { drm_block,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNBLOCK)]    = { drm_unblock,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AUTH_MAGIC)] = { drm_authmagic,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_MAP)]    = { drm_addmap,	  1, 1 },
	
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_CTX)]    = { tdfx_addctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_CTX)]     = { tdfx_rmctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MOD_CTX)]    = { tdfx_modctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CTX)]    = { tdfx_getctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_SWITCH_CTX)] = { tdfx_switchctx,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_NEW_CTX)]    = { tdfx_newctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RES_CTX)]    = { tdfx_resctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_DRAW)]   = { drm_adddraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_DRAW)]    = { drm_rmdraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_LOCK)]	     = { tdfx_lock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNLOCK)]     = { tdfx_unlock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FINISH)]     = { drm_finish,	  1, 0 },
#ifdef DRM_AGP
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ACQUIRE)]   = {drm_agp_acquire, 1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_RELEASE)]   = {drm_agp_release, 1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ENABLE)]    = {drm_agp_enable,  1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_INFO)]      = {drm_agp_info,    1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ALLOC)]     = {drm_agp_alloc,   1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_FREE)]      = {drm_agp_free,    1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_BIND)]      = {drm_agp_unbind,  1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_UNBIND)]    = {drm_agp_bind,    1, 1},
#endif
};
#define TDFX_IOCTL_COUNT DRM_ARRAY_SIZE(tdfx_ioctls)

static int
tdfx_setup(drm_device_t *dev)
{
	int i;
	
	device_busy(dev->device);

	atomic_set(&dev->ioctl_count, 0);
	atomic_set(&dev->vma_count, 0);
	dev->buf_use	  = 0;
	atomic_set(&dev->buf_alloc, 0);

	atomic_set(&dev->total_open, 0);
	atomic_set(&dev->total_close, 0);
	atomic_set(&dev->total_ioctl, 0);
	atomic_set(&dev->total_irq, 0);
	atomic_set(&dev->total_ctx, 0);
	atomic_set(&dev->total_locks, 0);
	atomic_set(&dev->total_unlocks, 0);
	atomic_set(&dev->total_contends, 0);
	atomic_set(&dev->total_sleeps, 0);

	for (i = 0; i < DRM_HASH_SIZE; i++) {
		dev->magiclist[i].head = NULL;
		dev->magiclist[i].tail = NULL;
	}
	dev->maplist	    = NULL;
	dev->map_count	    = 0;
	dev->vmalist	    = NULL;
	dev->lock.hw_lock   = NULL;
	dev->lock.lock_queue = 0;
	dev->queue_count    = 0;
	dev->queue_reserved = 0;
	dev->queue_slots    = 0;
	dev->queuelist	    = NULL;
	dev->irq	    = 0;
	dev->context_flag   = 0;
	dev->interrupt_flag = 0;
	dev->dma            = 0;
	dev->dma_flag	    = 0;
	dev->last_context   = 0;
	dev->last_switch    = 0;
	dev->last_checked   = 0;
	callout_init(&dev->timer);
	dev->context_wait   = 0;

	timespecclear(&dev->ctx_start);
	timespecclear(&dev->lck_start);
	
	dev->buf_rp	  = dev->buf;
	dev->buf_wp	  = dev->buf;
	dev->buf_end	  = dev->buf + DRM_BSZ;
	bzero(&dev->buf_sel, sizeof dev->buf_sel);
	dev->buf_sigio	  = NULL;
	dev->buf_readers  = 0;
	dev->buf_writers  = 0;
	dev->buf_selecting = 0;

	tdfx_res_ctx.handle=-1;
			
	DRM_DEBUG("\n");
			
	/* The kernel's context could be created here, but is now created
	   in drm_dma_enqueue.	This is more resource-efficient for
	   hardware that does not do DMA, but may mean that
	   drm_select_queue fails between the time the interrupt is
	   initialized and the time the queues are initialized. */
			
	return 0;
}


static int
tdfx_takedown(drm_device_t *dev)
{
	int		  i;
	drm_magic_entry_t *pt, *next;
	drm_map_t	  *map;
	drm_vma_entry_t	  *vma, *vma_next;

	DRM_DEBUG("\n");

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	callout_stop(&dev->timer);
	
	if (dev->devname) {
		drm_free(dev->devname, strlen(dev->devname)+1, DRM_MEM_DRIVER);
		dev->devname = NULL;
	}
	
	if (dev->unique) {
		drm_free(dev->unique, strlen(dev->unique)+1, DRM_MEM_DRIVER);
		dev->unique = NULL;
		dev->unique_len = 0;
	}
				/* Clear pid list */
	for (i = 0; i < DRM_HASH_SIZE; i++) {
		for (pt = dev->magiclist[i].head; pt; pt = next) {
			next = pt->next;
			drm_free(pt, sizeof(*pt), DRM_MEM_MAGIC);
		}
		dev->magiclist[i].head = dev->magiclist[i].tail = NULL;
	}
#ifdef DRM_AGP
				/* Clear AGP information */
	if (dev->agp) {
		drm_agp_mem_t *temp;
		drm_agp_mem_t *temp_next;
	   
		temp = dev->agp->memory;
		while(temp != NULL) {
			temp_next = temp->next;
			drm_free_agp(temp->handle, temp->pages);
			drm_free(temp, sizeof(*temp), DRM_MEM_AGPLISTS);
			temp = temp_next;
		}

		if (dev->agp->acquired)
			agp_release(dev->agp->agpdev);
		drm_free(dev->agp, sizeof(*dev->agp), DRM_MEM_AGPLISTS);
		dev->agp = NULL;
	}
#endif
	
				/* Clear vma list (only built for debugging) */
	if (dev->vmalist) {
		for (vma = dev->vmalist; vma; vma = vma_next) {
			vma_next = vma->next;
			drm_free(vma, sizeof(*vma), DRM_MEM_VMAS);
		}
		dev->vmalist = NULL;
	}
	
				/* Clear map area and mtrr information */
	if (dev->maplist) {
		for (i = 0; i < dev->map_count; i++) {
			map = dev->maplist[i];
			switch (map->type) {
			case _DRM_REGISTERS:
			case _DRM_FRAME_BUFFER:
#ifdef CONFIG_MTRR
				if (map->mtrr >= 0) {
					int retcode;
					retcode = mtrr_del(map->mtrr,
							   map->offset,
							   map->size);
					DRM_DEBUG("mtrr_del = %d\n", retcode);
				}
#endif
				drm_ioremapfree(map->handle, map->size);
				break;
			case _DRM_SHM:
				drm_free_pages((unsigned long)map->handle,
					       drm_order(map->size)
					       - PAGE_SHIFT,
					       DRM_MEM_SAREA);
				break;
			case _DRM_AGP:
				break; /* XXX */
			}
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		}
		drm_free(dev->maplist,
			 dev->map_count * sizeof(*dev->maplist),
			 DRM_MEM_MAPS);
		dev->maplist   = NULL;
		dev->map_count = 0;
	}
	
	if (dev->lock.hw_lock) {
		dev->lock.hw_lock    = NULL; /* SHM removed */
		dev->lock.pid	     = 0;
		wakeup(&dev->lock.lock_queue);
	}
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);

	return 0;
}

/* tdfx_init is called via tdfx_attach at module load time, */

static int
tdfx_init(device_t nbdev)
{
	drm_device_t *dev = device_get_softc(nbdev);
	int          retcode;

	DRM_DEBUG("\n");

	memset((void *)dev, 0, sizeof(*dev));
	simple_lock_init(&dev->count_lock);
	lockinit(&dev->dev_lock, PZERO, "drmlk", 0, 0);
	
#if 0
	drm_parse_options(tdfx);
#endif

	dev->device = nbdev;
	dev->devnode = make_dev(&tdfx_cdevsw,
				device_get_unit(nbdev),
				DRM_DEV_UID,
				DRM_DEV_GID,
				DRM_DEV_MODE,
				TDFX_NAME);
	dev->name   = TDFX_NAME;

	drm_mem_init();
	drm_sysctl_init(dev);
	TAILQ_INIT(&dev->files);

#ifdef DRM_AGP
	dev->agp    = drm_agp_init();
#endif
	if((retcode = drm_ctxbitmap_init(dev))) {
		DRM_ERROR("Cannot allocate memory for context bitmap.\n");
		drm_sysctl_cleanup(dev);
		tdfx_takedown(dev);
		return retcode;
	}

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n",
		 TDFX_NAME,
		 TDFX_MAJOR,
		 TDFX_MINOR,
		 TDFX_PATCHLEVEL,
		 TDFX_DATE,
		 device_get_unit(nbdev));
	
	return 0;
}

/* tdfx_cleanup is called via tdfx_detach at module unload time. */

static void
tdfx_cleanup(device_t nbdev)
{
	drm_device_t *dev = device_get_softc(nbdev);

	DRM_DEBUG("\n");
	
	drm_sysctl_cleanup(dev);
	destroy_dev(dev->devnode);

	DRM_INFO("Module unloaded\n");

	drm_ctxbitmap_cleanup(dev);
	tdfx_takedown(dev);
}

static int
tdfx_version(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	drm_version_t version;
	int	      len;

	version = *(drm_version_t *) data;

#define DRM_COPY(name,value)				\
	len = strlen(value);				\
	if (len > name##_len) len = name##_len;		\
	name##_len = strlen(value);			\
	if (len && name) {				\
		int error = copyout(value, name, len);	\
		if (error) return error;		\
	}

	version.version_major	   = TDFX_MAJOR;
	version.version_minor	   = TDFX_MINOR;
	version.version_patchlevel = TDFX_PATCHLEVEL;

	DRM_COPY(version.name, TDFX_NAME);
	DRM_COPY(version.date, TDFX_DATE);
	DRM_COPY(version.desc, TDFX_DESC);

	*(drm_version_t *) data = version;
	return 0;
}

static int
tdfx_open(dev_t kdev, int flags, int fmt, struct proc *p)
{
	drm_device_t  *dev    = TDFX_SOFTC(minor(kdev));
	int	      retcode = 0;
	
	DRM_DEBUG("open_count = %d\n", dev->open_count);

	device_busy(dev->device);
	if (!(retcode = drm_open_helper(kdev, flags, fmt, p, dev))) {
		atomic_inc(&dev->total_open);
		simple_lock(&dev->count_lock);
		if (!dev->open_count++) {
			simple_unlock(&dev->count_lock);
			retcode = tdfx_setup(dev);
		}
		simple_unlock(&dev->count_lock);
	}
	device_unbusy(dev->device);

	return retcode;
}

static int
tdfx_close(dev_t kdev, int flags, int fmt, struct proc *p)
{
	drm_device_t  *dev    = kdev->si_drv1;
	int	      retcode = 0;

	DRM_DEBUG("open_count = %d\n", dev->open_count);
	if (!(retcode = drm_close(kdev, flags, fmt, p))) {
		atomic_inc(&dev->total_close);
		simple_lock(&dev->count_lock);
		if (!--dev->open_count) {
			if (atomic_read(&dev->ioctl_count) || dev->blocked) {
				DRM_ERROR("Device busy: %d %d\n",
					  atomic_read(&dev->ioctl_count),
					  dev->blocked);
				simple_unlock(&dev->count_lock);
				return EBUSY;
			}
			simple_unlock(&dev->count_lock);
			device_unbusy(dev->device);
			return tdfx_takedown(dev);
		}
		simple_unlock(&dev->count_lock);
	}

	return retcode;
}

/* tdfx_ioctl is called whenever a process performs an ioctl on /dev/drm. */

static int
tdfx_ioctl(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	int		 nr	 = DRM_IOCTL_NR(cmd);
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_file_t       *priv;
	int		 retcode = 0;
	drm_ioctl_desc_t *ioctl;
	d_ioctl_t	 *func;

	DRM_DEBUG("dev=%p\n", dev);
	priv = drm_find_file_by_proc(dev, p);
	if (!priv) {
		DRM_DEBUG("can't find authenticator\n");
		return EINVAL;
	}

	atomic_inc(&dev->ioctl_count);
	atomic_inc(&dev->total_ioctl);
	++priv->ioctl_count;
	
	DRM_DEBUG("pid = %d, cmd = 0x%02lx, nr = 0x%02x, auth = %d\n",
		  p->p_pid, cmd, nr, priv->authenticated);

	switch (cmd) {
	case FIONBIO:
		atomic_dec(&dev->ioctl_count);
		return 0;

	case FIOASYNC:
		atomic_dec(&dev->ioctl_count);
		dev->flags |= FASYNC;
		return 0;

	case FIOSETOWN:
		atomic_dec(&dev->ioctl_count);
		return fsetown(*(int *)data, &dev->buf_sigio);

	case FIOGETOWN:
		atomic_dec(&dev->ioctl_count);
		*(int *) data = fgetown(dev->buf_sigio);
		return 0;
	}

	if (nr >= TDFX_IOCTL_COUNT) {
		retcode = EINVAL;
	} else {
		ioctl	  = &tdfx_ioctls[nr];
		func	  = ioctl->func;

		if (!func) {
			DRM_DEBUG("no function\n");
			retcode = EINVAL;
		} else if ((ioctl->root_only && suser(p))
			    || (ioctl->auth_needed && !priv->authenticated)) {
			retcode = EACCES;
		} else {
			retcode = (func)(kdev, cmd, data, flags, p);
		}
	}
	
	atomic_dec(&dev->ioctl_count);
	return retcode;
}

static int
tdfx_lock(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
        drm_device_t      *dev    = kdev->si_drv1;
        int               ret   = 0;
        drm_lock_t        lock;
#if DRM_DMA_HISTOGRAM

        getnanotime(&dev->lck_start);
#endif

	lock = *(drm_lock_t *) data;

        if (lock.context == DRM_KERNEL_CONTEXT) {
                DRM_ERROR("Process %d using kernel context %d\n",
                          p->p_pid, lock.context);
                return EINVAL;
        }

        DRM_DEBUG("%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
                  lock.context, p->p_pid, dev->lock.hw_lock->lock,
                  lock.flags);

#if 0
				/* dev->queue_count == 0 right now for
                                   tdfx.  FIXME? */
        if (lock.context < 0 || lock.context >= dev->queue_count)
                return EINVAL;
#endif
        
        if (!ret) {
#if 0
                if (_DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock)
                    != lock.context) {
                        long j = ticks - dev->lock.lock_time;

                        if (lock.context == tdfx_res_ctx.handle &&
				j >= 0 && j < DRM_LOCK_SLICE) {
                                /* Can't take lock if we just had it and
                                   there is contention. */
                                DRM_DEBUG("%d (pid %d) delayed j=%d dev=%d ticks=%d\n",
					lock.context, p->p_pid, j, 
					dev->lock.lock_time, ticks);
				ret = tsleep(&never, PZERO|PCATCH, "drmlk1",
					       DRM_LOCK_SLICE - j);
				if (ret)
					return ret;
				DRM_DEBUG("ticks=%d\n", ticks);
                        }
                }
#endif
                for (;;) {
                        if (!dev->lock.hw_lock) {
                                /* Device has been unregistered */
                                ret = EINTR;
                                break;
                        }
                        if (drm_lock_take(&dev->lock.hw_lock->lock,
                                          lock.context)) {
                                dev->lock.pid       = p->p_pid;
                                dev->lock.lock_time = ticks;
                                atomic_inc(&dev->total_locks);
                                break;  /* Got lock */
                        }
                        
                                /* Contention */
                        atomic_inc(&dev->total_sleeps);
			ret = tsleep(&dev->lock.lock_queue,
				       PZERO|PCATCH,
				       "drmlk2",
				       0);
			if (ret)
				break;
                }
        }

#if 0
	if (!ret && dev->last_context != lock.context &&
		lock.context != tdfx_res_ctx.handle &&
		dev->last_context != tdfx_res_ctx.handle) {
		add_wait_queue(&dev->context_wait, &entry);
	        current->state = TASK_INTERRUPTIBLE;
                /* PRE: dev->last_context != lock.context */
	        tdfx_context_switch(dev, dev->last_context, lock.context);
		/* POST: we will wait for the context
                   switch and will dispatch on a later call
                   when dev->last_context == lock.context
                   NOTE WE HOLD THE LOCK THROUGHOUT THIS
                   TIME! */
		current->policy |= SCHED_YIELD;
	        schedule();
	        current->state = TASK_RUNNING;
	        remove_wait_queue(&dev->context_wait, &entry);
	        if (signal_pending(current)) {
	                ret = EINTR;
	        } else if (dev->last_context != lock.context) {
			DRM_ERROR("Context mismatch: %d %d\n",
                        	dev->last_context, lock.context);
	        }
	}
#endif

        if (!ret) {
                if (lock.flags & _DRM_LOCK_READY) {
				/* Wait for space in DMA/FIFO */
		}
                if (lock.flags & _DRM_LOCK_QUIESCENT) {
				/* Make hardware quiescent */
#if 0
                        tdfx_quiescent(dev);
#endif
		}
        }

#if 0
	DRM_ERROR("pid = %5d, old counter = %5ld\n", 
		p->p_pid, current->counter);
#endif
#if 0
	while (current->counter > 25)
		current->counter >>= 1; /* decrease time slice */
	DRM_ERROR("pid = %5d, new counter = %5ld\n",
		 p->p_pid, current->counter);
#endif
        DRM_DEBUG("%d %s\n", lock.context, ret ? "interrupted" : "has lock");

#if DRM_DMA_HISTOGRAM
	{
	    struct timespec ts;
	    getnanotime(&ts);
	    timespecsub(&ts, &dev->lck_start);
	    atomic_inc(&dev->histo.lhld[drm_histogram_slot(&ts)]);
	}
#endif
        
        return ret;
}


static int
tdfx_unlock(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	drm_device_t	  *dev	  = kdev->si_drv1;
	drm_lock_t	  lock;

	lock = *(drm_lock_t *) data;
	
	if (lock.context == DRM_KERNEL_CONTEXT) {
		DRM_ERROR("Process %d using kernel context %d\n",
			  p->p_pid, lock.context);
		return EINVAL;
	}

	DRM_DEBUG("%d frees lock (%d holds)\n",
		  lock.context,
		  _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
	atomic_inc(&dev->total_unlocks);
	if (_DRM_LOCK_IS_CONT(dev->lock.hw_lock->lock))
		atomic_inc(&dev->total_contends);
	drm_lock_transfer(dev, &dev->lock.hw_lock->lock, DRM_KERNEL_CONTEXT);
				/* FIXME: Try to send data to card here */
	if (!dev->context_flag) {
		if (drm_lock_free(dev, &dev->lock.hw_lock->lock,
				  DRM_KERNEL_CONTEXT)) {
			DRM_ERROR("\n");
		}
	}

	return 0;
}
