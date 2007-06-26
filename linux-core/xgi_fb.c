
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
#include "xgi_fb.h"

#define XGI_FB_HEAP_START 0x1000000

static xgi_mem_heap_t   *xgi_fb_heap;
static kmem_cache_t     *xgi_fb_cache_block = NULL;
extern struct list_head xgi_mempid_list;

static xgi_mem_block_t *xgi_mem_new_node(void);
static xgi_mem_block_t *xgi_mem_alloc(xgi_info_t *info, unsigned long size);
static xgi_mem_block_t *xgi_mem_free(xgi_info_t *info, unsigned long offset);

void xgi_fb_alloc(xgi_info_t *info,
                  xgi_mem_req_t *req,
                  xgi_mem_alloc_t *alloc)
{
    xgi_mem_block_t *block;
    xgi_mem_pid_t *mempid_block;

    if (req->is_front)
    {
        alloc->location = LOCAL;
        alloc->bus_addr = info->fb.base;
        alloc->hw_addr  = 0;
        XGI_INFO("Video RAM allocation on front buffer successfully! \n");
    }
    else
    {
        xgi_down(info->fb_sem);
        block = xgi_mem_alloc(info, req->size);
        xgi_up(info->fb_sem);

        if (block == NULL)
        {
            alloc->location = LOCAL;
            alloc->size     = 0;
            alloc->bus_addr = 0;
            alloc->hw_addr  = 0;
            XGI_ERROR("Video RAM allocation failed\n");
        }
        else
        {
            XGI_INFO("Video RAM allocation succeeded: 0x%p\n",
                    (char *) block->offset);
            alloc->location = LOCAL;
            alloc->size     = block->size;
            alloc->bus_addr = info->fb.base + block->offset;
            alloc->hw_addr  = block->offset;

            /* manage mempid */
            mempid_block = kmalloc(sizeof(xgi_mem_pid_t), GFP_KERNEL);
            mempid_block->location = LOCAL;
            mempid_block->bus_addr = alloc->bus_addr;
            mempid_block->pid = alloc->pid;

            if (!mempid_block)
                XGI_ERROR("mempid_block alloc failed\n");

            XGI_INFO("Memory ProcessID add one fb block pid:%ld successfully! \n", mempid_block->pid);
            list_add(&mempid_block->list, &xgi_mempid_list);
        }
    }
}

void xgi_fb_free(xgi_info_t *info, unsigned long bus_addr)
{
    xgi_mem_block_t     *block;
    unsigned long       offset = bus_addr - info->fb.base;
    xgi_mem_pid_t       *mempid_block;
    xgi_mem_pid_t       *mempid_freeblock = NULL;
    struct list_head    *mempid_list;

    if (offset < 0)
    {
        XGI_INFO("free onscreen frame buffer successfully !\n");
    }
    else
    {
        xgi_down(info->fb_sem);
        block = xgi_mem_free(info, offset);
        xgi_up(info->fb_sem);

        if (block == NULL)
        {
            XGI_ERROR("xgi_mem_free() failed at base 0x%lx\n", offset);
        }

        /* manage mempid */
        mempid_list = xgi_mempid_list.next;
        while (mempid_list != &xgi_mempid_list)
        {
            mempid_block = list_entry(mempid_list, struct xgi_mem_pid_s, list);
            if (mempid_block->location == LOCAL && mempid_block->bus_addr == bus_addr)
            {
                mempid_freeblock = mempid_block;
                break;
            }
            mempid_list = mempid_list->next;
        }
        if (mempid_freeblock)
        {
            list_del(&mempid_freeblock->list);
            XGI_INFO("Memory ProcessID delete one fb block pid:%ld successfully! \n", mempid_freeblock->pid);
            kfree(mempid_freeblock);
        }
    }
}

int xgi_fb_heap_init(xgi_info_t *info)
{
    xgi_mem_block_t *block;

    xgi_fb_heap = kmalloc(sizeof(xgi_mem_heap_t), GFP_KERNEL);
    if (!xgi_fb_heap)
    {
        XGI_ERROR("xgi_fb_heap alloc failed\n");
        return 0;
    }

    INIT_LIST_HEAD(&xgi_fb_heap->free_list);
    INIT_LIST_HEAD(&xgi_fb_heap->used_list);
    INIT_LIST_HEAD(&xgi_fb_heap->sort_list);

    xgi_fb_cache_block = kmem_cache_create("xgi_fb_block", sizeof(xgi_mem_block_t),
                                           0, SLAB_HWCACHE_ALIGN, NULL, NULL);

    if (NULL == xgi_fb_cache_block)
    {
         XGI_ERROR("Fail to creat xgi_fb_block\n");
         goto fail1;
    }

    block = (xgi_mem_block_t *)kmem_cache_alloc(xgi_fb_cache_block, GFP_KERNEL);
    if (!block)
    {
        XGI_ERROR("kmem_cache_alloc failed\n");
        goto fail2;
    }

    block->offset = XGI_FB_HEAP_START;
    block->size   = info->fb.size - XGI_FB_HEAP_START;

    list_add(&block->list, &xgi_fb_heap->free_list);

    xgi_fb_heap->max_freesize = info->fb.size - XGI_FB_HEAP_START;

    XGI_INFO("fb start offset: 0x%lx, memory size : 0x%lx\n", block->offset, block->size);
    XGI_INFO("xgi_fb_heap->max_freesize: 0x%lx \n", xgi_fb_heap->max_freesize);

    return 1;

fail2:
    if (xgi_fb_cache_block)
    {
        kmem_cache_destroy(xgi_fb_cache_block);
        xgi_fb_cache_block = NULL;
    }
fail1:
    if(xgi_fb_heap)
    {
        kfree(xgi_fb_heap);
        xgi_fb_heap = NULL;
    }
    return 0;
}

void xgi_fb_heap_cleanup(xgi_info_t *info)
{
    struct list_head    *free_list, *temp;
    xgi_mem_block_t     *block;
    int                 i;

    if (xgi_fb_heap)
    {
        free_list = &xgi_fb_heap->free_list;
        for (i = 0; i < 3; i++, free_list++)
        {
            temp = free_list->next;
            while (temp != free_list)
            {
                block = list_entry(temp, struct xgi_mem_block_s, list);
                temp = temp->next;

                XGI_INFO("No. %d block->offset: 0x%lx block->size: 0x%lx \n",
                          i, block->offset, block->size);
                //XGI_INFO("No. %d free block: 0x%p \n", i, block);
                kmem_cache_free(xgi_fb_cache_block, block);
                block = NULL;
            }
        }
        XGI_INFO("xgi_fb_heap: 0x%p \n", xgi_fb_heap);
        kfree(xgi_fb_heap);
        xgi_fb_heap = NULL;
    }

    if (xgi_fb_cache_block)
    {
        kmem_cache_destroy(xgi_fb_cache_block);
        xgi_fb_cache_block = NULL;
    }
}

static xgi_mem_block_t * xgi_mem_new_node(void)
{
    xgi_mem_block_t *block;

    block = (xgi_mem_block_t *)kmem_cache_alloc(xgi_fb_cache_block, GFP_KERNEL);
    if (!block)
    {
        XGI_ERROR("kmem_cache_alloc failed\n");
        return NULL;
    }

    return block;
}

#if 0
static void xgi_mem_insert_node_after(xgi_mem_list_t *list,
                                      xgi_mem_block_t *current,
                                      xgi_mem_block_t *block);
static void xgi_mem_insert_node_before(xgi_mem_list_t *list,
                                       xgi_mem_block_t *current,
                                       xgi_mem_block_t *block);
static void xgi_mem_insert_node_head(xgi_mem_list_t *list,
                                     xgi_mem_block_t *block);
static void xgi_mem_insert_node_tail(xgi_mem_list_t *list,
                                     xgi_mem_block_t *block);
static void xgi_mem_delete_node(xgi_mem_list_t *list,
                                xgi_mem_block_t *block);
/*
 *  insert node:block after node:current
 */
static void xgi_mem_insert_node_after(xgi_mem_list_t *list,
                                      xgi_mem_block_t *current,
                                      xgi_mem_block_t *block)
{
    block->prev = current;
    block->next = current->next;
    current->next = block;

    if (current == list->tail)
    {
        list->tail = block;
    }
    else
    {
        block->next->prev = block;
    }
}

/*
 *  insert node:block before node:current
 */
static void xgi_mem_insert_node_before(xgi_mem_list_t *list,
                                       xgi_mem_block_t *current,
                                       xgi_mem_block_t *block)
{
    block->prev = current->prev;
    block->next = current;
    current->prev = block;
    if (current == list->head)
    {
        list->head = block;
    }
    else
    {
        block->prev->next = block;
    }
}
void xgi_mem_insert_node_head(xgi_mem_list_t *list,
                              xgi_mem_block_t *block)
{
    block->next = list->head;
    block->prev = NULL;

    if (NULL == list->head)
    {
        list->tail = block;
    }
    else
    {
        list->head->prev = block;
    }
    list->head = block;
}

static void xgi_mem_insert_node_tail(xgi_mem_list_t *list,
                                     xgi_mem_block_t *block)

{
    block->next = NULL;
    block->prev = list->tail;
    if (NULL == list->tail)
    {
        list->head = block;
    }
    else
    {
        list->tail->next = block;
    }
    list->tail = block;
}

static void xgi_mem_delete_node(xgi_mem_list_t *list,
                         xgi_mem_block_t *block)
{
    if (block == list->head)
    {
        list->head = block->next;
    }
    if (block == list->tail)
    {
        list->tail = block->prev;
    }

    if (block->prev)
    {
        block->prev->next = block->next;
    }
    if (block->next)
    {
        block->next->prev = block->prev;
    }

    block->next = block->prev = NULL;
}
#endif
static xgi_mem_block_t *xgi_mem_alloc(xgi_info_t *info, unsigned long originalSize)
{
    struct list_head    *free_list;
    xgi_mem_block_t     *block, *free_block, *used_block;

    unsigned long       size = (originalSize + PAGE_SIZE - 1) & PAGE_MASK;

    XGI_INFO("Original 0x%lx bytes requested, really 0x%lx allocated\n", originalSize, size);

    if (size == 0)
    {
        XGI_ERROR("size == 0\n");
        return (NULL);
    }
    XGI_INFO("max_freesize: 0x%lx \n", xgi_fb_heap->max_freesize);
    if (size > xgi_fb_heap->max_freesize)
    {
        XGI_ERROR("size: 0x%lx is bigger than frame buffer total free size: 0x%lx !\n",
                  size, xgi_fb_heap->max_freesize);
        return (NULL);
    }

    free_list = xgi_fb_heap->free_list.next;

    while (free_list != &xgi_fb_heap->free_list)
    {
        XGI_INFO("free_list: 0x%px \n", free_list);
        block = list_entry(free_list, struct xgi_mem_block_s, list);
        if (size <= block->size)
        {
            break;
        }
        free_list = free_list->next;
    }

    if (free_list == &xgi_fb_heap->free_list)
    {
        XGI_ERROR("Can't allocate %ldk size from frame buffer memory !\n", size/1024);
        return (NULL);
    }

    free_block = block;
    XGI_INFO("alloc size: 0x%lx from offset: 0x%lx size: 0x%lx \n",
              size, free_block->offset, free_block->size);

    if (size == free_block->size)
    {
        used_block = free_block;
        XGI_INFO("size == free_block->size: free_block = 0x%p\n", free_block);
        list_del(&free_block->list);
    }
    else
    {
        used_block = xgi_mem_new_node();

        if (used_block == NULL)  return (NULL);

        if (used_block == free_block)
        {
            XGI_ERROR("used_block == free_block = 0x%p\n", used_block);
        }

        used_block->offset = free_block->offset;
        used_block->size = size;

        free_block->offset += size;
        free_block->size -= size;
    }

    xgi_fb_heap->max_freesize -= size;

    list_add(&used_block->list, &xgi_fb_heap->used_list);

    return (used_block);
}

static xgi_mem_block_t *xgi_mem_free(xgi_info_t *info, unsigned long offset)
{
    struct list_head    *free_list, *used_list;
    xgi_mem_block_t     *used_block = NULL, *block = NULL;
    xgi_mem_block_t     *prev, *next;

    unsigned long       upper;
    unsigned long       lower;

    used_list = xgi_fb_heap->used_list.next;
    while (used_list != &xgi_fb_heap->used_list)
    {
        block = list_entry(used_list, struct xgi_mem_block_s, list);
        if (block->offset == offset)
        {
            break;
        }
        used_list = used_list->next;
    }

    if (used_list == &xgi_fb_heap->used_list)
    {
        XGI_ERROR("can't find block: 0x%lx to free!\n", offset);
        return (NULL);
    }

    used_block = block;
    XGI_INFO("used_block: 0x%p, offset = 0x%lx, size = 0x%lx\n",
              used_block, used_block->offset, used_block->size);

    xgi_fb_heap->max_freesize += used_block->size;

    prev = next = NULL;
    upper = used_block->offset + used_block->size;
    lower = used_block->offset;

    free_list = xgi_fb_heap->free_list.next;
    while (free_list != &xgi_fb_heap->free_list)
    {
        block = list_entry(free_list, struct xgi_mem_block_s, list);

        if (block->offset == upper)
        {
            next = block;
        }
        else if ((block->offset + block->size) == lower)
        {
            prev = block;
        }
        free_list = free_list->next;
    }

    XGI_INFO("next = 0x%p, prev = 0x%p\n", next, prev);
    list_del(&used_block->list);

    if (prev && next)
    {
        prev->size += (used_block->size + next->size);
        list_del(&next->list);
        XGI_INFO("free node 0x%p\n", next);
        kmem_cache_free(xgi_fb_cache_block, next);
        kmem_cache_free(xgi_fb_cache_block, used_block);

        next = NULL;
        used_block = NULL;
        return (prev);
    }

    if (prev)
    {
        prev->size += used_block->size;
        XGI_INFO("free node 0x%p\n", used_block);
        kmem_cache_free(xgi_fb_cache_block, used_block);
        used_block = NULL;
        return (prev);
    }

    if (next)
    {
        next->size += used_block->size;
        next->offset = used_block->offset;
        XGI_INFO("free node 0x%p\n", used_block);
        kmem_cache_free(xgi_fb_cache_block, used_block);
        used_block = NULL;
        return (next);
    }

    list_add(&used_block->list, &xgi_fb_heap->free_list);
    XGI_INFO("Recycled free node %p, offset = 0x%lx, size = 0x%lx\n",
              used_block, used_block->offset, used_block->size);

    return (used_block);
}

