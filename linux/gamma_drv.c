/* gamma.c -- 3dlabs GMX 2000 driver -*- linux-c -*-
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

#include <linux/config.h>
#include "gamma.h"
#include "drmP.h"
#include "gamma_drv.h"

#define DRIVER_AUTHOR		"VA Linux Systems Inc."

#define DRIVER_NAME		"gamma"
#define DRIVER_DESC		"3DLabs gamma"
#define DRIVER_DATE		"20010215"

#define DRIVER_MAJOR		1
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	0


#define DRIVER_IOCTLS							  \
	[DRM_IOCTL_NR(DRM_IOCTL_DMA)]	     = { gamma_dma,	  1, 0 }

#define __HAVE_MTRR		1
#define __HAVE_CTX_BITMAP	0
#define __HAVE_PCI_DMA		1
#define __HAVE_MULTIPLE_DMA_QUEUES	1
#define __HAVE_DMA_FLUSH		1
#define __HAVE_DMA_QUEUE		0
#define __HAVE_DMA_SCHEDULE		1
#define __HAVE_DMA_WAITQUEUE		1
#define __HAVE_DMA_WAITLIST	1
#define __HAVE_DMA_FREELIST	1
#define __HAVE_DMA		1
#define __HAVE_OLD_DMA		1
#define __HAVE_DMA_IRQ		1

#define __HAVE_COUNTERS		5
#define __HAVE_COUNTER6		_DRM_STAT_IRQ
#define __HAVE_COUNTER7		_DRM_STAT_DMA
#define __HAVE_COUNTER8		_DRM_STAT_PRIMARY
#define __HAVE_COUNTER9		_DRM_STAT_SPECIAL
#define __HAVE_COUNTER10	_DRM_STAT_MISSED

#define __HAVE_DMA_READY	1
#define DRIVER_DMA_READY()						\
do {									\
	gamma_dma_ready(dev);						\
} while (0)

#define __HAVE_DMA_QUIESCENT	1
#define DRIVER_DMA_QUIESCENT()						\
do {									\
	/* FIXME ! */ 							\
	gamma_dma_quiescent_dual(dev);					\
	return 0;							\
} while (0)

#if 0
#define __HAVE_DRIVER_RELEASE	1
#define DRIVER_RELEASE() do {						\
	gamma_reclaim_buffers( dev, priv->pid );			\
	if ( dev->dev_private ) {					\
		drm_gamma_private_t *dev_priv = dev->dev_private;	\
		dev_priv->dispatch_status &= MGA_IN_DISPATCH;		\
	}								\
} while (0)
#endif

#if 0
#define DRIVER_PRETAKEDOWN() do {					\
	if ( dev->dev_private ) gamma_do_cleanup_dma( dev );		\
} while (0)
#endif

#include "drm_drv.h"
