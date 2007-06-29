
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

#ifndef _XGI_PCIE_H_
#define _XGI_PCIE_H_

#ifndef XGI_PCIE_ALLOC_MAX_ORDER
#define XGI_PCIE_ALLOC_MAX_ORDER    1	/* 8K in Kernel 2.4.* */
#endif

struct xgi_page_block {
	struct xgi_page_block *next;
	unsigned long phys_addr;
	unsigned long virt_addr;
	unsigned long page_count;
	unsigned long page_order;
};

struct xgi_pcie_block {
	struct list_head list;
	unsigned long offset;	/* block's offset in pcie memory, begin from 0 */
	unsigned long size;	/* The block size.              */
	unsigned long bus_addr;	/* CPU access address/bus address */
	unsigned long hw_addr;	/* GE access address            */

	unsigned long page_count;
	unsigned long page_order;
	struct xgi_page_block *page_block;
	struct xgi_pte *page_table;	/* list of physical pages allocated */

	atomic_t use_count;
	enum PcieOwner owner;
	unsigned long processID;
};

struct xgi_pcie_heap {
	struct list_head free_list;
	struct list_head used_list;
	struct list_head sort_list;
	unsigned long max_freesize;
};

#endif
