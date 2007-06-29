
/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.			
 *																			*
 * All Rights Reserved.														*
 *																			*
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the	
 * "Software"), to deal in the Software without restriction, including	
 * without limitation on the rights to use, copy, modify, merge,	
 * publish, distribute, sublicense, and/or sell copies of the Software,	
 * and to permit persons to whom the Software is furnished to do so,	
 * subject to the following conditions:					
 *																			*
 * The above copyright notice and this permission notice (including the	
 * next paragraph) shall be included in all copies or substantial	
 * portions of the Software.						
 *																			*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,	
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF	
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND		
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR			
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,		
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,		
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER			
 * DEALINGS IN THE SOFTWARE.												
 ***************************************************************************/

#include "xgi_types.h"
#include "xgi_linux.h"
#include "xgi_drv.h"
#include "xgi_regs.h"
#include "xgi_pcie.h"
#include "xgi_misc.h"

static struct xgi_pcie_heap *xgi_pcie_heap = NULL;
static struct kmem_cache *xgi_pcie_cache_block = NULL;
static struct xgi_pcie_block *xgi_pcie_vertex_block = NULL;
static struct xgi_pcie_block *xgi_pcie_cmdlist_block = NULL;
static struct xgi_pcie_block *xgi_pcie_scratchpad_block = NULL;
extern struct list_head xgi_mempid_list;

static unsigned long xgi_pcie_lut_alloc(unsigned long page_order)
{
	struct page *page;
	unsigned long page_addr = 0;
	unsigned long page_count = 0;
	int i;

	page_count = (1 << page_order);
	page_addr = __get_free_pages(GFP_KERNEL, page_order);

	if (page_addr == 0UL) {
		XGI_ERROR("Can't get free pages: 0x%lx from system memory !\n",
			  page_count);
		return 0;
	}

	page = virt_to_page(page_addr);

	for (i = 0; i < page_count; i++, page++) {
		XGI_INC_PAGE_COUNT(page);
		XGILockPage(page);
	}

	XGI_INFO("page_count: 0x%lx page_order: 0x%lx page_addr: 0x%lx \n",
		 page_count, page_order, page_addr);
	return page_addr;
}

static void xgi_pcie_lut_free(unsigned long page_addr, unsigned long page_order)
{
	struct page *page;
	unsigned long page_count = 0;
	int i;

	page_count = (1 << page_order);
	page = virt_to_page(page_addr);

	for (i = 0; i < page_count; i++, page++) {
		XGI_DEC_PAGE_COUNT(page);
		XGIUnlockPage(page);
	}

	free_pages(page_addr, page_order);
}

static int xgi_pcie_lut_init(struct xgi_info * info)
{
	unsigned char *page_addr = NULL;
	unsigned long pciePageCount, lutEntryNum, lutPageCount, lutPageOrder;
	unsigned long count = 0;
	u8 temp = 0;

	/* Jong 06/06/2006 */
	unsigned long pcie_aperture_size;

	info->pcie.size = 128 * 1024 * 1024;

	/* Get current FB aperture size */
	temp = In3x5(0x27);
	XGI_INFO("In3x5(0x27): 0x%x \n", temp);

	if (temp & 0x01) {	/* 256MB; Jong 06/05/2006; 0x10000000 */
		/* Jong 06/06/2006; allocate memory */
		pcie_aperture_size = 256 * 1024 * 1024;
		/* info->pcie.base = 256 * 1024 * 1024; *//* pcie base is different from fb base */
	} else {		/* 128MB; Jong 06/05/2006; 0x08000000 */

		/* Jong 06/06/2006; allocate memory */
		pcie_aperture_size = 128 * 1024 * 1024;
		/* info->pcie.base = 128 * 1024 * 1024; */
	}

	/* Jong 06/06/2006; allocate memory; it can be used for build-in kernel modules */
	/* info->pcie.base=(unsigned long)alloc_bootmem(pcie_mem_size); */
	/* total 496 MB; need 256 MB (0x10000000); start from 240 MB (0x0F000000) */
	/* info->pcie.base=ioremap(0x0F000000, 0x10000000); *//* Cause system hang */
	info->pcie.base = pcie_aperture_size;	/* works */
	/* info->pcie.base=info->fb.base + info->fb.size; *//* System hang */
	/* info->pcie.base=128 * 1024 * 1024; *//* System hang */

	XGI_INFO("Jong06062006-info->pcie.base: 0x%lx \n", info->pcie.base);

	/* Get current lookup table page size */
	temp = bReadReg(0xB00C);
	if (temp & 0x04) {	/* 8KB */
		info->lutPageSize = 8 * 1024;
	} else {		/* 4KB */

		info->lutPageSize = 4 * 1024;
	}

	XGI_INFO("info->lutPageSize: 0x%lx \n", info->lutPageSize);

#if 0
	/* Get current lookup table location */
	temp = bReadReg(0xB00C);
	if (temp & 0x02) {	/* LFB */
		info->isLUTInLFB = TRUE;
		/* Current we only support lookup table in LFB */
		temp &= 0xFD;
		bWriteReg(0xB00C, temp);
		info->isLUTInLFB = FALSE;
	} else {		/* SFB */

		info->isLUTInLFB = FALSE;
	}

	XGI_INFO("info->lutPageSize: 0x%lx \n", info->lutPageSize);

	/* Get current SDFB page size */
	temp = bReadReg(0xB00C);
	if (temp & 0x08) {	/* 8MB */
		info->sdfbPageSize = 8 * 1024 * 1024;
	} else {		/* 4MB */

		info->sdfbPageSize = 4 * 1024 * 1024;
	}
#endif
	pciePageCount = (info->pcie.size + PAGE_SIZE - 1) / PAGE_SIZE;

	/*
	 * Allocate memory for PCIE GART table;
	 */
	lutEntryNum = pciePageCount;
	lutPageCount = (lutEntryNum * 4 + PAGE_SIZE - 1) / PAGE_SIZE;

	/* get page_order base on page_count */
	count = lutPageCount;
	for (lutPageOrder = 0; count; count >>= 1, ++lutPageOrder) ;

	if ((lutPageCount << 1) == (1 << lutPageOrder)) {
		lutPageOrder -= 1;
	}

	XGI_INFO("lutEntryNum: 0x%lx lutPageCount: 0x%lx lutPageOrder 0x%lx\n",
		 lutEntryNum, lutPageCount, lutPageOrder);

	info->lutPageOrder = lutPageOrder;
	page_addr = (unsigned char *)xgi_pcie_lut_alloc(lutPageOrder);

	if (!page_addr) {
		XGI_ERROR("cannot allocate PCIE lut page!\n");
		goto fail;
	}
	info->lut_base = (unsigned long *)page_addr;

	XGI_INFO("page_addr: 0x%p virt_to_phys(page_virtual): 0x%lx \n",
		 page_addr, virt_to_phys(page_addr));

	XGI_INFO
	    ("info->lut_base: 0x%p __pa(info->lut_base): 0x%lx info->lutPageOrder 0x%lx\n",
	     info->lut_base, __pa(info->lut_base), info->lutPageOrder);

	/*
	 * clean all PCIE GART Entry
	 */
	memset(page_addr, 0, PAGE_SIZE << lutPageOrder);

#if defined(__i386__) || defined(__x86_64__)
	asm volatile ("wbinvd":::"memory");
#else
	mb();
#endif

	/* Set GART in SFB */
	bWriteReg(0xB00C, bReadReg(0xB00C) & ~0x02);
	/* Set GART base address to HW */
	dwWriteReg(0xB034, __pa(info->lut_base));

	return 1;
      fail:
	return 0;
}

static void xgi_pcie_lut_cleanup(struct xgi_info * info)
{
	if (info->lut_base) {
		XGI_INFO("info->lut_base: 0x%p info->lutPageOrder: 0x%lx \n",
			 info->lut_base, info->lutPageOrder);
		xgi_pcie_lut_free((unsigned long)info->lut_base,
				  info->lutPageOrder);
		info->lut_base = NULL;
	}
}

static struct xgi_pcie_block *xgi_pcie_new_node(void)
{
	struct xgi_pcie_block *block =
	    (struct xgi_pcie_block *) kmem_cache_alloc(xgi_pcie_cache_block,
						  GFP_KERNEL);
	if (block == NULL) {
		return NULL;
	}

	block->offset = 0;	/* block's offset in pcie memory, begin from 0 */
	block->size = 0;	/* The block size.              */
	block->bus_addr = 0;	/* CPU access address/bus address */
	block->hw_addr = 0;	/* GE access address            */
	block->page_count = 0;
	block->page_order = 0;
	block->page_block = NULL;
	block->page_table = NULL;
	block->owner = PCIE_INVALID;

	return block;
}

static void xgi_pcie_block_stuff_free(struct xgi_pcie_block * block)
{
	struct page *page;
	struct xgi_page_block *page_block = block->page_block;
	struct xgi_page_block *free_block;
	unsigned long page_count = 0;
	int i;

	//XGI_INFO("block->page_block: 0x%p \n", block->page_block);
	while (page_block) {
		page_count = page_block->page_count;

		page = virt_to_page(page_block->virt_addr);
		for (i = 0; i < page_count; i++, page++) {
			XGI_DEC_PAGE_COUNT(page);
			XGIUnlockPage(page);
		}
		free_pages(page_block->virt_addr, page_block->page_order);

		page_block->phys_addr = 0;
		page_block->virt_addr = 0;
		page_block->page_count = 0;
		page_block->page_order = 0;

		free_block = page_block;
		page_block = page_block->next;
		//XGI_INFO("free free_block: 0x%p \n", free_block);
		kfree(free_block);
		free_block = NULL;
	}

	if (block->page_table) {
		//XGI_INFO("free block->page_table: 0x%p \n", block->page_table);
		kfree(block->page_table);
		block->page_table = NULL;
	}
}

int xgi_pcie_heap_init(struct xgi_info * info)
{
	struct xgi_pcie_block *block;

	if (!xgi_pcie_lut_init(info)) {
		XGI_ERROR("xgi_pcie_lut_init failed\n");
		return 0;
	}

	xgi_pcie_heap =
	    (struct xgi_pcie_heap *) kmalloc(sizeof(struct xgi_pcie_heap), GFP_KERNEL);
	if (!xgi_pcie_heap) {
		XGI_ERROR("xgi_pcie_heap alloc failed\n");
		goto fail1;
	}
	INIT_LIST_HEAD(&xgi_pcie_heap->free_list);
	INIT_LIST_HEAD(&xgi_pcie_heap->used_list);
	INIT_LIST_HEAD(&xgi_pcie_heap->sort_list);

	xgi_pcie_heap->max_freesize = info->pcie.size;

	xgi_pcie_cache_block =
	    kmem_cache_create("xgi_pcie_block", sizeof(struct xgi_pcie_block), 0,
			      SLAB_HWCACHE_ALIGN, NULL, NULL);

	if (NULL == xgi_pcie_cache_block) {
		XGI_ERROR("Fail to creat xgi_pcie_block\n");
		goto fail2;
	}

	block = (struct xgi_pcie_block *) xgi_pcie_new_node();
	if (!block) {
		XGI_ERROR("xgi_pcie_new_node failed\n");
		goto fail3;
	}

	block->offset = 0;	/* block's offset in pcie memory, begin from 0 */
	block->size = info->pcie.size;

	list_add(&block->list, &xgi_pcie_heap->free_list);

	XGI_INFO("PCIE start address: 0x%lx, memory size : 0x%lx\n",
		 block->offset, block->size);
	return 1;
      fail3:
	if (xgi_pcie_cache_block) {
		kmem_cache_destroy(xgi_pcie_cache_block);
		xgi_pcie_cache_block = NULL;
	}

      fail2:
	if (xgi_pcie_heap) {
		kfree(xgi_pcie_heap);
		xgi_pcie_heap = NULL;
	}
      fail1:
	xgi_pcie_lut_cleanup(info);
	return 0;
}

void xgi_pcie_heap_check(void)
{
	struct list_head *useList, *temp;
	struct xgi_pcie_block *block;
	unsigned int ownerIndex;
#ifdef XGI_DEBUG
	char *ownerStr[6] =
	    { "2D", "3D", "3D_CMD", "3D_SCR", "3D_TEX", "ELSE" };
#endif

	if (xgi_pcie_heap) {
		useList = &xgi_pcie_heap->used_list;
		temp = useList->next;
		XGI_INFO("pcie freemax = 0x%lx\n", xgi_pcie_heap->max_freesize);
		while (temp != useList) {
			block = list_entry(temp, struct xgi_pcie_block, list);
			if (block->owner == PCIE_2D)
				ownerIndex = 0;
			else if (block->owner > PCIE_3D_TEXTURE
				 || block->owner < PCIE_2D
				 || block->owner < PCIE_3D)
				ownerIndex = 5;
			else
				ownerIndex = block->owner - PCIE_3D + 1;
			XGI_INFO
			    ("Allocated by %s, block->offset: 0x%lx block->size: 0x%lx \n",
			     ownerStr[ownerIndex], block->offset, block->size);
			temp = temp->next;
		}

	}
}

void xgi_pcie_heap_cleanup(struct xgi_info * info)
{
	struct list_head *free_list, *temp;
	struct xgi_pcie_block *block;
	int j;

	xgi_pcie_lut_cleanup(info);
	XGI_INFO("xgi_pcie_lut_cleanup scceeded\n");

	if (xgi_pcie_heap) {
		free_list = &xgi_pcie_heap->free_list;
		for (j = 0; j < 3; j++, free_list++) {
			temp = free_list->next;

			while (temp != free_list) {
				block =
				    list_entry(temp, struct xgi_pcie_block,
					       list);
				XGI_INFO
				    ("No. %d block->offset: 0x%lx block->size: 0x%lx \n",
				     j, block->offset, block->size);
				xgi_pcie_block_stuff_free(block);
				block->bus_addr = 0;
				block->hw_addr = 0;

				temp = temp->next;
				//XGI_INFO("No. %d free block: 0x%p \n", j, block);
				kmem_cache_free(xgi_pcie_cache_block, block);
				block = NULL;
			}
		}

		XGI_INFO("free xgi_pcie_heap: 0x%p \n", xgi_pcie_heap);
		kfree(xgi_pcie_heap);
		xgi_pcie_heap = NULL;
	}

	if (xgi_pcie_cache_block) {
		kmem_cache_destroy(xgi_pcie_cache_block);
		xgi_pcie_cache_block = NULL;
	}
}

static struct xgi_pcie_block *xgi_pcie_mem_alloc(struct xgi_info * info,
					    unsigned long originalSize,
					    enum PcieOwner owner)
{
	struct list_head *free_list;
	struct xgi_pcie_block *block, *used_block, *free_block;
	struct xgi_page_block *page_block, *prev_page_block;
	struct page *page;
	unsigned long page_order = 0, count = 0, index = 0;
	unsigned long page_addr = 0;
	unsigned long *lut_addr = NULL;
	unsigned long lut_id = 0;
	unsigned long size = (originalSize + PAGE_SIZE - 1) & PAGE_MASK;
	int i, j, page_count = 0;
	int temp = 0;

	XGI_INFO("Jong05302006-xgi_pcie_mem_alloc-Begin\n");
	XGI_INFO("Original 0x%lx bytes requested, really 0x%lx allocated\n",
		 originalSize, size);

	if (owner == PCIE_3D) {
		if (xgi_pcie_vertex_block) {
			XGI_INFO
			    ("PCIE Vertex has been created, return directly.\n");
			return xgi_pcie_vertex_block;
		}
	}

	if (owner == PCIE_3D_CMDLIST) {
		if (xgi_pcie_cmdlist_block) {
			XGI_INFO
			    ("PCIE Cmdlist has been created, return directly.\n");
			return xgi_pcie_cmdlist_block;
		}
	}

	if (owner == PCIE_3D_SCRATCHPAD) {
		if (xgi_pcie_scratchpad_block) {
			XGI_INFO
			    ("PCIE Scratchpad has been created, return directly.\n");
			return xgi_pcie_scratchpad_block;
		}
	}

	if (size == 0) {
		XGI_ERROR("size == 0 \n");
		return (NULL);
	}

	XGI_INFO("max_freesize: 0x%lx \n", xgi_pcie_heap->max_freesize);
	if (size > xgi_pcie_heap->max_freesize) {
		XGI_ERROR
		    ("size: 0x%lx bigger than PCIE total free size: 0x%lx.\n",
		     size, xgi_pcie_heap->max_freesize);
		return (NULL);
	}

	/* Jong 05/30/2006; find next free list which has enough space */
	free_list = xgi_pcie_heap->free_list.next;
	while (free_list != &xgi_pcie_heap->free_list) {
		//XGI_INFO("free_list: 0x%px \n", free_list);
		block = list_entry(free_list, struct xgi_pcie_block, list);
		if (size <= block->size) {
			break;
		}
		free_list = free_list->next;
	}

	if (free_list == &xgi_pcie_heap->free_list) {
		XGI_ERROR("Can't allocate %ldk size from PCIE memory !\n",
			  size / 1024);
		return (NULL);
	}

	free_block = block;
	XGI_INFO("alloc size: 0x%lx from offset: 0x%lx size: 0x%lx \n",
		 size, free_block->offset, free_block->size);

	if (size == free_block->size) {
		used_block = free_block;
		XGI_INFO("size==free_block->size: free_block = 0x%p\n",
			 free_block);
		list_del(&free_block->list);
	} else {
		used_block = xgi_pcie_new_node();
		if (used_block == NULL) {
			return NULL;
		}

		if (used_block == free_block) {
			XGI_ERROR("used_block == free_block = 0x%p\n",
				  used_block);
		}

		used_block->offset = free_block->offset;
		used_block->size = size;

		free_block->offset += size;
		free_block->size -= size;
	}

	xgi_pcie_heap->max_freesize -= size;

	used_block->bus_addr = info->pcie.base + used_block->offset;
	used_block->hw_addr = info->pcie.base + used_block->offset;
	used_block->page_count = page_count = size / PAGE_SIZE;

	/* get page_order base on page_count */
	for (used_block->page_order = 0; page_count; page_count >>= 1) {
		++used_block->page_order;
	}

	if ((used_block->page_count << 1) == (1 << used_block->page_order)) {
		used_block->page_order--;
	}
	XGI_INFO
	    ("used_block->offset: 0x%lx, used_block->size: 0x%lx, used_block->bus_addr: 0x%lx, used_block->hw_addr: 0x%lx, used_block->page_count: 0x%lx used_block->page_order: 0x%lx\n",
	     used_block->offset, used_block->size, used_block->bus_addr,
	     used_block->hw_addr, used_block->page_count,
	     used_block->page_order);

	used_block->page_block = NULL;
	//used_block->page_block = (struct xgi_pages_block *)kmalloc(sizeof(struct xgi_pages_block), GFP_KERNEL);
	//if (!used_block->page_block) return NULL;_t
	//used_block->page_block->next = NULL;

	used_block->page_table =
	    (struct xgi_pte *) kmalloc(sizeof(struct xgi_pte) * used_block->page_count,
				  GFP_KERNEL);
	if (used_block->page_table == NULL) {
		goto fail;
	}

	lut_id = (used_block->offset >> PAGE_SHIFT);
	lut_addr = info->lut_base;
	lut_addr += lut_id;
	XGI_INFO("lutAddr: 0x%p lutID: 0x%lx \n", lut_addr, lut_id);

	/* alloc free pages from system */
	page_count = used_block->page_count;
	page_block = used_block->page_block;
	prev_page_block = used_block->page_block;
	for (i = 0; page_count > 0; i++) {
		/* if size is bigger than 2M bytes, it should be split */
		if (page_count > (1 << XGI_PCIE_ALLOC_MAX_ORDER)) {
			page_order = XGI_PCIE_ALLOC_MAX_ORDER;
		} else {
			count = page_count;
			for (page_order = 0; count; count >>= 1, ++page_order) ;

			if ((page_count << 1) == (1 << page_order)) {
				page_order -= 1;
			}
		}

		count = (1 << page_order);
		page_addr = __get_free_pages(GFP_KERNEL, page_order);
		XGI_INFO("Jong05302006-xgi_pcie_mem_alloc-page_addr=0x%lx \n",
			 page_addr);

		if (!page_addr) {
			XGI_ERROR
			    ("No: %d :Can't get free pages: 0x%lx from system memory !\n",
			     i, count);
			goto fail;
		}

		/* Jong 05/30/2006; test */
		memset((unsigned char *)page_addr, 0xFF,
		       PAGE_SIZE << page_order);
		/* memset((unsigned char *)page_addr, 0, PAGE_SIZE << page_order); */

		if (page_block == NULL) {
			page_block =
			    (struct xgi_page_block *)
			    kmalloc(sizeof(struct xgi_page_block), GFP_KERNEL);
			if (!page_block) {
				XGI_ERROR
				    ("Can't get memory for page_block! \n");
				goto fail;
			}
		}

		if (prev_page_block == NULL) {
			used_block->page_block = page_block;
			prev_page_block = page_block;
		} else {
			prev_page_block->next = page_block;
			prev_page_block = page_block;
		}

		page_block->next = NULL;
		page_block->phys_addr = __pa(page_addr);
		page_block->virt_addr = page_addr;
		page_block->page_count = count;
		page_block->page_order = page_order;

		XGI_INFO
		    ("Jong05302006-xgi_pcie_mem_alloc-page_block->phys_addr=0x%lx \n",
		     page_block->phys_addr);
		XGI_INFO
		    ("Jong05302006-xgi_pcie_mem_alloc-page_block->virt_addr=0x%lx \n",
		     page_block->virt_addr);

		page = virt_to_page(page_addr);

		//XGI_INFO("No: %d page_order: 0x%lx page_count: 0x%x count: 0x%lx index: 0x%lx lut_addr: 0x%p"
		//         "page_block->phys_addr: 0x%lx page_block->virt_addr: 0x%lx \n",
		//          i, page_order, page_count, count, index, lut_addr, page_block->phys_addr, page_block->virt_addr);

		for (j = 0; j < count; j++, page++, lut_addr++) {
			used_block->page_table[index + j].phys_addr =
			    __pa(page_address(page));
			used_block->page_table[index + j].virt_addr =
			    (unsigned long)page_address(page);

			XGI_INFO
			    ("Jong05302006-xgi_pcie_mem_alloc-used_block->page_table[index + j].phys_addr=0x%lx \n",
			     used_block->page_table[index + j].phys_addr);
			XGI_INFO
			    ("Jong05302006-xgi_pcie_mem_alloc-used_block->page_table[index + j].virt_addr=0x%lx \n",
			     used_block->page_table[index + j].virt_addr);

			*lut_addr = __pa(page_address(page));
			XGI_INC_PAGE_COUNT(page);
			XGILockPage(page);

			if (temp) {
				XGI_INFO
				    ("__pa(page_address(page)): 0x%lx lutAddr: 0x%p lutAddr No: 0x%x = 0x%lx \n",
				     __pa(page_address(page)), lut_addr, j,
				     *lut_addr);
				temp--;
			}
		}

		page_block = page_block->next;
		page_count -= count;
		index += count;
		temp = 0;
	}

	used_block->owner = owner;
	list_add(&used_block->list, &xgi_pcie_heap->used_list);

#if defined(__i386__) || defined(__x86_64__)
	asm volatile ("wbinvd":::"memory");
#else
	mb();
#endif

	/* Flush GART Table */
	bWriteReg(0xB03F, 0x40);
	bWriteReg(0xB03F, 0x00);

	if (owner == PCIE_3D) {
		xgi_pcie_vertex_block = used_block;
	}

	if (owner == PCIE_3D_CMDLIST) {
		xgi_pcie_cmdlist_block = used_block;
	}

	if (owner == PCIE_3D_SCRATCHPAD) {
		xgi_pcie_scratchpad_block = used_block;
	}

	XGI_INFO("Jong05302006-xgi_pcie_mem_alloc-End \n");
	return (used_block);

      fail:
	xgi_pcie_block_stuff_free(used_block);
	kmem_cache_free(xgi_pcie_cache_block, used_block);
	return NULL;
}

static struct xgi_pcie_block *xgi_pcie_mem_free(struct xgi_info * info,
					   unsigned long offset)
{
	struct list_head *free_list, *used_list;
	struct xgi_pcie_block *used_block, *block = NULL;
	struct xgi_pcie_block *prev, *next;
	unsigned long upper, lower;

	used_list = xgi_pcie_heap->used_list.next;
	while (used_list != &xgi_pcie_heap->used_list) {
		block = list_entry(used_list, struct xgi_pcie_block, list);
		if (block->offset == offset) {
			break;
		}
		used_list = used_list->next;
	}

	if (used_list == &xgi_pcie_heap->used_list) {
		XGI_ERROR("can't find block: 0x%lx to free!\n", offset);
		return (NULL);
	}

	used_block = block;
	XGI_INFO
	    ("used_block: 0x%p, offset = 0x%lx, size = 0x%lx, bus_addr = 0x%lx, hw_addr = 0x%lx\n",
	     used_block, used_block->offset, used_block->size,
	     used_block->bus_addr, used_block->hw_addr);

	xgi_pcie_block_stuff_free(used_block);

	/* update xgi_pcie_heap */
	xgi_pcie_heap->max_freesize += used_block->size;

	prev = next = NULL;
	upper = used_block->offset + used_block->size;
	lower = used_block->offset;

	free_list = xgi_pcie_heap->free_list.next;

	while (free_list != &xgi_pcie_heap->free_list) {
		block = list_entry(free_list, struct xgi_pcie_block, list);
		if (block->offset == upper) {
			next = block;
		} else if ((block->offset + block->size) == lower) {
			prev = block;
		}
		free_list = free_list->next;
	}

	XGI_INFO("next = 0x%p, prev = 0x%p\n", next, prev);
	list_del(&used_block->list);

	if (prev && next) {
		prev->size += (used_block->size + next->size);
		list_del(&next->list);
		XGI_INFO("free node 0x%p\n", next);
		kmem_cache_free(xgi_pcie_cache_block, next);
		kmem_cache_free(xgi_pcie_cache_block, used_block);
		next = NULL;
		used_block = NULL;
		return (prev);
	}

	if (prev) {
		prev->size += used_block->size;
		XGI_INFO("free node 0x%p\n", used_block);
		kmem_cache_free(xgi_pcie_cache_block, used_block);
		used_block = NULL;
		return (prev);
	}

	if (next) {
		next->size += used_block->size;
		next->offset = used_block->offset;
		XGI_INFO("free node 0x%p\n", used_block);
		kmem_cache_free(xgi_pcie_cache_block, used_block);
		used_block = NULL;
		return (next);
	}

	used_block->bus_addr = 0;
	used_block->hw_addr = 0;
	used_block->page_count = 0;
	used_block->page_order = 0;
	list_add(&used_block->list, &xgi_pcie_heap->free_list);
	XGI_INFO("Recycled free node %p, offset = 0x%lx, size = 0x%lx\n",
		 used_block, used_block->offset, used_block->size);
	return (used_block);
}

void xgi_pcie_alloc(struct xgi_info * info, unsigned long size,
		    enum PcieOwner owner, struct xgi_mem_alloc * alloc)
{
	struct xgi_pcie_block *block;
	struct xgi_mem_pid *mempid_block;

	xgi_down(info->pcie_sem);
	block = xgi_pcie_mem_alloc(info, size, owner);
	xgi_up(info->pcie_sem);

	if (block == NULL) {
		alloc->location = INVALID;
		alloc->size = 0;
		alloc->bus_addr = 0;
		alloc->hw_addr = 0;
		XGI_ERROR("PCIE RAM allocation failed\n");
	} else {
		XGI_INFO
		    ("PCIE RAM allocation succeeded: offset = 0x%lx, bus_addr = 0x%lx\n",
		     block->offset, block->bus_addr);
		alloc->location = NON_LOCAL;
		alloc->size = block->size;
		alloc->bus_addr = block->bus_addr;
		alloc->hw_addr = block->hw_addr;

		/*
		   manage mempid, handle PCIE_3D, PCIE_3D_TEXTURE.
		   PCIE_3D request means a opengl process created.
		   PCIE_3D_TEXTURE request means texture cannot alloc from fb.
		 */
		if (owner == PCIE_3D || owner == PCIE_3D_TEXTURE) {
			mempid_block =
			    kmalloc(sizeof(struct xgi_mem_pid), GFP_KERNEL);
			if (!mempid_block)
				XGI_ERROR("mempid_block alloc failed\n");
			mempid_block->location = NON_LOCAL;
			if (owner == PCIE_3D)
				mempid_block->bus_addr = 0xFFFFFFFF;	/*xgi_pcie_vertex_block has the address */
			else
				mempid_block->bus_addr = alloc->bus_addr;
			mempid_block->pid = alloc->pid;

			XGI_INFO
			    ("Memory ProcessID add one pcie block pid:%ld successfully! \n",
			     mempid_block->pid);
			list_add(&mempid_block->list, &xgi_mempid_list);
		}
	}
}

void xgi_pcie_free(struct xgi_info * info, unsigned long bus_addr)
{
	struct xgi_pcie_block *block;
	unsigned long offset = bus_addr - info->pcie.base;
	struct xgi_mem_pid *mempid_block;
	struct xgi_mem_pid *mempid_freeblock = NULL;
	struct list_head *mempid_list;
	char isvertex = 0;
	int processcnt;

	if (xgi_pcie_vertex_block
	    && xgi_pcie_vertex_block->bus_addr == bus_addr)
		isvertex = 1;

	if (isvertex) {
		/*check is there any other process using vertex */
		processcnt = 0;
		mempid_list = xgi_mempid_list.next;
		while (mempid_list != &xgi_mempid_list) {
			mempid_block =
			    list_entry(mempid_list, struct xgi_mem_pid, list);
			if (mempid_block->location == NON_LOCAL
			    && mempid_block->bus_addr == 0xFFFFFFFF) {
				++processcnt;
			}
			mempid_list = mempid_list->next;
		}
		if (processcnt > 1) {
			return;
		}
	}

	xgi_down(info->pcie_sem);
	block = xgi_pcie_mem_free(info, offset);
	xgi_up(info->pcie_sem);

	if (block == NULL) {
		XGI_ERROR("xgi_pcie_free() failed at base 0x%lx\n", offset);
	}

	if (isvertex)
		xgi_pcie_vertex_block = NULL;

	/* manage mempid */
	mempid_list = xgi_mempid_list.next;
	while (mempid_list != &xgi_mempid_list) {
		mempid_block =
		    list_entry(mempid_list, struct xgi_mem_pid, list);
		if (mempid_block->location == NON_LOCAL
		    && ((isvertex && mempid_block->bus_addr == 0xFFFFFFFF)
			|| (!isvertex && mempid_block->bus_addr == bus_addr))) {
			mempid_freeblock = mempid_block;
			break;
		}
		mempid_list = mempid_list->next;
	}
	if (mempid_freeblock) {
		list_del(&mempid_freeblock->list);
		XGI_INFO
		    ("Memory ProcessID delete one pcie block pid:%ld successfully! \n",
		     mempid_freeblock->pid);
		kfree(mempid_freeblock);
	}
}

/*
 * given a bus address, fid the pcie mem block
 * uses the bus address as the key.
 */
struct xgi_pcie_block *xgi_find_pcie_block(struct xgi_info * info,
					   unsigned long address)
{
	struct list_head *used_list;
	struct xgi_pcie_block *block;
	int i;

	used_list = xgi_pcie_heap->used_list.next;

	while (used_list != &xgi_pcie_heap->used_list) {
		block = list_entry(used_list, struct xgi_pcie_block, list);

		if (block->bus_addr == address) {
			return block;
		}

		if (block->page_table) {
			for (i = 0; i < block->page_count; i++) {
				unsigned long offset = block->bus_addr;
				if ((address >= offset)
				    && (address < (offset + PAGE_SIZE))) {
					return block;
				}
			}
		}
		used_list = used_list->next;
	}

	XGI_ERROR("could not find map for vm 0x%lx\n", address);

	return NULL;
}

/**
 * xgi_find_pcie_virt
 * @address: GE HW address
 *
 * Returns CPU virtual address.  Assumes the CPU VAddr is continuous in not
 * the same block
 */
void *xgi_find_pcie_virt(struct xgi_info * info, unsigned long address)
{
	struct list_head *used_list = xgi_pcie_heap->used_list.next;
	const unsigned long offset_in_page = address & (PAGE_SIZE - 1);

	XGI_INFO("begin (used_list = 0x%p, address = 0x%lx, "
		 "PAGE_SIZE - 1 = %lu, offset_in_page = %lu)\n",
		 used_list, address, PAGE_SIZE - 1, offset_in_page);

	while (used_list != &xgi_pcie_heap->used_list) {
		struct xgi_pcie_block *block = 
			list_entry(used_list, struct xgi_pcie_block, list);

		XGI_INFO("block = 0x%p (hw_addr = 0x%lx, size=%lu)\n",
			 block, block->hw_addr, block->size);

		if ((address >= block->hw_addr)
		    && (address < (block->hw_addr + block->size))) {
			const unsigned long loc_in_pagetable =
			    (address - block->hw_addr) >> PAGE_SHIFT;
			void *const ret =
			    (void *)(block->page_table[loc_in_pagetable].
				     virt_addr + offset_in_page);

			XGI_INFO("PAGE_SHIFT = %d\n", PAGE_SHIFT);
			XGI_INFO("block->page_table[0x%lx].virt_addr = 0x%lx\n",
				 loc_in_pagetable,
				 block->page_table[loc_in_pagetable].virt_addr);
			XGI_INFO("return 0x%p\n", ret);

			return ret;
		} else {
			XGI_INFO("used_list = used_list->next;\n");
			used_list = used_list->next;
		}
	}

	XGI_ERROR("could not find map for vm 0x%lx\n", address);
	return NULL;
}

void xgi_read_pcie_mem(struct xgi_info * info, struct xgi_mem_req * req)
{

}

void xgi_write_pcie_mem(struct xgi_info * info, struct xgi_mem_req * req)
{
}

/*
    address -- GE hw address
*/
void xgi_test_rwinkernel(struct xgi_info * info, unsigned long address)
{
	unsigned long *virtaddr = 0;
	if (address == 0) {
		XGI_INFO("[Jong-kd] input GE HW addr is 0x00000000\n");
		return;
	}

	virtaddr = (unsigned long *)xgi_find_pcie_virt(info, address);

	XGI_INFO("[Jong-kd] input GE HW addr is 0x%lx\n", address);
	XGI_INFO("[Jong-kd] convert to CPU virt addr 0x%px\n", virtaddr);
	XGI_INFO("[Jong-kd] origin [virtaddr] = 0x%lx\n", *virtaddr);
	if (virtaddr != NULL) {
		*virtaddr = 0x00f00fff;
	}

	XGI_INFO("[Jong-kd] modified [virtaddr] = 0x%lx\n", *virtaddr);
}
