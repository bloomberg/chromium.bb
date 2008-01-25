/*
 * Copyright 2007 Jerome Glisse.
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
 */
/*
 * Authors:
 *    Jerome Glisse <glisse@freedesktop.org>
 */
#include "drm_pciids.h"
#include "radeon_ms.h"

extern struct drm_fence_driver radeon_ms_fence_driver;
extern struct drm_bo_driver radeon_ms_bo_driver;
extern struct drm_ioctl_desc radeon_ms_ioctls[];
extern int radeon_ms_num_ioctls;

static int radeon_ms_driver_dri_library_name(struct drm_device * dev,
					     char * buf);
static int radeon_ms_driver_probe(struct pci_dev *pdev,
				  const struct pci_device_id *ent);

static struct pci_device_id pciidlist[] = {
	radeon_ms_PCI_IDS
};

static struct drm_driver driver = {
	.load = radeon_ms_driver_load,
	.firstopen = NULL,
	.open = radeon_ms_driver_open,
	.preclose = NULL,
	.postclose = NULL,
	.lastclose = radeon_ms_driver_lastclose,
	.unload = radeon_ms_driver_unload,
	.dma_ioctl = radeon_ms_driver_dma_ioctl,
	.dma_ready = NULL,
	.dma_quiescent = NULL,
	.context_ctor = NULL,
	.context_dtor = NULL,
	.kernel_context_switch = NULL,
	.kernel_context_switch_unlock = NULL,
	.dri_library_name = radeon_ms_driver_dri_library_name,
	.device_is_agp = NULL,
	.irq_handler = radeon_ms_irq_handler,
	.irq_preinstall = radeon_ms_irq_preinstall,
	.irq_postinstall = radeon_ms_irq_postinstall,
	.irq_uninstall = radeon_ms_irq_uninstall,
	.reclaim_buffers = drm_core_reclaim_buffers,
	.reclaim_buffers_locked = NULL,
	.reclaim_buffers_idlelocked = NULL,
	.get_map_ofs = drm_core_get_map_ofs,
	.get_reg_ofs = drm_core_get_reg_ofs,
	.set_version = NULL,
	.fb_probe = radeonfb_probe,
	.fb_remove = radeonfb_remove,
	.fence_driver = &radeon_ms_fence_driver,
	.bo_driver = &radeon_ms_bo_driver,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.driver_features =
	    DRIVER_USE_AGP | DRIVER_USE_MTRR | DRIVER_PCI_DMA | DRIVER_SG |
  	    DRIVER_HAVE_IRQ | DRIVER_HAVE_DMA | DRIVER_IRQ_SHARED,
	.dev_priv_size = 0, 
	.ioctls = radeon_ms_ioctls,
	.num_ioctls = 0,
	.fops = {
		.owner = THIS_MODULE,
		.open = drm_open,
		.release = drm_release,
		.ioctl = drm_ioctl,
		.mmap = drm_mmap,
		.poll = drm_poll,
		.fasync = drm_fasync,
#if defined(CONFIG_COMPAT) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
		.compat_ioctl = radeon_ms_compat_ioctl,
#endif
		},
	.pci_driver = {
		.name = DRIVER_NAME,
		.id_table = pciidlist,
		.probe = radeon_ms_driver_probe,
		.remove = __devexit_p(drm_cleanup_pci),
	},
};

static int radeon_ms_driver_probe(struct pci_dev *pdev,
				  const struct pci_device_id *ent)
{
	return drm_get_dev(pdev, ent, &driver);
}

static int radeon_ms_driver_dri_library_name(struct drm_device * dev,
					     char * buf)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int ret;

	switch (dev_priv->family) {
	default:
		ret = snprintf(buf, PAGE_SIZE, "\n");
	}
	return ret;
}

static void __exit radeon_ms_driver_exit(void)
{
	drm_exit(&driver);
}

static int __init radeon_ms_driver_init(void)
{
	driver.num_ioctls = radeon_ms_num_ioctls;
	return drm_init(&driver, pciidlist);
}

module_init(radeon_ms_driver_init);
module_exit(radeon_ms_driver_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
