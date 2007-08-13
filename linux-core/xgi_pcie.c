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

#include "xgi_drv.h"
#include "xgi_regs.h"
#include "xgi_misc.h"

static int xgi_pcie_lut_init(struct xgi_info * info)
{
	u8 temp = 0;
	int err;
	unsigned i;
	struct drm_scatter_gather request;
	struct drm_sg_mem *sg;
	u32 *lut;


	/* Get current FB aperture size */
	temp = IN3X5B(info->mmio_map, 0x27);
	DRM_INFO("In3x5(0x27): 0x%x \n", temp);

	if (temp & 0x01) {	/* 256MB; Jong 06/05/2006; 0x10000000 */
		info->pcie.base = 256 * 1024 * 1024;
	} else {		/* 128MB; Jong 06/05/2006; 0x08000000 */
		info->pcie.base = 128 * 1024 * 1024;
	}


	DRM_INFO("info->pcie.base: 0x%lx\n", (unsigned long) info->pcie.base);

	/* Get current lookup table page size */
	temp = DRM_READ8(info->mmio_map, 0xB00C);
	if (temp & 0x04) {	/* 8KB */
		info->lutPageSize = 8 * 1024;
	} else {		/* 4KB */
		info->lutPageSize = 4 * 1024;
	}

	DRM_INFO("info->lutPageSize: 0x%x \n", info->lutPageSize);


	request.size = info->pcie.size;
	err = drm_sg_alloc(info->dev, & request);
	if (err) {
		DRM_ERROR("cannot allocate PCIE GART backing store!  "
			  "size = %d\n", info->pcie.size);
		return err;
	}

	sg = info->dev->sg;

	info->lut_handle = drm_pci_alloc(info->dev, 
					 sizeof(u32) * sg->pages,
					 PAGE_SIZE,
					 DMA_31BIT_MASK);
	if (info->lut_handle == NULL) {
		DRM_ERROR("cannot allocate PCIE lut page!\n");
		return -ENOMEM;
	}

	lut = info->lut_handle->vaddr;
	for (i = 0; i < sg->pages; i++) {
		info->dev->sg->busaddr[i] = pci_map_page(info->dev->pdev,
							 sg->pagelist[i],
							 0,
							 PAGE_SIZE,
							 DMA_BIDIRECTIONAL);
		if (dma_mapping_error(info->dev->sg->busaddr[i])) {
			DRM_ERROR("cannot map GART backing store for DMA!\n");
			return info->dev->sg->busaddr[i];
		}

		lut[i] = info->dev->sg->busaddr[i];
	}

	DRM_MEMORYBARRIER();

	/* Set GART in SFB */
	temp = DRM_READ8(info->mmio_map, 0xB00C);
	DRM_WRITE8(info->mmio_map, 0xB00C, temp & ~0x02);

	/* Set GART base address to HW */
	DRM_WRITE32(info->mmio_map, 0xB034, info->lut_handle->busaddr);

	/* Flush GART table. */
	DRM_WRITE8(info->mmio_map, 0xB03F, 0x40);
	DRM_WRITE8(info->mmio_map, 0xB03F, 0x00);

	return 0;
}

void xgi_pcie_lut_cleanup(struct xgi_info * info)
{
	if (info->lut_handle) {
		drm_pci_free(info->dev, info->lut_handle);
		info->lut_handle = NULL;
	}
}

int xgi_pcie_heap_init(struct xgi_info * info)
{
	int err;

	err = xgi_pcie_lut_init(info);
	if (err) {
		DRM_ERROR("xgi_pcie_lut_init failed\n");
		return err;
	}


	mutex_lock(&info->dev->struct_mutex);
	err = drm_sman_set_range(&info->sman, XGI_MEMLOC_NON_LOCAL,
				 0, info->pcie.size);
	mutex_unlock(&info->dev->struct_mutex);
	if (err) {
		xgi_pcie_lut_cleanup(info);
	}

	info->pcie_heap_initialized = (err == 0);
	return err;
}


/**
 * xgi_find_pcie_virt
 * @address: GE HW address
 *
 * Returns CPU virtual address.  Assumes the CPU VAddr is continuous in not
 * the same block
 */
void *xgi_find_pcie_virt(struct xgi_info * info, u32 address)
{
	const unsigned long offset = address - info->pcie.base;

	return ((u8 *) info->dev->sg->virtual) + offset;
}

/*
    address -- GE hw address
*/
int xgi_test_rwinkernel_ioctl(struct drm_device * dev, void * data,
			      struct drm_file * filp)
{
	struct xgi_info *info = dev->dev_private;
	u32 address = *(u32 *) data;
	u32 *virtaddr = 0;


	DRM_INFO("input GE HW addr is 0x%x\n", address);

	if (address == 0) {
		return -EFAULT;
	}

	virtaddr = (u32 *)xgi_find_pcie_virt(info, address);

	DRM_INFO("convert to CPU virt addr 0x%p\n", virtaddr);

	if (virtaddr != NULL) {
		DRM_INFO("original [virtaddr] = 0x%x\n", *virtaddr);
		*virtaddr = 0x00f00fff;
		DRM_INFO("modified [virtaddr] = 0x%x\n", *virtaddr);
	} else {
		return -EFAULT;
	}

	return 0;
}
