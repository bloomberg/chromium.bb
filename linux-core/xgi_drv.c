/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * XGI AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ***************************************************************************/

#include "drmP.h"
#include "drm.h"
#include "xgi_drv.h"
#include "xgi_regs.h"
#include "xgi_misc.h"
#include "xgi_cmdlist.h"

#include "drm_pciids.h"

static struct pci_device_id pciidlist[] = {
	xgi_PCI_IDS
};

static struct drm_fence_driver xgi_fence_driver = {
	.num_classes = 1,
	.wrap_diff = BEGIN_BEGIN_IDENTIFICATION_MASK,
	.flush_diff = BEGIN_BEGIN_IDENTIFICATION_MASK - 1,
	.sequence_mask = BEGIN_BEGIN_IDENTIFICATION_MASK,
	.lazy_capable = 1,
	.emit = xgi_fence_emit_sequence,
	.poke_flush = xgi_poke_flush,
	.has_irq = xgi_fence_has_irq
};

int xgi_bootstrap(struct drm_device *, void *, struct drm_file *);

static struct drm_ioctl_desc xgi_ioctls[] = {
	DRM_IOCTL_DEF(DRM_XGI_BOOTSTRAP, xgi_bootstrap, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_XGI_ALLOC, xgi_alloc_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_XGI_FREE, xgi_free_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_XGI_SUBMIT_CMDLIST, xgi_submit_cmdlist, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_XGI_STATE_CHANGE, xgi_state_change_ioctl, DRM_AUTH|DRM_MASTER),
};

static const int xgi_max_ioctl = DRM_ARRAY_SIZE(xgi_ioctls);

static int probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static int xgi_driver_load(struct drm_device *dev, unsigned long flags);
static int xgi_driver_unload(struct drm_device *dev);
static void xgi_driver_lastclose(struct drm_device * dev);
static void xgi_reclaim_buffers_locked(struct drm_device * dev,
	struct drm_file * filp);
static irqreturn_t xgi_kern_isr(DRM_IRQ_ARGS);


static struct drm_driver driver = {
	.driver_features =
		DRIVER_PCI_DMA | DRIVER_HAVE_DMA | DRIVER_HAVE_IRQ |
		DRIVER_IRQ_SHARED | DRIVER_SG,
	.dev_priv_size = sizeof(struct xgi_info),
	.load = xgi_driver_load,
	.unload = xgi_driver_unload,
	.lastclose = xgi_driver_lastclose,
	.dma_quiescent = NULL,
	.irq_preinstall = NULL,
	.irq_postinstall = NULL,
	.irq_uninstall = NULL,
	.irq_handler = xgi_kern_isr,
	.reclaim_buffers = drm_core_reclaim_buffers,
	.reclaim_buffers_idlelocked = xgi_reclaim_buffers_locked,
	.get_map_ofs = drm_core_get_map_ofs,
	.get_reg_ofs = drm_core_get_reg_ofs,
	.ioctls = xgi_ioctls,
	.dma_ioctl = NULL,

	.fops = {
		.owner = THIS_MODULE,
		.open = drm_open,
		.release = drm_release,
		.ioctl = drm_ioctl,
		.mmap = drm_mmap,
		.poll = drm_poll,
		.fasync = drm_fasync,
#if defined(CONFIG_COMPAT) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
		.compat_ioctl = xgi_compat_ioctl,
#endif
	},

	.pci_driver = {
		.name = DRIVER_NAME,
		.id_table = pciidlist,
		.probe = probe,
		.remove = __devexit_p(drm_cleanup_pci),
	},

	.fence_driver = &xgi_fence_driver,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,

};

static int probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	return drm_get_dev(pdev, ent, &driver);
}


static int __init xgi_init(void)
{
	driver.num_ioctls = xgi_max_ioctl;
	return drm_init(&driver, pciidlist);
}

static void __exit xgi_exit(void)
{
	drm_exit(&driver);
}

module_init(xgi_init);
module_exit(xgi_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");


void xgi_engine_init(struct xgi_info * info)
{
	u8 temp;


	OUT3C5B(info->mmio_map, 0x11, 0x92);

	/* -------> copy from OT2D
	 * PCI Retry Control Register.
	 * disable PCI read retry & enable write retry in mem. (10xx xxxx)b
	 */
	temp = IN3X5B(info->mmio_map, 0x55);
	OUT3X5B(info->mmio_map, 0x55, (temp & 0xbf) | 0x80);

	xgi_enable_ge(info);

	/* Enable linear addressing of the card. */
	temp = IN3X5B(info->mmio_map, 0x21);
	OUT3X5B(info->mmio_map, 0x21, temp | 0x20);

	/* Enable 32-bit internal data path */
	temp = IN3X5B(info->mmio_map, 0x2A);
	OUT3X5B(info->mmio_map, 0x2A, temp | 0x40);

	/* Enable PCI burst write ,disable burst read and enable MMIO. */
	/*
	 * 0x3D4.39 Enable PCI burst write, disable burst read and enable MMIO.
	 * 7 ---- Pixel Data Format 1:  big endian 0:  little endian
	 * 6 5 4 3---- Memory Data with Big Endian Format, BE[3:0]#  with Big Endian Format
	 * 2 ---- PCI Burst Write Enable
	 * 1 ---- PCI Burst Read Enable
	 * 0 ---- MMIO Control
	 */
	temp = IN3X5B(info->mmio_map, 0x39);
	OUT3X5B(info->mmio_map, 0x39, (temp | 0x05) & 0xfd);

	/* enable GEIO decode */
	/* temp = IN3X5B(info->mmio_map, 0x29);
	 * OUT3X5B(info->mmio_map, 0x29, temp | 0x08);
	 */

	/* Enable graphic engine I/O PCI retry function*/
	/* temp = IN3X5B(info->mmio_map, 0x62);
	 * OUT3X5B(info->mmio_map, 0x62, temp | 0x50);
	 */

	/* protect all register except which protected by 3c5.0e.7 */
        /* OUT3C5B(info->mmio_map, 0x11, 0x87); */
}


int xgi_bootstrap(struct drm_device * dev, void * data,
		  struct drm_file * filp)
{
	struct xgi_info *info = dev->dev_private;
	struct xgi_bootstrap * bs = (struct xgi_bootstrap *) data;
	struct drm_map_list *maplist;
	int err;


	DRM_SPININIT(&info->fence_lock, "fence lock");
	info->next_sequence = 0;
	info->complete_sequence = 0;

	if (info->mmio_map == NULL) {
		err = drm_addmap(dev, info->mmio.base, info->mmio.size,
				 _DRM_REGISTERS, _DRM_KERNEL,
				 &info->mmio_map);
		if (err) {
			DRM_ERROR("Unable to map MMIO region: %d\n", err);
			return err;
		}

		xgi_enable_mmio(info);
		xgi_engine_init(info);
	}


	info->fb.size = IN3CFB(info->mmio_map, 0x54) * 8 * 1024 * 1024;

	DRM_INFO("fb   base: 0x%lx, size: 0x%x (probed)\n",
		 (unsigned long) info->fb.base, info->fb.size);


	if ((info->fb.base == 0) || (info->fb.size == 0)) {
		DRM_ERROR("framebuffer appears to be wrong: 0x%lx 0x%x\n",
			  (unsigned long) info->fb.base, info->fb.size);
		return -EINVAL;
	}


	/* Init the resource manager */
	if (!info->fb_heap_initialized) {
		err = xgi_fb_heap_init(info);
		if (err) {
			DRM_ERROR("Unable to initialize FB heap.\n");
			return err;
		}
	}


	info->pcie.size = bs->gart.size;

	/* Init the resource manager */
	if (!info->pcie_heap_initialized) {
		err = xgi_pcie_heap_init(info);
		if (err) {
			DRM_ERROR("Unable to initialize GART heap.\n");
			return err;
		}

		/* Alloc 1M bytes for cmdbuffer which is flush2D batch array */
		err = xgi_cmdlist_initialize(info, 0x100000, filp);
		if (err) {
			DRM_ERROR("xgi_cmdlist_initialize() failed\n");
			return err;
		}
	}


	if (info->pcie_map == NULL) {
		err = drm_addmap(info->dev, 0, info->pcie.size,
				 _DRM_SCATTER_GATHER, _DRM_LOCKED,
				 & info->pcie_map);
		if (err) {
			DRM_ERROR("Could not add map for GART backing "
				  "store.\n");
			return err;
		}
	}


	maplist = drm_find_matching_map(dev, info->pcie_map);
	if (maplist == NULL) {
		DRM_ERROR("Could not find GART backing store map.\n");
		return -EINVAL;
	}

	bs->gart = *info->pcie_map;
	bs->gart.handle = (void *)(unsigned long) maplist->user_token;
	return 0;
}


void xgi_driver_lastclose(struct drm_device * dev)
{
	struct xgi_info * info = dev->dev_private;

	if (info != NULL) {
		if (info->mmio_map != NULL) {
			xgi_cmdlist_cleanup(info);
			xgi_disable_ge(info);
			xgi_disable_mmio(info);
		}

		/* The core DRM lastclose routine will destroy all of our
		 * mappings for us.  NULL out the pointers here so that
		 * xgi_bootstrap can do the right thing.
		 */
		info->pcie_map = NULL;
		info->mmio_map = NULL;
		info->fb_map = NULL;

		if (info->pcie_heap_initialized) {
			drm_ati_pcigart_cleanup(dev, &info->gart_info);
		}

		if (info->fb_heap_initialized
		    || info->pcie_heap_initialized) {
			drm_sman_cleanup(&info->sman);

			info->fb_heap_initialized = FALSE;
			info->pcie_heap_initialized = FALSE;
		}
	}
}


void xgi_reclaim_buffers_locked(struct drm_device * dev,
				struct drm_file * filp)
{
	struct xgi_info * info = dev->dev_private;

	mutex_lock(&info->dev->struct_mutex);
	if (drm_sman_owner_clean(&info->sman, (unsigned long) filp)) {
		mutex_unlock(&info->dev->struct_mutex);
		return;
	}

	if (dev->driver->dma_quiescent) {
		dev->driver->dma_quiescent(dev);
	}

	drm_sman_owner_cleanup(&info->sman, (unsigned long) filp);
	mutex_unlock(&info->dev->struct_mutex);
	return;
}


/*
 * driver receives an interrupt if someone waiting, then hand it off.
 */
irqreturn_t xgi_kern_isr(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *) arg;
	struct xgi_info *info = dev->dev_private;
	const u32 irq_bits = le32_to_cpu(DRM_READ32(info->mmio_map,
					(0x2800 
					 + M2REG_AUTO_LINK_STATUS_ADDRESS)))
		& (M2REG_ACTIVE_TIMER_INTERRUPT_MASK
		   | M2REG_ACTIVE_INTERRUPT_0_MASK
		   | M2REG_ACTIVE_INTERRUPT_2_MASK
		   | M2REG_ACTIVE_INTERRUPT_3_MASK);


	if (irq_bits != 0) {
		DRM_WRITE32(info->mmio_map, 
			    0x2800 + M2REG_AUTO_LINK_SETTING_ADDRESS,
			    cpu_to_le32(M2REG_AUTO_LINK_SETTING_COMMAND | irq_bits));
		xgi_fence_handler(dev);
		return IRQ_HANDLED;
	} else {
		return IRQ_NONE;
	}
}


int xgi_driver_load(struct drm_device *dev, unsigned long flags)
{
	struct xgi_info *info = drm_alloc(sizeof(*info), DRM_MEM_DRIVER);
	int err;

	if (!info)
		return -ENOMEM;

	(void) memset(info, 0, sizeof(*info));
	dev->dev_private = info;
	info->dev = dev;

	info->mmio.base = drm_get_resource_start(dev, 1);
	info->mmio.size = drm_get_resource_len(dev, 1);

	DRM_INFO("mmio base: 0x%lx, size: 0x%x\n",
		 (unsigned long) info->mmio.base, info->mmio.size);


	if ((info->mmio.base == 0) || (info->mmio.size == 0)) {
		DRM_ERROR("mmio appears to be wrong: 0x%lx 0x%x\n",
			  (unsigned long) info->mmio.base, info->mmio.size);
		err = -EINVAL;
		goto fail;
	}


	info->fb.base = drm_get_resource_start(dev, 0);
	info->fb.size = drm_get_resource_len(dev, 0);

	DRM_INFO("fb   base: 0x%lx, size: 0x%x\n",
		 (unsigned long) info->fb.base, info->fb.size);


	err = drm_sman_init(&info->sman, 2, 12, 8);
	if (err) {
		goto fail;
	}


	return 0;
	
fail:
	drm_free(info, sizeof(*info), DRM_MEM_DRIVER);
	return err;
}

int xgi_driver_unload(struct drm_device *dev)
{
	struct xgi_info * info = dev->dev_private;

	drm_sman_takedown(&info->sman);
	drm_free(info, sizeof(*info), DRM_MEM_DRIVER);
	dev->dev_private = NULL;

	return 0;
}
