/* mga_drv.c -- Matrox g200/g400 driver
 * Created: Mon Dec 13 01:56:22 1999 by jhartmann@precisioninsight.com
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
 * Authors: Rickard E. (Rik) Faith <faith@valinux.com>
 *	    Jeff Hartmann <jhartmann@valinux.com>
 *
 *
 */

#include "drmP.h"
#include "mga_drv.h"

#include <pci/pcivar.h>

MODULE_DEPEND(mga, drm, 1, 1, 1);
MODULE_DEPEND(mga, agp, 1, 1, 1);

#define MGA_NAME	 "mga"
#define MGA_DESC	 "Matrox g200/g400"
#define MGA_DATE	 "20000928"
#define MGA_MAJOR	 2
#define MGA_MINOR	 0
#define MGA_PATCHLEVEL	 0

drm_ctx_t		      mga_res_ctx;

static int mga_probe(device_t dev)
{
	const char *s = 0;

	switch (pci_get_devid(dev)) {
	case 0x0525102b:
		s = "Matrox MGA G400 AGP graphics accelerator";
		break;

	case 0x0521102b:
		s = "Matrox MGA G200 AGP graphics accelerator";
		break;
	}

	if (s) {
		device_set_desc(dev, s);
		return 0;
	}

	return ENXIO;
}

static int mga_attach(device_t dev)
{
	return mga_init(dev);
}

static int mga_detach(device_t dev)
{
	mga_cleanup(dev);
	return 0;
}

static device_method_t mga_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mga_probe),
	DEVMETHOD(device_attach,	mga_attach),
	DEVMETHOD(device_detach,	mga_detach),

	{ 0, 0 }
};

static driver_t mga_driver = {
	"drm",
	mga_methods,
	sizeof(drm_device_t),
};

static devclass_t mga_devclass;
#define MGA_SOFTC(unit) \
	((drm_device_t *) devclass_get_softc(mga_devclass, unit))

DRIVER_MODULE(if_mga, pci, mga_driver, mga_devclass, 0, 0);

#define CDEV_MAJOR	145
				/* mga_drv.c */
static struct cdevsw mga_cdevsw = {
	/* open */	mga_open,
	/* close */	mga_close,
	/* read */	drm_read,
	/* write */	drm_write,
	/* ioctl */	mga_ioctl,
	/* poll */	drm_poll,
	/* mmap */	drm_mmap,
	/* strategy */	nostrategy,
	/* name */	"mga",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_TRACKCLOSE,
	/* bmaj */	-1
};

static drm_ioctl_desc_t	      mga_ioctls[] = {
	[DRM_IOCTL_NR(DRM_IOCTL_VERSION)]     = { mga_version,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_UNIQUE)]  = { drm_getunique,  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAGIC)]   = { drm_getmagic,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_IRQ_BUSID)]   = { drm_irq_busid,  0, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_SET_UNIQUE)]  = { drm_setunique,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_BLOCK)]	      = { drm_block,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNBLOCK)]     = { drm_unblock,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_CONTROL)]     = { mga_control,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AUTH_MAGIC)]  = { drm_authmagic,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_MAP)]     = { drm_addmap,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_BUFS)]    = { mga_addbufs,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MARK_BUFS)]   = { mga_markbufs,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_INFO_BUFS)]   = { mga_infobufs,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_MAP_BUFS)]    = { mga_mapbufs,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FREE_BUFS)]   = { mga_freebufs,	  1, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_CTX)]     = { mga_addctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_CTX)]      = { mga_rmctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MOD_CTX)]     = { mga_modctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CTX)]     = { mga_getctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_SWITCH_CTX)]  = { mga_switchctx,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_NEW_CTX)]     = { mga_newctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RES_CTX)]     = { mga_resctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_DRAW)]    = { drm_adddraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_DRAW)]     = { drm_rmdraw,	  1, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_DMA)]	      = { mga_dma,	  1, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_LOCK)]	      = { mga_lock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNLOCK)]      = { mga_unlock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FINISH)]      = { drm_finish,	  1, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ACQUIRE)] = { drm_agp_acquire, 1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_RELEASE)] = { drm_agp_release, 1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ENABLE)]  = { drm_agp_enable,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_INFO)]    = { drm_agp_info,    1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ALLOC)]   = { drm_agp_alloc,   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_FREE)]    = { drm_agp_free,    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_BIND)]    = { drm_agp_bind,    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_UNBIND)]  = { drm_agp_unbind,  1, 1 },
   	[DRM_IOCTL_NR(DRM_IOCTL_MGA_INIT)]    = { mga_dma_init,    1, 1 },
   	[DRM_IOCTL_NR(DRM_IOCTL_MGA_SWAP)]    = { mga_swap_bufs,   1, 0 },
   	[DRM_IOCTL_NR(DRM_IOCTL_MGA_CLEAR)]   = { mga_clear_bufs,  1, 0 },
   	[DRM_IOCTL_NR(DRM_IOCTL_MGA_ILOAD)]   = { mga_iload,       1, 0 },
   	[DRM_IOCTL_NR(DRM_IOCTL_MGA_VERTEX)]  = { mga_vertex,      1, 0 },
   	[DRM_IOCTL_NR(DRM_IOCTL_MGA_FLUSH)]   = { mga_flush_ioctl, 1, 0 },
   	[DRM_IOCTL_NR(DRM_IOCTL_MGA_INDICES)] = { mga_indices,     1, 0 },
};

#define MGA_IOCTL_COUNT DRM_ARRAY_SIZE(mga_ioctls)

static int mga_setup(drm_device_t *dev)
{
	int i;
	
	device_busy(dev->device);

	atomic_set(&dev->ioctl_count, 0);
	atomic_set(&dev->vma_count, 0);
	dev->buf_use	  = 0;
	atomic_set(&dev->buf_alloc, 0);

	drm_dma_setup(dev);

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
			
	DRM_DEBUG("\n");
			
	/* The kernel's context could be created here, but is now created
	   in drm_dma_enqueue.	This is more resource-efficient for
	   hardware that does not do DMA, but may mean that
	   drm_select_queue fails between the time the interrupt is
	   initialized and the time the queues are initialized. */
			
	return 0;
}


static int mga_takedown(drm_device_t *dev)
{
	int		  i;
	drm_magic_entry_t *pt, *next;
	drm_map_t	  *map;
	drm_vma_entry_t	  *vma, *vma_next;

	DRM_DEBUG("\n");

	if (dev->irq) mga_irq_uninstall(dev);
	
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
   				/* Clear AGP information */
	if (dev->agp) {
		drm_agp_mem_t *entry;
		drm_agp_mem_t *nexte;
		
				/* Remove AGP resources, but leave dev->agp
                                   intact until cleanup is called. */
		for (entry = dev->agp->memory; entry; entry = nexte) {
			nexte = entry->next;
			if (entry->bound) drm_unbind_agp(entry->handle);
			drm_free_agp(entry->handle, entry->pages);
			drm_free(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
		}
		dev->agp->memory = NULL;
		
		if (dev->agp->acquired)
			agp_release(dev->agp->agpdev);
		
		dev->agp->acquired = 0;
		dev->agp->enabled  = 0;
	}
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
				break;
			}
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		}
		drm_free(dev->maplist,
			 dev->map_count * sizeof(*dev->maplist),
			 DRM_MEM_MAPS);
		dev->maplist   = NULL;
		dev->map_count = 0;
	}
	
	if (dev->queuelist) {
		for (i = 0; i < dev->queue_count; i++) {
			drm_waitlist_destroy(&dev->queuelist[i]->waitlist);
			if (dev->queuelist[i]) {
				drm_free(dev->queuelist[i],
					 sizeof(*dev->queuelist[0]),
					 DRM_MEM_QUEUES);
				dev->queuelist[i] = NULL;
			}
		}
		drm_free(dev->queuelist,
			 dev->queue_slots * sizeof(*dev->queuelist),
			 DRM_MEM_QUEUES);
		dev->queuelist	 = NULL;
	}

	drm_dma_takedown(dev);

	dev->queue_count     = 0;
	if (dev->lock.hw_lock) {
		dev->lock.hw_lock    = NULL; /* SHM removed */
		dev->lock.pid	     = 0;
		wakeup(&dev->lock.lock_queue);
	}
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
	
	return 0;
}

/* mga_init is called via mga_attach at module load time, */

int
mga_init(device_t nbdev)
{
	int		      retcode;
	drm_device_t	      *dev = device_get_softc(nbdev);

	DRM_DEBUG("\n");

	memset((void *)dev, 0, sizeof(*dev));
	simple_lock_init(&dev->count_lock);
	lockinit(&dev->dev_lock, PZERO, "drmlk", 0, 0);
	
#if 0
	drm_parse_options(mga);
#endif
	dev->device = nbdev;
	dev->devnode = make_dev(&mga_cdevsw,
				device_get_unit(nbdev),
				DRM_DEV_UID,
				DRM_DEV_GID,
				DRM_DEV_MODE,
				MGA_NAME);
	dev->name   = MGA_NAME;

   	DRM_DEBUG("doing mem init\n");
	drm_mem_init();
	DRM_DEBUG("doing proc init\n");
	drm_sysctl_init(dev);
	TAILQ_INIT(&dev->files);
	DRM_DEBUG("doing agp init\n");
	dev->agp    = drm_agp_init();
      	if(dev->agp == NULL) {
	   	DRM_INFO("The mga drm module requires the agp module"
			 " to function correctly\nPlease load the agp"
			 " module before you load the mga module\n");
	   	drm_sysctl_cleanup(dev);
	   	mga_takedown(dev);
	   	return ENOMEM;
	}
#if 0
   	dev->agp->agp_mtrr = mtrr_add(dev->agp->agp_info.aper_base,
				      dev->agp->agp_info.aper_size * 1024 * 1024,
				      MTRR_TYPE_WRCOMB,
				      1);
#endif
	DRM_DEBUG("doing ctxbitmap init\n");
	if((retcode = drm_ctxbitmap_init(dev))) {
		DRM_ERROR("Cannot allocate memory for context bitmap.\n");
		drm_sysctl_cleanup(dev);
		mga_takedown(dev);
		return retcode;
	}

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n",
		 MGA_NAME,
		 MGA_MAJOR,
		 MGA_MINOR,
		 MGA_PATCHLEVEL,
		 MGA_DATE,
		 device_get_unit(nbdev));

	return 0;
}

/* mga_cleanup is called via cleanup_module at module unload time. */

void mga_cleanup(device_t nbdev)
{
	drm_device_t	      *dev = device_get_softc(nbdev);

	DRM_DEBUG("\n");
	
	drm_sysctl_cleanup(dev);
	destroy_dev(dev->devnode);

	DRM_INFO("Module unloaded\n");
	drm_ctxbitmap_cleanup(dev);
	mga_dma_cleanup(dev);
#if 0
   	if(dev->agp && dev->agp->agp_mtrr) {
	   	int retval;
	   	retval = mtrr_del(dev->agp->agp_mtrr, 
				  dev->agp->agp_info.aper_base,
				  dev->agp->agp_info.aper_size * 1024*1024);
	   	DRM_DEBUG("mtrr_del = %d\n", retval);
	}
#endif

	mga_takedown(dev);
	if (dev->agp) {
		drm_free(dev->agp, sizeof(*dev->agp), DRM_MEM_AGPLISTS);
		dev->agp = NULL;
	}
}

int
mga_version(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
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

	version.version_major	   = MGA_MAJOR;
	version.version_minor	   = MGA_MINOR;
	version.version_patchlevel = MGA_PATCHLEVEL;

	DRM_COPY(version.name, MGA_NAME);
	DRM_COPY(version.date, MGA_DATE);
	DRM_COPY(version.desc, MGA_DESC);

	*(drm_version_t *) data = version;
	return 0;
}

int
mga_open(dev_t kdev, int flags, int fmt, struct proc *p)
{
	drm_device_t  *dev    = MGA_SOFTC(minor(kdev));
	int	      retcode = 0;
	
	DRM_DEBUG("open_count = %d\n", dev->open_count);

	device_busy(dev->device);
	if (!(retcode = drm_open_helper(kdev, flags, fmt, p, dev))) {
		atomic_inc(&dev->total_open);
		simple_lock(&dev->count_lock);
		if (!dev->open_count++) {
			simple_unlock(&dev->count_lock);
			retcode = mga_setup(dev);
		}
		simple_unlock(&dev->count_lock);
	}
	device_unbusy(dev->device);

	return retcode;
}

int
mga_close(dev_t kdev, int flags, int fmt, struct proc *p)
{
	drm_device_t  *dev    = kdev->si_drv1;
	drm_file_t    *priv;
	int	      retcode = 0;

	DRM_DEBUG("pid = %d, open_count = %d\n",
		  p->p_pid, dev->open_count);

	priv = drm_find_file_by_proc(dev, p);
	if (!priv) {
		DRM_DEBUG("can't find authenticator\n");
		return EINVAL;
	}

	if (dev->lock.hw_lock && _DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)
	    && dev->lock.pid == p->p_pid) {
	      	mga_reclaim_buffers(dev, priv->pid);
		DRM_ERROR("Process %d dead, freeing lock for context %d\n",
			  p->p_pid,
			  _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
		drm_lock_free(dev,
			      &dev->lock.hw_lock->lock,
			      _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
		
				/* FIXME: may require heavy-handed reset of
                                   hardware at this point, possibly
                                   processed via a callback to the X
                                   server. */
	} else if (dev->lock.hw_lock) {
	   	/* The lock is required to reclaim buffers */
		for (;;) {
			if (!dev->lock.hw_lock) {
				/* Device has been unregistered */
				retcode = EINTR;
				break;
			}
			if (drm_lock_take(&dev->lock.hw_lock->lock,
					  DRM_KERNEL_CONTEXT)) {
				dev->lock.pid	    = p->p_pid;
				dev->lock.lock_time = ticks;
				atomic_inc(&dev->total_locks);
				break;	/* Got lock */
			}			
				/* Contention */
			atomic_inc(&dev->total_sleeps);
			retcode = tsleep(&dev->lock.lock_queue,
					 PZERO|PCATCH,
					 "drmlk2",
					 0);
			if (retcode)
				break;
		}
	   	if(!retcode) {
		   	mga_reclaim_buffers(dev, priv->pid);
		   	drm_lock_free(dev, &dev->lock.hw_lock->lock,
				      DRM_KERNEL_CONTEXT);
		}
	}
	funsetown(dev->buf_sigio);

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, p);
	priv = drm_find_file_by_proc(dev, p);
	if (priv) {
		priv->refs--;
		if (!priv->refs) {
			TAILQ_REMOVE(&dev->files, priv, link);
			drm_free(priv, sizeof(*priv), DRM_MEM_FILES);
		}
	}
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, p);

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
	   	return mga_takedown(dev);
	}
   	simple_unlock(&dev->count_lock);
	return retcode;
}


/* mga_ioctl is called whenever a process performs an ioctl on /dev/drm. */

int
mga_ioctl(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	int		 nr	 = DRM_IOCTL_NR(cmd);
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_file_t	 *priv;
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

	if (nr >= MGA_IOCTL_COUNT) {
		retcode = EINVAL;
	} else {
		ioctl	  = &mga_ioctls[nr];
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

int
mga_unlock(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
        drm_device_t      *dev    = kdev->si_drv1;
	drm_lock_t	  lock;
	int		  s;

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

	s = splsofttq();
	mga_dma_schedule(dev, 1);
	splx(s);

	if (drm_lock_free(dev, &dev->lock.hw_lock->lock,
			  DRM_KERNEL_CONTEXT)) {
	   DRM_ERROR("\n");
	}

	return 0;
}
