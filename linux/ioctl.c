/* ioctl.c -- IOCTL processing for DRM -*- linux-c -*-
 * Created: Fri Jan  8 09:01:26 1999 by faith@precisioninsight.com
 * Revised: Fri Aug 20 09:27:02 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 * $PI: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/ioctl.c,v 1.3 1999/08/30 13:05:00 faith Exp $
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/ioctl.c,v 1.1 1999/09/25 14:38:01 dawes Exp $
 *
 */

#define __NO_VERSION__
#include "drmP.h"

int drm_irq_busid(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_irq_busid_t p;
	struct pci_dev	*dev;

	copy_from_user_ret(&p, (drm_irq_busid_t *)arg, sizeof(p), -EFAULT);
	dev = pci_find_slot(p.busnum, PCI_DEVFN(p.devnum, p.funcnum));
	if (dev) p.irq = dev->irq;
	else	 p.irq = 0;
	DRM_DEBUG("%d:%d:%d => IRQ %d\n",
		  p.busnum, p.devnum, p.funcnum, p.irq);
	copy_to_user_ret((drm_irq_busid_t *)arg, &p, sizeof(p), -EFAULT);
	return 0;
}

int drm_getunique(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_unique_t	 u;

	copy_from_user_ret(&u, (drm_unique_t *)arg, sizeof(u), -EFAULT);
	if (u.unique_len >= dev->unique_len) {
		copy_to_user_ret(u.unique, dev->unique, dev->unique_len,
				 -EFAULT);
	}
	u.unique_len = dev->unique_len;
	copy_to_user_ret((drm_unique_t *)arg, &u, sizeof(u), -EFAULT);
	return 0;
}

int drm_setunique(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_unique_t	 u;

	if (dev->unique_len || dev->unique) return -EBUSY;

	copy_from_user_ret(&u, (drm_unique_t *)arg, sizeof(u), -EFAULT);
	if (!u.unique_len) return -EINVAL;
	
	dev->unique_len = u.unique_len;
	dev->unique	= drm_alloc(u.unique_len + 1, DRM_MEM_DRIVER);
	copy_from_user_ret(dev->unique, u.unique, dev->unique_len,
			   -EFAULT);
	dev->unique[dev->unique_len] = '\0';

	dev->devname = drm_alloc(strlen(dev->name) + strlen(dev->unique) + 2,
				 DRM_MEM_DRIVER);
	sprintf(dev->devname, "%s@%s", dev->name, dev->unique);

	return 0;
}
