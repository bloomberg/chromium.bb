/* fops.c -- File operations for DRM -*- c -*-
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
 *    Daryll Strauss <daryll@valinux.com>
 *
 */

#define __NO_VERSION__
#include "drmP.h"
#include <sys/signalvar.h>
#include <sys/poll.h>

drm_file_t *drm_find_file_by_proc(drm_device_t *dev, struct proc *p)
{
	uid_t uid = p->p_cred->p_svuid;
	pid_t pid = p->p_pid;
	drm_file_t *priv;

	TAILQ_FOREACH(priv, &dev->files, link)
		if (priv->pid == pid && priv->uid == uid)
			return priv;
	return NULL;
}


/* drm_open is called whenever a process opens /dev/drm. */

int drm_open_helper(dev_t kdev, int flags, int fmt, struct proc *p,
		    drm_device_t *dev)
{
	int	     m = minor(kdev);
	drm_file_t   *priv;

	if (flags & O_EXCL)
		return EBUSY; /* No exclusive opens */

	dev->flags = flags;

	DRM_DEBUG("pid = %d, device = %p, minor = %d\n",
		  p->p_pid, dev->device, m);

	priv = drm_find_file_by_proc(dev, p);
	if (priv) {
		priv->refs++;
	} else {
		priv		    = drm_alloc(sizeof(*priv), DRM_MEM_FILES);
		memset(priv, 0, sizeof(*priv));
		priv->uid	    = p->p_cred->p_svuid;
		priv->pid	    = p->p_pid;
		priv->refs          = 1;
		priv->minor	    = m;
		priv->devXX	    = dev;
		priv->ioctl_count   = 0;
		priv->authenticated = !suser(p);
		lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, p);
		TAILQ_INSERT_TAIL(&dev->files, priv, link);
		lockmgr(&dev->dev_lock, LK_RELEASE, 0, p);
	}

	kdev->si_drv1 = dev;
	
	return 0;
}

int drm_write(dev_t kdev, struct uio *uio, int ioflag)
{
	struct proc   *p      = curproc;
	drm_device_t  *dev    = kdev->si_drv1;

	DRM_DEBUG("pid = %d, device = %p, open_count = %d\n",
		  p->p_pid, dev->device, dev->open_count);
	return 0;
}

/* drm_release is called whenever a process closes /dev/drm*. */

int drm_close(dev_t kdev, int fflag, int devtype, struct proc *p)
{
	drm_device_t  *dev  = kdev->si_drv1;
	drm_file_t    *priv;

	DRM_DEBUG("pid = %d, device = %p, open_count = %d\n",
		  p->p_pid, dev->device, dev->open_count);

	priv = drm_find_file_by_proc(dev, p);
	if (!priv) {
		DRM_DEBUG("can't find authenticator\n");
		return EINVAL;
	}

	if (dev->lock.hw_lock && _DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)
	    && dev->lock.pid == p->p_pid) {
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
	}
	drm_reclaim_buffers(dev, priv->pid);

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
	
	return 0;
}

/* The drm_read and drm_write_string code (especially that which manages
   the circular buffer), is based on Alessandro Rubini's LINUX DEVICE
   DRIVERS (Cambridge: O'Reilly, 1998), pages 111-113. */

ssize_t drm_read(dev_t kdev, struct uio *uio, int ioflag)
{
	drm_device_t  *dev    = kdev->si_drv1;
	int	      left;
	int	      avail;
	int	      send;
	int	      cur;
	int	      error = 0;

	DRM_DEBUG("%p, %p\n", dev->buf_rp, dev->buf_wp);
	
	while (dev->buf_rp == dev->buf_wp) {
		DRM_DEBUG("  sleeping\n");
		if (dev->flags & FASYNC) {
			return EWOULDBLOCK;
		}
		error = tsleep(&dev->buf_rp, PZERO|PCATCH, "drmrd", 0);
		if (error) {
			DRM_DEBUG("  interrupted\n");
			return error;
		}
		DRM_DEBUG("  awake\n");
	}

	left  = (dev->buf_rp + DRM_BSZ - dev->buf_wp) % DRM_BSZ;
	avail = DRM_BSZ - left;
	send  = DRM_MIN(avail, uio->uio_resid);

	while (send) {
		if (dev->buf_wp > dev->buf_rp) {
			cur = DRM_MIN(send, dev->buf_wp - dev->buf_rp);
		} else {
			cur = DRM_MIN(send, dev->buf_end - dev->buf_rp);
		}
		error = uiomove(dev->buf_rp, cur, uio);
		if (error)
			break;
		dev->buf_rp += cur;
		if (dev->buf_rp == dev->buf_end) dev->buf_rp = dev->buf;
		send -= cur;
	}
	
	wakeup(&dev->buf_wp);

	return error;
}

int drm_write_string(drm_device_t *dev, const char *s)
{
	int left   = (dev->buf_rp + DRM_BSZ - dev->buf_wp) % DRM_BSZ;
	int send   = strlen(s);
	int count;

	DRM_DEBUG("%d left, %d to send (%p, %p)\n",
		  left, send, dev->buf_rp, dev->buf_wp);
	
	if (left == 1 || dev->buf_wp != dev->buf_rp) {
		DRM_ERROR("Buffer not empty (%d left, wp = %p, rp = %p)\n",
			  left,
			  dev->buf_wp,
			  dev->buf_rp);
	}

	while (send) {
		if (dev->buf_wp >= dev->buf_rp) {
			count = DRM_MIN(send, dev->buf_end - dev->buf_wp);
			if (count == left) --count; /* Leave a hole */
		} else {
			count = DRM_MIN(send, dev->buf_rp - dev->buf_wp - 1);
		}
		strncpy(dev->buf_wp, s, count);
		dev->buf_wp += count;
		if (dev->buf_wp == dev->buf_end) dev->buf_wp = dev->buf;
		send -= count;
	}

	if (dev->buf_selecting) {
		dev->buf_selecting = 0;
		selwakeup(&dev->buf_sel);
	}
		
	DRM_DEBUG("dev->buf_sigio=%p\n", dev->buf_sigio);
	if (dev->buf_sigio) {
		DRM_DEBUG("dev->buf_sigio->sio_pgid=%d\n", dev->buf_sigio->sio_pgid);
		pgsigio(dev->buf_sigio, SIGIO, 0);
	}

	DRM_DEBUG("waking\n");
	wakeup(&dev->buf_rp);
	return 0;
}

int drm_poll(dev_t kdev, int events, struct proc *p)
{
	drm_device_t  *dev    = kdev->si_drv1;
	int           s;
	int	      revents = 0;

	s = spldrm();
	if (events & (POLLIN | POLLRDNORM)) {
		int left  = (dev->buf_rp + DRM_BSZ - dev->buf_wp) % DRM_BSZ;
		if (left > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &dev->buf_sel);
	}
	splx(s);

	return revents;
}
