/* agpsupport-pre24.h -- Support for pre-2.4.0 kernels -*- linux-c -*-
 * Created: Mon Nov 13 10:54:15 2000 by faith@valinux.com
 *
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
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
 * 
 * Author: Rickard E. (Rik) Faith <faith@valinux.com>
 *
 */

#ifndef _AGPSUPPORT_PRE24_H_
#define _AGPSUPPORT_PRE24_H_
typedef struct {
	void       (*free_memory)(agp_memory *);
	agp_memory *(*allocate_memory)(size_t, u32);
	int        (*bind_memory)(agp_memory *, off_t);
	int        (*unbind_memory)(agp_memory *);
	void       (*enable)(u32);
	int        (*acquire)(void);
	void       (*release)(void);
	void       (*copy_info)(agp_kern_info *);
} drm_agp_t;

static drm_agp_t drm_agp_struct = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

/* The C standard says that 'void *' is not guaranteed to hold a function
   pointer, so we use this union to define a generic pointer that is
   guaranteed to hold any of the function pointers we care about. */
typedef union {
	void          (*free_memory)(agp_memory *);
	agp_memory    *(*allocate_memory)(size_t, u32);
	int           (*bind_memory)(agp_memory *, off_t);
	int           (*unbind_memory)(agp_memory *);
	void          (*enable)(u32);
	int           (*acquire)(void);
	void          (*release)(void);
	void          (*copy_info)(agp_kern_info *);
	unsigned long address;
} drm_agp_func_u;

typedef struct drm_agp_fill {
        const char     *name;
	drm_agp_func_u *f;
} drm_agp_fill_t;

static drm_agp_fill_t drm_agp_fill[] = {
	{ __MODULE_STRING(agp_free_memory),
	   (drm_agp_func_u *)&drm_agp_struct.free_memory     },
	{ __MODULE_STRING(agp_allocate_memory), 
	   (drm_agp_func_u *)&drm_agp_struct.allocate_memory },
	{ __MODULE_STRING(agp_bind_memory),     
	   (drm_agp_func_u *)&drm_agp_struct.bind_memory     },
	{ __MODULE_STRING(agp_unbind_memory),   
	   (drm_agp_func_u *)&drm_agp_struct.unbind_memory   },
	{ __MODULE_STRING(agp_enable),          
	   (drm_agp_func_u *)&drm_agp_struct.enable          },
	{ __MODULE_STRING(agp_backend_acquire), 
	   (drm_agp_func_u *)&drm_agp_struct.acquire         },
	{ __MODULE_STRING(agp_backend_release), 
	   (drm_agp_func_u *)&drm_agp_struct.release         },
	{ __MODULE_STRING(agp_copy_info),       
	   (drm_agp_func_u *)&drm_agp_struct.copy_info       },
	{ NULL, NULL }
};

#define DRM_AGP_GET _drm_agp_get()
#define DRM_AGP_PUT _drm_agp_put()

static drm_agp_t *_drm_agp_get(void)
{
	drm_agp_fill_t *fill;
	int            agp_available = 1;

	for (fill = &drm_agp_fill[0]; fill->name; fill++) {
		char *n  = (char *)fill->name;
		*fill->f = (drm_agp_func_u)get_module_symbol(NULL, n);
		DRM_DEBUG("%s resolves to 0x%08lx\n", n, (*fill->f).address);
		if (!(*fill->f).address) agp_available = 0;
	}
	return &drm_agp_struct;
}

static void _drm_agp_put(void)
{
	drm_agp_fill_t *fill;
	
	for (fill = &drm_agp_fill[0]; fill->name; fill++) {
#if LINUX_VERSION_CODE >= 0x020400
		if ((*fill->f).address) put_module_symbol((*fill->f).address);
#endif
		(*fill->f).address = 0;
	}
}
#endif
