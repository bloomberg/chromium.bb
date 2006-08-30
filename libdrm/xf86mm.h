/**************************************************************************
 * 
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND. USA.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * 
 **************************************************************************/

#ifndef _XF86MM_H_
#define _XF86MM_H_
#include <stddef.h>
#include "drm.h"

/*
 * List macros heavily inspired by the Linux kernel
 * list handling. No list looping yet.
 */

typedef struct _drmMMListHead
{
    struct _drmMMListHead *prev;
    struct _drmMMListHead *next;
} drmMMListHead;

#define DRMINITLISTHEAD(__item)		       \
  do{					       \
    (__item)->prev = (__item);		       \
    (__item)->next = (__item);		       \
  } while (0)

#define DRMLISTADD(__item, __list)			\
  do {						\
    (__item)->prev = (__list);			\
    (__item)->next = (__list)->next;		\
    (__list)->next->prev = (__item);		\
    (__list)->next = (__item);			\
  } while (0)

#define DRMLISTADDTAIL(__item, __list)		\
  do {						\
    (__item)->next = (__list);			\
    (__item)->prev = (__list)->prev;		\
    (__list)->prev->next = (__item);		\
    (__list)->prev = (__item);			\
  } while(0)

#define DRMLISTDEL(__item)			\
  do {						\
    (__item)->prev->next = (__item)->next;	\
    (__item)->next->prev = (__item)->prev;	\
  } while(0)

#define DRMLISTDELINIT(__item)			\
  do {						\
    (__item)->prev->next = (__item)->next;	\
    (__item)->next->prev = (__item)->prev;	\
    (__item)->next = (__item);			\
    (__item)->prev = (__item);			\
  } while(0)

#define DRMLISTENTRY(__type, __item, __field)   \
    ((__type *)(((char *) (__item)) - offsetof(__type, __field)))


typedef struct _drmBO{
    drm_bo_type_t type;
    unsigned handle;
    drm_handle_t mapHandle;
    unsigned flags;
    unsigned mask;
    unsigned mapFlags;
    unsigned long size;
    unsigned long offset;
    unsigned long start;
    void *virtual;
    void *mapVirtual;
    int mapCount;
    drmTTM *ttm;
} drmBO;


typedef struct _drmBONode {
    drmMMListHead head;
    drmBO *buf;
    drm_bo_arg_t bo_arg;
} drmBONode;

typedef struct _drmBOList {
    unsigned numTarget;
    unsigned numCurrent;
    unsigned numOnList;
    drmMMListHead list;
    drmMMListHead free;
} drmBOList;

extern int drmBOCreate(int fd, drmTTM *ttm, unsigned long start, unsigned long size,
			      void *user_buffer, drm_bo_type_t type, unsigned mask,
		unsigned hint, drmBO *buf);
extern int drmBODestroy(int fd, drmBO *buf);
extern int drmBOReference(int fd, unsigned handle, drmBO *buf);
extern int drmBOUnReference(int fd, drmBO *buf);


#endif
