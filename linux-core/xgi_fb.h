
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

#ifndef _XGI_FB_H_
#define _XGI_FB_H_

typedef struct xgi_mem_block_s {
    struct  list_head       list;
    unsigned long           offset;
    unsigned long           size;
    atomic_t                use_count;
} xgi_mem_block_t;

typedef struct xgi_mem_heap_s {
    struct list_head    free_list;
    struct list_head    used_list;
    struct list_head    sort_list;
    unsigned long       max_freesize;
    spinlock_t          lock;
} xgi_mem_heap_t;

#if 0
typedef struct xgi_mem_block_s {
    struct xgi_mem_block_s  *next;
    struct xgi_mem_block_s  *prev;
    unsigned long           offset;
    unsigned long           size;
    atomic_t                use_count;
} xgi_mem_block_t;

typedef struct xgi_mem_list_s {
    xgi_mem_block_t     *head;
    xgi_mem_block_t     *tail;
} xgi_mem_list_t;

typedef struct xgi_mem_heap_s {
    xgi_mem_list_t      *free_list;
    xgi_mem_list_t      *used_list;
    xgi_mem_list_t      *sort_list;
    unsigned long       max_freesize;
    spinlock_t          lock;
} xgi_mem_heap_t;
#endif

#endif

