/* auth.c -- IOCTLs for authentication -*- c -*-
 * Created: Tue Feb  2 08:37:54 1999 by faith@precisioninsight.com
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

#define __NO_VERSION__
#include "drmP.h"

static int drm_hash_magic(drm_magic_t magic)
{
	return magic & (DRM_HASH_SIZE-1);
}

static drm_file_t *drm_find_file(drm_device_t *dev, drm_magic_t magic)
{
	drm_file_t	  *retval = NULL;
	drm_magic_entry_t *pt;
	int		  hash	  = drm_hash_magic(magic);

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	for (pt = dev->magiclist[hash].head; pt; pt = pt->next) {
		if (pt->priv->authenticated) continue;
		if (pt->magic == magic) {
			retval = pt->priv;
			break;
		}
	}
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);

	return retval;
}

int drm_add_magic(drm_device_t *dev, drm_file_t *priv, drm_magic_t magic)
{
	int		  hash;
	drm_magic_entry_t *entry;
	
	DRM_DEBUG("%d\n", magic);
	
	hash	     = drm_hash_magic(magic);
	entry	     = drm_alloc(sizeof(*entry), DRM_MEM_MAGIC);
	if (!entry) return ENOMEM;
	entry->magic = magic;
	entry->priv  = priv;
	entry->next  = NULL;

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	if (dev->magiclist[hash].tail) {
		dev->magiclist[hash].tail->next = entry;
		dev->magiclist[hash].tail	= entry;
	} else {
		dev->magiclist[hash].head	= entry;
		dev->magiclist[hash].tail	= entry;
	}
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
	
	return 0;
}

int drm_remove_magic(drm_device_t *dev, drm_magic_t magic)
{
	drm_magic_entry_t *prev = NULL;
	drm_magic_entry_t *pt;
	int		  hash;
	
	DRM_DEBUG("%d\n", magic);
	hash = drm_hash_magic(magic);
	
	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	for (pt = dev->magiclist[hash].head; pt; prev = pt, pt = pt->next) {
		if (pt->magic == magic) {
			if (dev->magiclist[hash].head == pt) {
				dev->magiclist[hash].head = pt->next;
			}
			if (dev->magiclist[hash].tail == pt) {
				dev->magiclist[hash].tail = prev;
			}
			if (prev) {
				prev->next = pt->next;
			}
			lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
			return 0;
		}
	}
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);

	drm_free(pt, sizeof(*pt), DRM_MEM_MAGIC);
	
	return EINVAL;
}

int drm_getmagic(dev_t kdev, u_long cmd, caddr_t data,
		 int flags, struct proc *p)
{
	static drm_magic_t sequence = 0;
#if 0
	static struct simplelock lock; /* XXX */
#endif
	drm_device_t	   *dev = kdev->si_drv1;
	drm_file_t	   *priv;
	drm_auth_t	   auth;

				/* Find unique magic */
	priv = drm_find_file_by_proc(dev, p);
	if (!priv) {
	    DRM_DEBUG("can't find file structure\n");
	    return EINVAL;
	}
	if (priv->magic) {
		auth.magic = priv->magic;
	} else {
		simple_lock(&lock);
		do {
			if (!sequence) ++sequence; /* reserve 0 */
			auth.magic = sequence++;
		} while (drm_find_file(dev, auth.magic));
		simple_unlock(&lock);
		priv->magic = auth.magic;
		drm_add_magic(dev, priv, auth.magic);
	}
	
	DRM_DEBUG("%u\n", auth.magic);
	*(drm_auth_t *) data = auth;
	return 0;
}

int drm_authmagic(dev_t kdev, u_long cmd, caddr_t data,
		  int flags, struct proc *p)
{
	drm_device_t	   *dev	    = kdev->si_drv1;
	drm_auth_t	   auth;
	drm_file_t	   *file;

	auth = *(drm_auth_t *) data;
	DRM_DEBUG("%u\n", auth.magic);
	if ((file = drm_find_file(dev, auth.magic))) {
		file->authenticated = 1;
		drm_remove_magic(dev, auth.magic);
		return 0;
	}
	return EINVAL;
}
