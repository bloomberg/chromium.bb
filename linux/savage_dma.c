/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


/*=========================================================*/
#include "savage.h"
#include "drmP.h"
#include "savage_drm.h"
#include "savage_drv.h"

#include <linux/interrupt.h>	/* For task queue support */
#include <linux/delay.h>

#define SAVAGE_DEFAULT_USEC_TIMEOUT	10000
#define SAVAGE_FREELIST_DEBUG		0

static int savage_preinit( drm_device_t *dev, unsigned long chipset )
{
	drm_savage_private_t *dev_priv;
	unsigned mmioBase, fbBase, fbSize, apertureBase;
	int ret = 0;

	dev_priv = DRM(alloc)( sizeof(drm_savage_private_t), DRM_MEM_DRIVER );
	if ( dev_priv == NULL )
		return DRM_ERR(ENOMEM);

	memset( dev_priv, 0, sizeof(drm_savage_private_t) );
	dev->dev_private = (void *)dev_priv;
	dev_priv->chipset = (enum savage_family)chipset;

	if( S3_SAVAGE3D_SERIES(dev_priv->chipset) ) {
		fbBase = pci_resource_start( dev->pdev, 0 );
		fbSize = SAVAGE_FB_SIZE_S3;
		mmioBase = fbBase + fbSize;
		apertureBase = fbBase + SAVAGE_APERTURE_OFFSET;
	} else if( chipset != S3_SUPERSAVAGE ) {
		mmioBase = pci_resource_start( dev->pdev, 0 );
		fbBase = pci_resource_start( dev->pdev, 1 );
		fbSize = SAVAGE_FB_SIZE_S4;
		apertureBase = fbBase + SAVAGE_APERTURE_OFFSET;
	} else {
		mmioBase = pci_resource_start( dev->pdev, 0 );
		fbBase = pci_resource_start( dev->pdev, 1 );
		fbSize = pci_resource_len( dev->pdev, 1 );
		apertureBase = pci_resource_start( dev->pdev, 2 );
	}

	if( (ret = DRM(initmap)( dev, mmioBase, SAVAGE_MMIO_SIZE,
				 _DRM_REGISTERS, 0 )))
		return ret;

	if( (ret = DRM(initmap)( dev, fbBase, fbSize,
				 _DRM_FRAME_BUFFER, _DRM_WRITE_COMBINING )))
		return ret;

	if( (ret = DRM(initmap)( dev, apertureBase, SAVAGE_APERTURE_SIZE,
				 _DRM_FRAME_BUFFER, _DRM_WRITE_COMBINING )))
		return ret;

	return ret;
}

void DRM(driver_register_fns)(drm_device_t *dev)
{
	dev->driver_features = DRIVER_USE_AGP | DRIVER_USE_MTRR;
	dev->fn_tbl.preinit = savage_preinit;
}
