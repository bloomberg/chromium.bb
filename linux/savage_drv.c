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

#include <linux/config.h>
#include "savage.h"
#include "drmP.h"
#include "savage_drm.h"
#include "savage_drv.h"

#define DRIVER_AUTHOR		"John Zhao, S3 Graphics Inc."

#define DRIVER_NAME		"savage"
#define DRIVER_DESC		"Savage4 Family"
#define DRIVER_DATE		"20011023"

#define DRIVER_MAJOR		1
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	0

/* Currently Savage4 not implement DMA */
/* mark off by Jiayo Hsu, Oct. 23, 2001*/


#if SAVAGE_CMD_DMA /* Check the 3D driver, and we need to fix this anyway */

#define DRIVER_IOCTLS \
	[DRM_IOCTL_NR(DRM_IOCTL_SAVAGE_ALLOC_CONTINUOUS_MEM)] \
	= {savage_alloc_continuous_mem,1,0},\
	[DRM_IOCTL_NR( DRM_IOCTL_SAVAGE_GET_PHYSICS_ADDRESS)] \
	= {savage_get_physics_address,1,0},\
        [DRM_IOCTL_NR(DRM_IOCTL_SAVAGE_FREE_CONTINUOUS_MEM)]  \
        = {savage_free_cont_mem,1,0}

int savage_alloc_continuous_mem(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
  drm_savage_alloc_cont_mem_t cont_mem;	
  unsigned long size,addr;
  void * ret;
  int i;
  mem_map_t *p;
  pgprot_t flags;

  /* add to list */  
  drm_file_t *priv = filp->private_data;
  drm_device_t *dev = priv->dev;
  drm_map_t *map;
  drm_map_list_t *list;

  if (copy_from_user(&cont_mem,(drm_savage_alloc_cont_mem_t *)arg,sizeof(cont_mem)))
    return -EFAULT;
 
  map = DRM(alloc)( sizeof(*map), DRM_MEM_MAPS );
  if ( !map )
    return -ENOMEM;
  
  /*check the parameters*/
  if (cont_mem.size <= 0 )
    return -EINVAL;
  
  size = cont_mem.type * cont_mem.size;
  printk("[drm]JTLIsize = %lu\n",size);
  
  ret = (void *)__get_free_pages(GFP_KERNEL, get_order(size));
  if (ret == NULL)
    {
      printk("Kalloc error\n");
      return 0;
    }
  
  /*set the reserverd flag so that the remap_page_range can map these page*/
  for(i=0,p=virt_to_page(ret);i< size /PAGE_SIZE; i++,p++)
    SetPageReserved(p);
  
  cont_mem.phyaddress = virt_to_bus(ret);
  cont_mem.location = DRM_SAVAGE_MEM_LOCATION_PCI; /* pci only at present*/
  
  
  /*Map the memory to user space*/
  down_write(&current->mm->mmap_sem);
  addr=do_mmap(NULL,0,size,PROT_READ|PROT_WRITE,MAP_PRIVATE,cont_mem.phyaddress);
  if (addr<=0)
    return -EFAULT;
  pgprot_val(flags)=_PAGE_PRESENT | _PAGE_RW |_PAGE_USER ;
  if (remap_page_range(addr,cont_mem.phyaddress,size,flags))
    return -EFAULT;
  up_write(&current->mm->mmap_sem);
  
  for(i=0,p=virt_to_page(ret);i< size /PAGE_SIZE; i++,p++)
    ClearPageReserved(p);
  
  cont_mem.linear = addr;
  
  /*map list*/
  map->handle =  ret; /* to distinguish with other*/
  map->offset = cont_mem.phyaddress;
  map->size = size;
  map->mtrr=-1;
  /*map-flags,type??*/
  
  list = DRM(alloc)(sizeof(*list), DRM_MEM_MAPS);
  if(!list) {
    DRM(free)(map, sizeof(*map), DRM_MEM_MAPS);
    return -EINVAL;
  }
  memset(list, 0, sizeof(*list));
  list->map = map;
  
  down(&dev->struct_sem);
  list_add(&list->head, &dev->maplist->head);
  up(&dev->struct_sem);
  
  if (copy_to_user((drm_savage_alloc_cont_mem_t *)arg,&cont_mem,sizeof(cont_mem)))
    return -EFAULT;

  for(i=0,p=virt_to_page(ret);i< size /PAGE_SIZE; i++,p++)
    atomic_set(&p->count,1);

  return 1;/*success*/  
}

int savage_get_physics_address(struct inode *inode, struct file *filp,
			       unsigned int cmd, unsigned long arg)
{
  
  drm_savage_get_physcis_address_t req;
  unsigned long buf;
  pgd_t *pgd;
  pmd_t *pmd;
  pte_t *pte;
  struct mm_struct *mm;
  
  if (copy_from_user(&req,(drm_savage_get_physcis_address_t *)arg,sizeof(req)))
    return -EFAULT;
  buf = req.v_address;
  
  /*What kind of virtual address ?*/
  if (buf >= (unsigned long)high_memory)
    mm = &init_mm;
  else
    mm = current->mm;
  
#ifdef LINUX_24
  spin_lock(&mm->page_table_lock);
#endif
  
  pgd=pgd_offset(mm,buf);
  pmd=pmd_offset(pgd,buf);
  pte=pte_offset_map(pmd,buf);
  
  if (!pte_present(*pte))
    return -EFAULT;
  
  req.p_address = ((pte_val(*pte) &PAGE_MASK) |( buf&(PAGE_SIZE-1)));
  
  if (copy_to_user((drm_savage_get_physcis_address_t  *)arg,&req,sizeof(req)))
    return -EFAULT;
  return 1;
}

/*free the continuous memory*/
int savage_free_cont_mem(struct inode *inode, struct file *filp,
			       unsigned int cmd, unsigned long arg)
{
  drm_savage_alloc_cont_mem_t cont_mem;	
  unsigned long size;

  /*map  list */  
  drm_file_t *priv = filp->private_data;
  drm_device_t *dev = priv->dev;
  drm_map_t *map;
  struct list_head *list;
  drm_map_list_t *r_list = NULL;

  if (copy_from_user(&cont_mem,(drm_savage_alloc_cont_mem_t *)arg,sizeof(cont_mem)))
    return -EFAULT;
  size = cont_mem.type * cont_mem.size;
  if (size <= 0)
    return -EINVAL;

  /* find the map in the list */
  list_for_each(list,&dev->maplist->head)
    {
      r_list = (drm_map_list_t *) list;
      
      if(r_list->map &&
	 r_list->map->offset == cont_mem.phyaddress )
	 break;   
    }
  /*find none*/
  if(list == (&dev->maplist->head)) {
    up(&dev->struct_sem);
    return -EINVAL;
  }
  map = r_list->map;
  list_del(list);
  DRM(free)(list, sizeof(*list), DRM_MEM_MAPS);

  /*unmap the user space */
#ifdef DO_MUNMAP_4_ARGS
  if ( do_munmap(current->mm,cont_mem.linear,size,1)!=0)
#else
  if ( do_munmap(current->mm,cont_mem.linear,size)!=0)
#endif
    return -EFAULT;
  /*free the page*/
  free_pages((unsigned long)map->handle, get_order(size));

  return 1;
}
#else /* SAVAGE_CMD_DMA */
#define DRIVER_IOCTLS
#endif /* SAVAGE_CMD_DMA */

#if 0

#define DRIVER_IOCTLS							   \
	[DRM_IOCTL_NR(DRM_IOCTL_DMA)]	      = { mga_dma_buffers, 1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_INIT)]    = { mga_dma_init,    1, 1 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_FLUSH)]   = { mga_dma_flush,   1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_RESET)]   = { mga_dma_reset,   1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_SWAP)]    = { mga_dma_swap,    1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_CLEAR)]   = { mga_dma_clear,   1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_VERTEX)]  = { mga_dma_vertex,  1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_INDICES)] = { mga_dma_indices, 1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_ILOAD)]   = { mga_dma_iload,   1, 0 }, \
	[DRM_IOCTL_NR(DRM_IOCTL_MGA_BLIT)]    = { mga_dma_blit,    1, 0 },


#endif /* end #if 0 */

#include "drm_core.h"


