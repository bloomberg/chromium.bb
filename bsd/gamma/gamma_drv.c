/* gamma.c -- 3dlabs GMX 2000 driver -*- c -*-
 * Created: Mon Jan  4 08:58:31 1999 by faith@precisioninsight.com
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
 *
 */

#include "drmP.h"
#include "gamma_drv.h"

#include <pci/pcivar.h>

MODULE_DEPEND(gamma, drm, 1, 1, 1);

#ifndef PCI_DEVICE_ID_3DLABS_GAMMA
#define PCI_DEVICE_ID_3DLABS_GAMMA 0x0008
#endif
#ifndef PCI_DEVICE_ID_3DLABS_MX
#define PCI_DEVICE_ID_3DLABS_MX 0x0006
#endif
#ifndef PCI_VENDOR_ID_3DLABS
#define PCI_VENDOR_ID_3DLABS 0x3d3d
#endif

static int gamma_init(device_t nbdev);
static void gamma_cleanup(device_t nbdev);

static int gamma_probe(device_t dev)
{
	const char *s = 0;

	switch (pci_get_devid(dev)) {
	case 0x00083d3d:
		s = "3D Labs Gamma graphics accelerator";
		break;

	case 0x00063d3d:
		s = "3D Labs MX graphics accelerator";
		break;
	}

	if (s) {
		device_set_desc(dev, s);
		return 0;
	}

	return ENXIO;
}

static int gamma_attach(device_t dev)
{
	gamma_init(dev);
	return 0;
}

static int gamma_detach(device_t dev)
{
	gamma_cleanup(dev);
	return 0;
}

static device_method_t gamma_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gamma_probe),
	DEVMETHOD(device_attach,	gamma_attach),
	DEVMETHOD(device_detach,	gamma_detach),

	{ 0, 0 }
};

static driver_t gamma_driver = {
	"drm",
	gamma_methods,
	sizeof(drm_device_t),
};

static devclass_t gamma_devclass;
#define GAMMA_SOFTC(unit) \
	((drm_device_t *) devclass_get_softc(gamma_devclass, unit))

DRIVER_MODULE(if_gamma, pci, gamma_driver, gamma_devclass, 0, 0);

#define GAMMA_NAME	 "gamma"
#define GAMMA_DESC	 "3dlabs GMX 2000"
#define GAMMA_DATE	 "20000606"
#define GAMMA_MAJOR	 1
#define GAMMA_MINOR	 0
#define GAMMA_PATCHLEVEL 0

#define CDEV_MAJOR	200

static struct cdevsw gamma_cdevsw = {
	/* open */	gamma_open,
	/* close */	gamma_close,
	/* read */	drm_read,
	/* write */	drm_write,
	/* ioctl */	gamma_ioctl,
	/* poll */	nopoll,
	/* mmap */	drm_mmap,
	/* strategy */	nostrategy,
	/* name */	"gamma",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TTY | D_TRACKCLOSE,
	/* bmaj */	-1
};

static drm_ioctl_desc_t	      gamma_ioctls[] = {
	[DRM_IOCTL_NR(DRM_IOCTL_VERSION)]    = { gamma_version,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_UNIQUE)] = { drm_getunique,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAGIC)]  = { drm_getmagic,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_IRQ_BUSID)]  = { drm_irq_busid,	  0, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_SET_UNIQUE)] = { drm_setunique,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_BLOCK)]	     = { drm_block,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNBLOCK)]    = { drm_unblock,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_CONTROL)]    = { gamma_control,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AUTH_MAGIC)] = { drm_authmagic,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_MAP)]    = { drm_addmap,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_BUFS)]   = { drm_addbufs,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MARK_BUFS)]  = { drm_markbufs,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_INFO_BUFS)]  = { drm_infobufs,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_MAP_BUFS)]   = { drm_mapbufs,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FREE_BUFS)]  = { drm_freebufs,	  1, 0 },
	
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_CTX)]    = { drm_addctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_CTX)]     = { drm_rmctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MOD_CTX)]    = { drm_modctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CTX)]    = { drm_getctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_SWITCH_CTX)] = { drm_switchctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_NEW_CTX)]    = { drm_newctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RES_CTX)]    = { drm_resctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_DRAW)]   = { drm_adddraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_DRAW)]    = { drm_rmdraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_DMA)]	     = { gamma_dma,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_LOCK)]	     = { gamma_lock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNLOCK)]     = { gamma_unlock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FINISH)]     = { drm_finish,	  1, 0 },
};
#define GAMMA_IOCTL_COUNT DRM_ARRAY_SIZE(gamma_ioctls)

static int 		      devices = 0;

static int gamma_setup(drm_device_t *dev)
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
#if DRM_DMA_HISTO
	memset(&dev->histo, 0, sizeof(dev->histo));
#endif
	timespecclear(&dev->ctx_start);
	timespecclear(&dev->lck_start);
	
	dev->buf_rp	  = dev->buf;
	dev->buf_wp	  = dev->buf;
	dev->buf_end	  = dev->buf + DRM_BSZ;
	dev->buf_sigio	  = NULL;
			
	DRM_DEBUG("\n");
			
	/* The kernel's context could be created here, but is now created
	   in drm_dma_enqueue.	This is more resource-efficient for
	   hardware that does not do DMA, but may mean that
	   drm_select_queue fails between the time the interrupt is
	   initialized and the time the queues are initialized. */
			
	return 0;
}


static int
gamma_takedown(drm_device_t *dev)
{
	int		  i;
	drm_magic_entry_t *pt, *next;
	drm_map_t	  *map;
	drm_vma_entry_t	  *vma, *vma_next;

	DRM_DEBUG("\n");

	if (dev->irq) gamma_irq_uninstall(dev);
	
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
				/* Do nothing here, because this is all
                                   handled in the AGP/GART driver. */
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
	
	device_unbusy(dev->device);

	return 0;
}

int gamma_found(void)
{
	return devices;
}

static int
gamma_find_devices(device_t dev)
{
	device_t *children, child;
	int nchildren, i;
	int count = 0;

	if (device_get_children(device_get_parent(dev), &children, &nchildren))
		return 0;

	for (i = 0; i < nchildren; i++) {
		child = children[i];

		if (pci_get_slot(dev) == pci_get_slot(child) &&
		    pci_get_vendor(child) == PCI_VENDOR_ID_3DLABS &&
		    pci_get_device(child) == PCI_DEVICE_ID_3DLABS_MX) {
			count++;
		}
	}
	free(children, M_TEMP);

	/* we don't currently support more than two */
	if (count > 2) count = 2;

	return count;
}

/* gamma_init is called via gamma_attach at module load time */

static int
gamma_init(device_t nbdev)
{
	drm_device_t	      *dev = device_get_softc(nbdev);

	DRM_DEBUG("\n");

	memset((void *)dev, 0, sizeof(*dev));
	simple_lock_init(&dev->count_lock);
	lockinit(&dev->dev_lock, PZERO, "drmlk", 0, 0);
	
#if 0				/* XXX use getenv I guess */
	drm_parse_options(gamma);
#endif
	devices = gamma_find_devices(nbdev);
	if (devices == 0) return -1;

#if 0
	if ((retcode = misc_register(&gamma_misc))) {
		DRM_ERROR("Cannot register \"%s\"\n", GAMMA_NAME);
		return retcode;
	}
#endif
	dev->device = nbdev;
	dev->devnode = make_dev(&gamma_cdevsw,
				device_get_unit(nbdev),
				DRM_DEV_UID,
				DRM_DEV_GID,
				DRM_DEV_MODE,
				GAMMA_NAME);
	dev->name   = GAMMA_NAME;

	drm_mem_init();
	drm_sysctl_init(dev);

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d with %d MX devices\n",
		 GAMMA_NAME,
		 GAMMA_MAJOR,
		 GAMMA_MINOR,
		 GAMMA_PATCHLEVEL,
		 GAMMA_DATE,
		 device_get_unit(nbdev),
		 devices);

	return 0;
}

/* gamma_cleanup is called via gamma_detach at module unload time. */

static void
gamma_cleanup(device_t nbdev)
{
	drm_device_t	      *dev = device_get_softc(nbdev);

	DRM_DEBUG("\n");
	
	drm_sysctl_cleanup(dev);
#if 0
	if (misc_deregister(&gamma_misc)) {
		DRM_ERROR("Cannot unload module\n");
	} else {
		DRM_INFO("Module unloaded\n");
	}
#endif
	device_busy(dev->device);
	gamma_takedown(dev);
}

SYSUNINIT(gamma_cleanup, SI_SUB_DRIVERS, SI_ORDER_ANY, gamma_cleanup, 0);

#if 0
int gamma_version(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_version_t version;
	int	      len;

	copy_from_user_ret(&version,
			   (drm_version_t *)arg,
			   sizeof(version),
			   -EFAULT);

#define DRM_COPY(name,value)				     \
	len = strlen(value);				     \
	if (len > name##_len) len = name##_len;		     \
	name##_len = strlen(value);			     \
	if (len && name) {				     \
		copy_to_user_ret(name, value, len, -EFAULT); \
	}

	version.version_major	   = GAMMA_MAJOR;
	version.version_minor	   = GAMMA_MINOR;
	version.version_patchlevel = GAMMA_PATCHLEVEL;

	DRM_COPY(version.name, GAMMA_NAME);
	DRM_COPY(version.date, GAMMA_DATE);
	DRM_COPY(version.desc, GAMMA_DESC);

	copy_to_user_ret((drm_version_t *)arg,
			 &version,
			 sizeof(version),
			 -EFAULT);
	return 0;
}
#endif

int
gamma_open(dev_t kdev, int flags, int fmt, struct proc *p)
{
	drm_device_t  *dev    = GAMMA_SOFTC(minor(kdev));
	int	      retcode = 0;
	
	DRM_DEBUG("open_count = %d\n", dev->open_count);

	device_busy(dev->device);
	if (!(retcode = drm_open_helper(kdev, flags, fmt, p, dev))) {
		atomic_inc(&dev->total_open);
		simple_lock(&dev->count_lock);
		if (!dev->open_count++) {
			simple_unlock(&dev->count_lock);
			retcode = gamma_setup(dev);
		}
		simple_unlock(&dev->count_lock);
	}
	device_unbusy(dev->device);

	return retcode;
}

int
gamma_close(dev_t kdev, int flags, int fmt, struct proc *p)
{
	drm_device_t	 *dev	 = kdev->si_drv1;
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
			return gamma_takedown(dev);
		}
		simple_unlock(&dev->count_lock);
	}
	return retcode;
}

/* drm_ioctl is called whenever a process performs an ioctl on /dev/drm. */

int
gamma_ioctl(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	int		 nr	 = DRM_IOCTL_NR(cmd);
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_file_t	 *priv;
	int		 retcode = 0;
	drm_ioctl_desc_t *ioctl;
	d_ioctl_t	 *func;

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
	case FIOSETOWN:
		return fsetown(*(int *)data, &dev->buf_sigio);

	case FIOGETOWN:
		*(int *) data = fgetown(dev->buf_sigio);
		return 0;
	}

	if (nr >= GAMMA_IOCTL_COUNT) {
		retcode = EINVAL;
	} else {
		ioctl	  = &gamma_ioctls[nr];
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

int gamma_unlock(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	drm_device_t	  *dev	  = kdev->si_drv1;
	drm_lock_t	  *lockp  = (drm_lock_t *) data;

	if (lockp->context == DRM_KERNEL_CONTEXT) {
		DRM_ERROR("Process %d using kernel context %d\n",
			  p->p_pid, lockp->context);
		return -EINVAL;
	}

	DRM_DEBUG("%d frees lock (%d holds)\n",
		  lockp->context,
		  _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
	atomic_inc(&dev->total_unlocks);
	if (_DRM_LOCK_IS_CONT(dev->lock.hw_lock->lock))
		atomic_inc(&dev->total_contends);
	drm_lock_transfer(dev, &dev->lock.hw_lock->lock, DRM_KERNEL_CONTEXT);
	gamma_dma_schedule(dev, 1);
	if (!dev->context_flag) {
		if (drm_lock_free(dev, &dev->lock.hw_lock->lock,
				  DRM_KERNEL_CONTEXT)) {
			DRM_ERROR("\n");
		}
	}

#if DRM_DMA_HISTOGRAM
	{
	    struct timespec ts;
	    getnanotime(&ts);
	    timespecsub(&ts, &dev->lck_start);
	    atomic_inc(&dev->histo.lhld[drm_histogram_slot(&ts)]);
	}
#endif
	
	return 0;
}
