/*
 * Copyright 2008 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author: Dave Airlie
 */
#include "drmP.h"
#include "drm.h"

#include "radeon_drm.h"
#include "radeon_drv.h"

static int radeon_gem_ib_init(struct drm_device *dev);
static int radeon_gem_ib_destroy(struct drm_device *dev);

int radeon_gem_init_object(struct drm_gem_object *obj)
{
	struct drm_radeon_gem_object *obj_priv;

	obj_priv = drm_calloc(1, sizeof(*obj_priv), DRM_MEM_DRIVER);
	if (!obj_priv) {
		return -ENOMEM;
	}

	obj->driver_private = obj_priv;
	obj_priv->obj = obj;
	
	return 0;
}

void radeon_gem_free_object(struct drm_gem_object *obj)
{

	struct drm_radeon_gem_object *obj_priv = obj->driver_private;

	/* tear down the buffer object - gem holds struct mutex */
	drm_bo_takedown_vm_locked(obj_priv->bo);
	drm_bo_usage_deref_locked(&obj_priv->bo);
	drm_free(obj->driver_private, 1, DRM_MEM_DRIVER);
}

int radeon_gem_info_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_radeon_gem_info *args = data;

	args->vram_start = dev_priv->mm.vram_offset;
	args->vram_size = dev_priv->mm.vram_size;
	args->vram_visible = dev_priv->mm.vram_visible;

	args->gart_start = dev_priv->mm.gart_start;
	args->gart_size = dev_priv->mm.gart_size;

	return 0;
}

struct drm_gem_object *radeon_gem_object_alloc(struct drm_device *dev, int size, int alignment,
					       int initial_domain)
{
	struct drm_gem_object *obj;
	struct drm_radeon_gem_object *obj_priv;
	int ret;
	uint32_t flags;

	DRM_DEBUG("size 0x%x, alignment %d, initial_domain %d\n", size, alignment, initial_domain);
	obj = drm_gem_object_alloc(dev, size);
	if (!obj)
		return NULL;;

	obj_priv = obj->driver_private;
	if (initial_domain == RADEON_GEM_DOMAIN_VRAM)
		flags = DRM_BO_FLAG_MEM_VRAM | DRM_BO_FLAG_MAPPABLE;
	else
		flags = DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_MAPPABLE;

	flags |= DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE | DRM_BO_FLAG_EXE;
	/* create a TTM BO */
	ret = drm_buffer_object_create(dev,
				       size, drm_bo_type_device,
				       flags, 0, alignment,
				       0, &obj_priv->bo);
	if (ret)
		goto fail;

	return obj;
fail:

	return NULL;
}

int radeon_gem_create_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	struct drm_radeon_gem_create *args = data;
	struct drm_radeon_gem_object *obj_priv;
	struct drm_gem_object *obj;
	int ret = 0;
	uint32_t flags;
	int handle;

	/* create a gem object to contain this object in */
	args->size = roundup(args->size, PAGE_SIZE);

	obj = radeon_gem_object_alloc(dev, args->size, args->alignment, args->initial_domain);
	if (!obj)
		return -EINVAL;

	obj_priv = obj->driver_private;
	DRM_DEBUG("obj is %p bo is %p, %d\n", obj, obj_priv->bo, obj_priv->bo->num_pages);
	ret = drm_gem_handle_create(file_priv, obj, &handle);
	mutex_lock(&dev->struct_mutex);
	drm_gem_object_handle_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	if (ret)
		goto fail;

	args->handle = handle;

	return 0;
fail:
	drm_gem_object_unreference(obj);

	return ret;
}

int radeon_gem_set_domain_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	/* transition the BO to a domain - just validate the BO into a certain domain */
	struct drm_radeon_gem_set_domain *args = data;
	struct drm_gem_object *obj;
	struct drm_radeon_gem_object *obj_priv;
	int ret;
	/* for now if someone requests domain CPU - just make sure the buffer is finished with */

	/* just do a BO wait for now */
	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EINVAL;

	obj_priv = obj->driver_private;

	mutex_lock(&obj_priv->bo->mutex);
	ret = drm_bo_wait(obj_priv->bo, 0, 1, 0, 0);
	mutex_unlock(&obj_priv->bo->mutex);

	mutex_lock(&dev->struct_mutex);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int radeon_gem_pread_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	return -ENOSYS;
}

int radeon_gem_pwrite_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	return -ENOSYS;
}

int radeon_gem_mmap_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_radeon_gem_mmap *args = data;
	struct drm_gem_object *obj;
	struct drm_radeon_gem_object *obj_priv;
	loff_t offset;
	unsigned long addr;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EINVAL;

	offset = args->offset;

	DRM_DEBUG("got here %p\n", obj);
	obj_priv = obj->driver_private;

	DRM_DEBUG("got here %p %p %lld %ld\n", obj, obj_priv->bo, args->size, obj_priv->bo->num_pages);
	if (!obj_priv->bo) {
		mutex_lock(&dev->struct_mutex);
		drm_gem_object_unreference(obj);
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	down_write(&current->mm->mmap_sem);
	addr = do_mmap_pgoff(file_priv->filp, 0, args->size,
			     PROT_READ | PROT_WRITE, MAP_SHARED,
			     obj_priv->bo->map_list.hash.key);
	up_write(&current->mm->mmap_sem);

	DRM_DEBUG("got here %p\n", obj);
	mutex_lock(&dev->struct_mutex);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
	if (IS_ERR((void *)addr))
		return addr;

	args->addr_ptr = (uint64_t) addr;

	return 0;
	
}

int radeon_gem_pin_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_radeon_gem_pin *args = data;
	struct drm_gem_object *obj;
	struct drm_radeon_gem_object *obj_priv;
	int ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EINVAL;

	obj_priv = obj->driver_private;

	DRM_DEBUG("got here %p %p %d\n", obj, obj_priv->bo, atomic_read(&obj_priv->bo->usage));
	/* validate into a pin with no fence */

	if (!(obj_priv->bo->type != drm_bo_type_kernel && !DRM_SUSER(DRM_CURPROC))) {
	  ret = drm_bo_do_validate(obj_priv->bo, 0, DRM_BO_FLAG_NO_EVICT,
				   DRM_BO_HINT_DONT_FENCE, 0);
	} else
	  ret = 0;

	args->offset = obj_priv->bo->offset;
	DRM_DEBUG("got here %p %p\n", obj, obj_priv->bo);

	mutex_lock(&dev->struct_mutex);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int radeon_gem_unpin_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	struct drm_radeon_gem_unpin *args = data;
	struct drm_gem_object *obj;
	struct drm_radeon_gem_object *obj_priv;
	int ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EINVAL;

	obj_priv = obj->driver_private;

	/* validate into a pin with no fence */

	ret = drm_bo_do_validate(obj_priv->bo, DRM_BO_FLAG_NO_EVICT, DRM_BO_FLAG_NO_EVICT,
				 DRM_BO_HINT_DONT_FENCE, 0);

	mutex_lock(&dev->struct_mutex);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int radeon_gem_busy(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	return 0;
}

int radeon_gem_execbuffer(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	return -ENOSYS;


}

int radeon_gem_indirect_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_radeon_gem_indirect *args = data;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_gem_object *obj;
	struct drm_radeon_gem_object *obj_priv;
	uint32_t start, end;
	int ret;
	RING_LOCALS;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (obj == NULL)
		return -EINVAL;

	obj_priv = obj->driver_private;

	DRM_DEBUG("got here %p %d\n", obj, args->used);
	//RING_SPACE_TEST_WITH_RETURN(dev_priv);
	//VB_AGE_TEST_WITH_RETURN(dev_priv);

	ret = drm_bo_do_validate(obj_priv->bo, 0, DRM_BO_FLAG_NO_EVICT,
				 0 , 0);
	if (ret)
		return ret;

	/* Wait for the 3D stream to idle before the indirect buffer
	 * containing 2D acceleration commands is processed.
	 */
	BEGIN_RING(2);

	RADEON_WAIT_UNTIL_3D_IDLE();

	ADVANCE_RING();
	
	start = 0;
	end = args->used;

	if (start != end) {
		int offset = (dev_priv->gart_vm_start + 
			      + obj_priv->bo->offset + start);
		int dwords = (end - start + 3) / sizeof(u32);

#if 0
		/* Indirect buffer data must be an even number of
		 * dwords, so if we've been given an odd number we must
		 * pad the data with a Type-2 CP packet.
		 */
		if (dwords & 1) {
			u32 *data = (u32 *)
			    ((char *)dev->agp_buffer_map->handle
			     + buf->offset + start);
			data[dwords++] = RADEON_CP_PACKET2;
		}
#endif
		/* Fire off the indirect buffer */
		BEGIN_RING(3);

		OUT_RING(CP_PACKET0(RADEON_CP_IB_BASE, 1));
		OUT_RING(offset);
		OUT_RING(dwords);

		ADVANCE_RING();
	}

	COMMIT_RING();

	/* we need to fence the buffer */
	ret = drm_fence_buffer_objects(dev, NULL, 0, NULL, &obj_priv->fence);
	if (ret) {
	  
		drm_putback_buffer_objects(dev);
		ret = 0;
		goto fail;
	}

	/* dereference he fence object */
	drm_fence_usage_deref_unlocked(&obj_priv->fence);

	mutex_lock(&dev->struct_mutex);
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
	ret = 0;
 fail:
	return ret;
}

/*
 * Depending on card genertation, chipset bugs, etc... the amount of vram
 * accessible to the CPU can vary. This function is our best shot at figuring
 * it out. Returns a value in KB.
 */
static uint32_t radeon_get_accessible_vram(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t aper_size;
	u8 byte;

	if (dev_priv->chip_family >= CHIP_R600)
		aper_size = RADEON_READ(R600_CONFIG_APER_SIZE) / 1024;
	else
		aper_size = RADEON_READ(RADEON_CONFIG_APER_SIZE) / 1024;

	/* Set HDP_APER_CNTL only on cards that are known not to be broken,
	 * that is has the 2nd generation multifunction PCI interface
	 */
	if (dev_priv->chip_family == CHIP_RV280 ||
	    dev_priv->chip_family == CHIP_RV350 ||
	    dev_priv->chip_family == CHIP_RV380 ||
	    dev_priv->chip_family == CHIP_R420 ||
	    dev_priv->chip_family == CHIP_RV410 ||
	    dev_priv->chip_family >= CHIP_RS600) {
		uint32_t temp = RADEON_READ(RADEON_HOST_PATH_CNTL);
		temp |= RADEON_HDP_APER_CNTL;
		RADEON_WRITE(RADEON_HOST_PATH_CNTL, temp);
		return aper_size * 2;
	}
	
	/* Older cards have all sorts of funny issues to deal with. First
	 * check if it's a multifunction card by reading the PCI config
	 * header type... Limit those to one aperture size
	 */
	pci_read_config_byte(dev->pdev, 0xe, &byte);
	if (byte & 0x80)
		return aper_size;
	
	/* Single function older card. We read HDP_APER_CNTL to see how the BIOS
	 * have set it up. We don't write this as it's broken on some ASICs but
	 * we expect the BIOS to have done the right thing (might be too optimistic...)
	 */
	if (RADEON_READ(RADEON_HOST_PATH_CNTL) & RADEON_HDP_APER_CNTL)
		return aper_size * 2;

	return aper_size;
}	

/* code from the DDX - do memory sizing */
void radeon_vram_setup(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t vram;
	uint32_t accessible,  bar_size;

	if ((dev_priv->chip_family <= CHIP_RV515) && (dev_priv->flags & RADEON_IS_IGP)) {
		uint32_t tom = RADEON_READ(RADEON_NB_TOM);

		vram = (((tom >> 16) - (tom & 0xffff) + 1) << 6);
		RADEON_WRITE(RADEON_CONFIG_MEMSIZE, vram * 1024);
	} else {
		if (dev_priv->chip_family >= CHIP_R600)
			vram = RADEON_READ(R600_CONFIG_MEMSIZE) / 1024;
		else {
			vram = RADEON_READ(RADEON_CONFIG_MEMSIZE) / 1024;

			/* Some production boards of m6 will return 0 if it's 8 MB */
			if (vram == 0) {
				vram = 8192;
				RADEON_WRITE(RADEON_CONFIG_MEMSIZE, 0x800000);
			}
		}
	}

	accessible = radeon_get_accessible_vram(dev);

	bar_size = drm_get_resource_len(dev, 0) / 1024;
	if (bar_size == 0)
		bar_size = 0x20000;
	if (accessible > bar_size)
		accessible = bar_size;

	DRM_INFO("Detected VRAM RAM=%dK, accessible=%uK, BAR=%uK\n",
		 vram, accessible, bar_size);

	dev_priv->mm.vram_offset = dev_priv->fb_aper_offset;
	dev_priv->mm.vram_size = vram * 1024;
	dev_priv->mm.vram_visible = accessible * 1024;


}

static int radeon_gart_init(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int ret;
	u32 base = 0;

	/* setup a 32MB GART */
	dev_priv->gart_size = dev_priv->mm.gart_size;

#if __OS_HAS_AGP
	/* setup VRAM vs GART here */
	if (dev_priv->flags & RADEON_IS_AGP) {
		base = dev->agp->base;
		if ((base + dev_priv->gart_size - 1) >= dev_priv->fb_location &&
		    base < (dev_priv->fb_location + dev_priv->fb_size - 1)) {
			DRM_INFO("Can't use agp base @0x%08xlx, won't fit\n",
				 dev->agp->base);
			base = 0;
		}
	}
#endif

	if (base == 0) {
		base = dev_priv->fb_location + dev_priv->fb_size;
		if (base < dev_priv->fb_location ||
		    ((base + dev_priv->gart_size) & 0xfffffffful) < base)
			base = dev_priv->fb_location
				- dev_priv->gart_size;
	}
	/* start on the card */
	dev_priv->gart_vm_start = base & 0xffc00000u;
	if (dev_priv->gart_vm_start != base)
		DRM_INFO("GART aligned down from 0x%08x to 0x%08x\n",
			 base, dev_priv->gart_vm_start);

	/* if on PCIE we need to allocate an fb object for the PCIE GART table */
	if (dev_priv->flags & RADEON_IS_PCIE) {
		ret = drm_buffer_object_create(dev, RADEON_PCIGART_TABLE_SIZE,
					       drm_bo_type_kernel,
					       DRM_BO_FLAG_READ | DRM_BO_FLAG_MEM_VRAM | DRM_BO_FLAG_MAPPABLE | DRM_BO_FLAG_NO_EVICT,
					       0, 1, 0, &dev_priv->mm.pcie_table.bo);
		if (ret)
			return -EINVAL;

		DRM_DEBUG("pcie table bo created %p, %x\n", dev_priv->mm.pcie_table.bo, dev_priv->mm.pcie_table.bo->offset);
		ret = drm_bo_kmap(dev_priv->mm.pcie_table.bo, 0, RADEON_PCIGART_TABLE_SIZE >> PAGE_SHIFT,
				  &dev_priv->mm.pcie_table.kmap);
		if (ret)
			return -EINVAL;

		dev_priv->pcigart_offset_set = 2;
		dev_priv->gart_info.bus_addr =  dev_priv->fb_location + dev_priv->mm.pcie_table.bo->offset;
		dev_priv->gart_info.addr = dev_priv->mm.pcie_table.kmap.virtual;
		dev_priv->gart_info.gart_reg_if = DRM_ATI_GART_PCIE;
		dev_priv->gart_info.gart_table_location = DRM_ATI_GART_FB;
		memset(dev_priv->gart_info.addr, 0, RADEON_PCIGART_TABLE_SIZE);
	} else if (!(dev_priv->flags & RADEON_IS_AGP)) {
		/* allocate PCI GART table */
		dev_priv->gart_info.table_mask = DMA_BIT_MASK(32);
		ret = drm_ati_alloc_pcigart_table(dev, &dev_priv->gart_info);
		if (ret) {
			DRM_ERROR("cannot allocate PCI GART page!\n");
			return -EINVAL;
		}

		dev_priv->gart_info.gart_table_location = DRM_ATI_GART_MAIN;
		if (dev_priv->flags & RADEON_IS_IGPGART)
			dev_priv->gart_info.gart_reg_if = DRM_ATI_GART_IGP;
		else
			dev_priv->gart_info.gart_reg_if = DRM_ATI_GART_PCI;
		dev_priv->gart_info.addr = NULL;
		dev_priv->gart_info.bus_addr = 0;
	}
	
	/* gart values setup - start the GART */
	if (dev_priv->flags & RADEON_IS_AGP) {
		radeon_set_pcigart(dev_priv, 0);
	} else {
		radeon_set_pcigart(dev_priv, 1);
	}
		
	return 0;
}

int radeon_alloc_gart_objects(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int ret;

	ret = drm_buffer_object_create(dev, RADEON_DEFAULT_RING_SIZE,
				       drm_bo_type_kernel,
				       DRM_BO_FLAG_READ | DRM_BO_FLAG_MEM_TT |
				       DRM_BO_FLAG_MAPPABLE | DRM_BO_FLAG_NO_EVICT,
				       0, 1, 0, &dev_priv->mm.ring.bo);
	if (ret) {
		DRM_ERROR("failed to allocate ring\n");
		return -EINVAL;
	}

	ret = drm_bo_kmap(dev_priv->mm.ring.bo, 0, RADEON_DEFAULT_RING_SIZE >> PAGE_SHIFT,
			  &dev_priv->mm.ring.kmap);
	if (ret) {
		DRM_ERROR("failed to map ring\n");
		return -EINVAL;
	}

	ret = drm_buffer_object_create(dev, PAGE_SIZE,
				       drm_bo_type_kernel,
				       DRM_BO_FLAG_WRITE |DRM_BO_FLAG_READ | DRM_BO_FLAG_MEM_TT |
				       DRM_BO_FLAG_MAPPABLE | DRM_BO_FLAG_NO_EVICT,
				       0, 1, 0, &dev_priv->mm.ring_read.bo);
	if (ret) {
		DRM_ERROR("failed to allocate ring read\n");
		return -EINVAL;
	}

	ret = drm_bo_kmap(dev_priv->mm.ring_read.bo, 0,
			  PAGE_SIZE >> PAGE_SHIFT,
			  &dev_priv->mm.ring_read.kmap);
	if (ret) {
		DRM_ERROR("failed to map ring read\n");
		return -EINVAL;
	}

	DRM_DEBUG("Ring ptr %p mapped at %d %p, read ptr %p maped at %d %p\n",
		  dev_priv->mm.ring.bo, dev_priv->mm.ring.bo->offset, dev_priv->mm.ring.kmap.virtual,
		  dev_priv->mm.ring_read.bo, dev_priv->mm.ring_read.bo->offset, dev_priv->mm.ring_read.kmap.virtual);

	/* init the indirect buffers */
	radeon_gem_ib_init(dev);
	return 0;			  

}

/* init memory manager - start with all of VRAM and a 32MB GART aperture for now */
int radeon_gem_mm_init(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int ret;

	/* size the mappable VRAM memory for now */
	radeon_vram_setup(dev);
	
	drm_bo_init_mm(dev, DRM_BO_MEM_VRAM, 0, /*dev_priv->mm.vram_offset >> PAGE_SHIFT,*/
		       (dev_priv->mm.vram_visible) >> PAGE_SHIFT,
		       0);


	dev_priv->mm.gart_size = (32 * 1024 * 1024);
	dev_priv->mm.gart_start = 0;
	ret = radeon_gart_init(dev);
	if (ret)
		return -EINVAL;
	
	drm_bo_init_mm(dev, DRM_BO_MEM_TT, 0,
		       dev_priv->mm.gart_size >> PAGE_SHIFT,
		       0);

	/* need to allocate some objects in the GART */
	/* ring + ring read ptr */
	ret = radeon_alloc_gart_objects(dev);
	if (ret)
		return -EINVAL;

	
	return 0;
}

void radeon_gem_mm_fini(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	radeon_gem_ib_destroy(dev);

	mutex_lock(&dev->struct_mutex);

	
	if (dev_priv->mm.ring_read.bo) {
		drm_bo_kunmap(&dev_priv->mm.ring_read.kmap);
		drm_bo_usage_deref_locked(&dev_priv->mm.ring_read.bo);
	}

	if (dev_priv->mm.ring.bo) {
		drm_bo_kunmap(&dev_priv->mm.ring.kmap);
		drm_bo_usage_deref_locked(&dev_priv->mm.ring.bo);
	}

	if (drm_bo_clean_mm(dev, DRM_BO_MEM_TT, 1)) {
		DRM_DEBUG("delaying takedown of TTM memory\n");
	}

	if (dev_priv->flags & RADEON_IS_PCIE) {
		if (dev_priv->mm.pcie_table.bo) {
			drm_bo_kunmap(&dev_priv->mm.pcie_table.kmap);
			drm_bo_usage_deref_locked(&dev_priv->mm.pcie_table.bo);
		}
	}

	if (drm_bo_clean_mm(dev, DRM_BO_MEM_VRAM, 1)) {
		DRM_DEBUG("delaying takedown of TTM memory\n");
	}

	mutex_unlock(&dev->struct_mutex);
}

int radeon_gem_object_pin(struct drm_gem_object *obj,
			  uint32_t alignment)
{
	struct drm_radeon_gem_object *obj_priv;
	int ret;

	obj_priv = obj->driver_private;

	ret = drm_bo_do_validate(obj_priv->bo, 0, DRM_BO_FLAG_NO_EVICT,
				 DRM_BO_HINT_DONT_FENCE, 0);

	return ret;
}

#define RADEON_IB_MEMORY (1*1024*1024)
#define RADEON_IB_SIZE (65536)

#define RADEON_NUM_IB (RADEON_IB_MEMORY / RADEON_IB_SIZE)

int radeon_gem_ib_get(struct drm_device *dev, void **ib, uint32_t dwords, uint32_t *card_offset)
{
	int i, index = -1;
	int ret;
	drm_radeon_private_t *dev_priv = dev->dev_private;

	for (i = 0; i < RADEON_NUM_IB; i++) {
		if (!(dev_priv->ib_alloc_bitmap & (1 << i))){
			index = i;
			break;
		}
	}

	/* if all in use we need to wait */
	if (index == -1) {
		for (i = 0; i < RADEON_NUM_IB; i++) {
			if (dev_priv->ib_alloc_bitmap & (1 << i)) {
				mutex_lock(&dev_priv->ib_objs[i]->bo->mutex);
				ret = drm_bo_wait(dev_priv->ib_objs[i]->bo, 0, 1, 0, 0);
				mutex_unlock(&dev_priv->ib_objs[i]->bo->mutex);
				if (ret)
					continue;
				dev_priv->ib_alloc_bitmap &= ~(1 << i);
				index = i;
				break;
			}
		}
	}

	if (index == -1) {
		DRM_ERROR("Major case fail to allocate IB from freelist %x\n", dev_priv->ib_alloc_bitmap);
		return -EINVAL;
	}
		

	if (dwords > RADEON_IB_SIZE / sizeof(uint32_t))
		return -EINVAL;

	ret = drm_bo_do_validate(dev_priv->ib_objs[index]->bo, 0,
				 DRM_BO_FLAG_NO_EVICT,
				 0, 0);
	if (ret) {
		DRM_ERROR("Failed to validate IB %d\n", index);
		return -EINVAL;
	}
		
	*card_offset = dev_priv->gart_vm_start + dev_priv->ib_objs[index]->bo->offset;
	*ib = dev_priv->ib_objs[index]->kmap.virtual;
	dev_priv->ib_alloc_bitmap |= (1 << i);
	return 0;
}

static void radeon_gem_ib_free(struct drm_device *dev, void *ib, uint32_t dwords)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_fence_object *fence;
	int ret;
	int i;

	for (i = 0; i < RADEON_NUM_IB; i++) {

		if (dev_priv->ib_objs[i]->kmap.virtual == ib) {
			/* emit a fence object */
			ret = drm_fence_buffer_objects(dev, NULL, 0, NULL, &fence);
			if (ret) {
				
				drm_putback_buffer_objects(dev);
			}
			/* dereference the fence object */
			if (fence)
				drm_fence_usage_deref_unlocked(&fence);
		}
	}

}

static int radeon_gem_ib_destroy(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int i;

	if (dev_priv->ib_objs) {
		for (i = 0; i < RADEON_NUM_IB; i++) {
			if (dev_priv->ib_objs[i]) {
				drm_bo_kunmap(&dev_priv->ib_objs[i]->kmap);
				drm_bo_usage_deref_unlocked(&dev_priv->ib_objs[i]->bo);
			}
			drm_free(dev_priv->ib_objs[i], sizeof(struct radeon_mm_obj), DRM_MEM_DRIVER);
		}
		drm_free(dev_priv->ib_objs, RADEON_NUM_IB*sizeof(struct radeon_mm_obj *), DRM_MEM_DRIVER);
	}
	dev_priv->ib_objs = NULL;
	return 0;
}

static int radeon_gem_relocate(struct drm_device *dev, struct drm_file *file_priv,
				uint32_t *reloc, uint32_t *offset)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	/* relocate the handle */
	int domains = reloc[2];
	struct drm_gem_object *obj;
	int flags = 0;
	int ret;
	struct drm_radeon_gem_object *obj_priv;

	obj = drm_gem_object_lookup(dev, file_priv, reloc[1]);
	if (!obj)
		return false;

	obj_priv = obj->driver_private;
	if (domains == RADEON_GEM_DOMAIN_VRAM) {
		flags = DRM_BO_FLAG_MEM_VRAM;
	} else {
		flags = DRM_BO_FLAG_MEM_TT;
	}

	ret = drm_bo_do_validate(obj_priv->bo, flags, DRM_BO_MASK_MEM, 0, 0);
	if (ret)
		return ret;

	if (flags == DRM_BO_FLAG_MEM_VRAM)
		*offset = obj_priv->bo->offset + dev_priv->fb_location;
	else
		*offset = obj_priv->bo->offset + dev_priv->gart_vm_start;

	/* BAD BAD BAD - LINKED LIST THE OBJS and UNREF ONCE IB is SUBMITTED */
	drm_gem_object_unreference(obj);
	return 0;
}

/* allocate 1MB of 64k IBs the the kernel can keep mapped */
static int radeon_gem_ib_init(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int i;
	int ret;

	dev_priv->ib_objs = drm_calloc(RADEON_NUM_IB, sizeof(struct radeon_mm_obj *), DRM_MEM_DRIVER);
	if (!dev_priv->ib_objs)
		goto free_all;

	for (i = 0; i  < RADEON_NUM_IB; i++) {
		dev_priv->ib_objs[i] = drm_calloc(1, sizeof(struct radeon_mm_obj), DRM_MEM_DRIVER);
		if (!dev_priv->ib_objs[i])
			goto free_all;

		ret = drm_buffer_object_create(dev, RADEON_IB_SIZE,
					       drm_bo_type_kernel,
					       DRM_BO_FLAG_READ | DRM_BO_FLAG_MEM_TT |
					       DRM_BO_FLAG_MAPPABLE, 0,
					       0, 0, &dev_priv->ib_objs[i]->bo);
		if (ret)
			goto free_all;

		ret = drm_bo_kmap(dev_priv->ib_objs[i]->bo, 0, RADEON_IB_SIZE >> PAGE_SHIFT,
				  &dev_priv->ib_objs[i]->kmap);

		if (ret)
			goto free_all;
	}

	dev_priv->ib_alloc_bitmap = 0;

	dev_priv->cs.ib_get = radeon_gem_ib_get;
	dev_priv->cs.ib_free = radeon_gem_ib_free;

	radeon_cs_init(dev);
	dev_priv->cs.relocate = radeon_gem_relocate;
	return 0;

free_all:
	radeon_gem_ib_destroy(dev);
	return -ENOMEM;
}

