/* ioctl.c -- IOCTL processing for DRM -*- c -*-
 * Created: Fri Jan  8 09:01:26 1999 by faith@precisioninsight.com
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
#include <sys/bus.h>
#include <pci/pcivar.h>

int
drm_irq_busid(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	drm_irq_busid_t id;
	devclass_t pci;
	device_t bus, dev;
	device_t *kids;
	int error, i, num_kids;

	id = *(drm_irq_busid_t *) data;
	pci = devclass_find("pci");
	if (!pci)
		return ENOENT;
	bus = devclass_get_device(pci, id.busnum);
	if (!bus)
		return ENOENT;
	error = device_get_children(bus, &kids, &num_kids);
	if (error)
		return error;

	dev = 0;
	for (i = 0; i < num_kids; i++) {
		dev = kids[i];
		if (pci_get_slot(dev) == id.devnum
		    && pci_get_function(dev) == id.funcnum)
			break;
	}

	free(kids, M_TEMP);

	if (i != num_kids)
		id.irq = pci_get_irq(dev);
	else
		id.irq = 0;

	DRM_DEBUG("%d:%d:%d => IRQ %d\n",
		  id.busnum, id.devnum, id.funcnum, id.irq);
	*(drm_irq_busid_t *) data = id;

	return 0;
}

int
drm_getunique(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_unique_t	 u;
	int		 error;

	u = *(drm_unique_t *) data;
	if (u.unique_len >= dev->unique_len) {
		error = copyout(dev->unique, u.unique, dev->unique_len);
		if (error)
			return error;
	}
	u.unique_len = dev->unique_len;
	*(drm_unique_t *) data = u;
	return 0;
}

int
drm_setunique(dev_t kdev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_unique_t	 u;
	int		 error;

	if (dev->unique_len || dev->unique) return EBUSY;

	u = *(drm_unique_t *) data;
	
	dev->unique_len = u.unique_len;
	dev->unique	= drm_alloc(u.unique_len + 1, DRM_MEM_DRIVER);
	error = copyin(u.unique, dev->unique, dev->unique_len);
	if (error)
		return error;
	dev->unique[dev->unique_len] = '\0';

	dev->devname = drm_alloc(strlen(dev->name) + strlen(dev->unique) + 2,
				 DRM_MEM_DRIVER);
	sprintf(dev->devname, "%s@%s", dev->name, dev->unique);

	return 0;
}
