/*
 * Copyright 2004 Jon Smirl <jonsmirl@gmail.com>
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

#include "drm_pciids.h"

int DRM(postinit)( struct drm_device *dev, unsigned long flags )
{
	DRM_INFO( "Initialized %s %d.%d.%d %s on minor %d: %s\n",
		DRIVER_NAME,
		DRIVER_MAJOR,
		DRIVER_MINOR,
		DRIVER_PATCHLEVEL,
		DRIVER_DATE,
		dev->minor,
		pci_pretty_name(pdev)
		);
	return 0;
}


static struct pci_device_id DRM(pciidlist)[] = {
	DRM(PCI_IDS)
};

static struct pci_driver DRM(driver) = {
	.name          = DRIVER_NAME,
	.id_table      = DRM(pciidlist),
	.probe         = drm_probe,
	.remove        = __devexit_p(drm_cleanup_pci),
};

static int __init DRM(init)(void)
{
	return drm_init(&DRM(driver), DRM(pciidlist));
}

static void __exit DRM(exit)(void)
{
	drm_exit(&DRM(driver));
}

#define drm_module_init(x) module_init(x)
#define drm_module_exit(x) module_exit(x)
drm_module_init(DRM(init));
drm_module_exit(DRM(exit));
