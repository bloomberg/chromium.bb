/* xf86drmCompat.c -- User-level interface to old DRM device drivers
 *
 * Copyright 2002 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * Backwards compatability modules broken out by:
 *   Jens Owen <jens@tungstengraphics.com>
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/xf86drmCompat.c,v 1.1 2002/10/30 12:52:33 alanh Exp $ */

#ifdef XFree86Server
# include "xf86.h"
# include "xf86_OSproc.h"
# include "xf86_ansic.h"
# define _DRM_MALLOC xalloc
# define _DRM_FREE   xfree
# ifndef XFree86LOADER
#  include <sys/mman.h>
# endif
#else
# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <ctype.h>
# include <fcntl.h>
# include <errno.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/ioctl.h>
# include <sys/mman.h>
# include <sys/time.h>
# ifdef DRM_USE_MALLOC
#  define _DRM_MALLOC malloc
#  define _DRM_FREE   free
extern int xf86InstallSIGIOHandler(int fd, void (*f)(int, void *), void *);
extern int xf86RemoveSIGIOHandler(int fd);
# else
#  include <X11/Xlibint.h>
#  define _DRM_MALLOC Xmalloc
#  define _DRM_FREE   Xfree
# endif
#endif

/* Not all systems have MAP_FAILED defined */
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#ifdef __linux__
#include <sys/sysmacros.h>	/* for makedev() */
#endif
#include "xf86drm.h"
#include "xf86drmCompat.h"
#include "drm.h"
#include "mga_drm.h"
#include "r128_drm.h"
#include "radeon_drm.h"
#ifndef __FreeBSD__
#include "sis_drm.h"
#include "i810_drm.h"
#include "i830_drm.h"
#endif

/* WARNING: Do not change, or add, anything to this file.  It is only provided
 * for binary backwards compatability with the old driver specific DRM
 * extensions used before XFree86 4.3.
 */

#ifndef __FreeBSD__
/* I810 */

Bool drmI810CleanupDma(int driSubFD)
{
   drm_i810_init_t init;
   
   memset(&init, 0, sizeof(drm_i810_init_t));
   init.func = I810_CLEANUP_DMA;
   
   if(ioctl(driSubFD, DRM_IOCTL_I810_INIT, &init)) {
      return 0; /* FALSE */
   }
   
   return 1; /* TRUE */
}

Bool drmI810InitDma(int driSubFD, drmCompatI810Init *info)
{
   drm_i810_init_t init;
   
   memset(&init, 0, sizeof(drm_i810_init_t));

   init.func = I810_INIT_DMA;
   init.mmio_offset = info->mmio_offset;
   init.buffers_offset = info->buffers_offset;
   init.ring_start = info->start;
   init.ring_end = info->end;
   init.ring_size = info->size;
   init.sarea_priv_offset = info->sarea_off;
   init.front_offset = info->front_offset;
   init.back_offset = info->back_offset;
   init.depth_offset = info->depth_offset;
   init.overlay_offset = info->overlay_offset;
   init.overlay_physical = info->overlay_physical;
   init.w = info->w;
   init.h = info->h;
   init.pitch = info->pitch;
   init.pitch_bits = info->pitch_bits;

   if(ioctl(driSubFD, DRM_IOCTL_I810_INIT, &init)) {
      return 0; /* FALSE */
   }
   return 1; /* TRUE */
}
#endif /* __FreeBSD__ */

/* Mga */

#define MGA_IDLE_RETRY		2048

int drmMGAInitDMA( int fd, drmCompatMGAInit *info )
{
   drm_mga_init_t init;

   memset( &init, 0, sizeof(drm_mga_init_t) );

   init.func			= MGA_INIT_DMA;

   init.sarea_priv_offset	= info->sarea_priv_offset;
   init.sgram			= info->sgram;
   init.chipset			= info->chipset;
   init.maccess			= info->maccess;

   init.fb_cpp			= info->fb_cpp;
   init.front_offset		= info->front_offset;
   init.front_pitch		= info->front_pitch;
   init.back_offset		= info->back_offset;
   init.back_pitch		= info->back_pitch;

   init.depth_cpp		= info->depth_cpp;
   init.depth_offset		= info->depth_offset;
   init.depth_pitch		= info->depth_pitch;

   init.texture_offset[0]	= info->texture_offset[0];
   init.texture_size[0]		= info->texture_size[0];
   init.texture_offset[1]	= info->texture_offset[1];
   init.texture_size[1]		= info->texture_size[1];

   init.fb_offset		= info->fb_offset;
   init.mmio_offset		= info->mmio_offset;
   init.status_offset		= info->status_offset;
   init.warp_offset		= info->warp_offset;
   init.primary_offset		= info->primary_offset;
   init.buffers_offset		= info->buffers_offset;

   if ( ioctl( fd, DRM_IOCTL_MGA_INIT, &init ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmMGACleanupDMA( int fd )
{
   drm_mga_init_t init;

   memset( &init, 0, sizeof(drm_mga_init_t) );

   init.func = MGA_CLEANUP_DMA;

   if ( ioctl( fd, DRM_IOCTL_MGA_INIT, &init ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmMGAFlushDMA( int fd, drmLockFlags flags )
{
   drm_lock_t lock;
   int ret, i = 0;

   memset( &lock, 0, sizeof(drm_lock_t) );

   if ( flags & DRM_LOCK_QUIESCENT )	lock.flags |= _DRM_LOCK_QUIESCENT;
   if ( flags & DRM_LOCK_FLUSH )	lock.flags |= _DRM_LOCK_FLUSH;
   if ( flags & DRM_LOCK_FLUSH_ALL )	lock.flags |= _DRM_LOCK_FLUSH_ALL;

   do {
      ret = ioctl( fd, DRM_IOCTL_MGA_FLUSH, &lock );
   } while ( ret && errno == EBUSY && i++ < MGA_IDLE_RETRY );

   if ( ret == 0 )
      return 0;
   if ( errno != EBUSY )
      return -errno;

   if ( lock.flags & _DRM_LOCK_QUIESCENT ) {
      /* Only keep trying if we need quiescence.
       */
      lock.flags &= ~(_DRM_LOCK_FLUSH | _DRM_LOCK_FLUSH_ALL);

      do {
	 ret = ioctl( fd, DRM_IOCTL_MGA_FLUSH, &lock );
      } while ( ret && errno == EBUSY && i++ < MGA_IDLE_RETRY );
   }

   if ( ret == 0 ) {
      return 0;
   } else {
      return -errno;
   }
}

int drmMGAEngineReset( int fd )
{
   if ( ioctl( fd, DRM_IOCTL_MGA_RESET, NULL ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmMGAFullScreen( int fd, int enable )
{
   return -EINVAL;
}

int drmMGASwapBuffers( int fd )
{
   int ret, i = 0;

   do {
      ret = ioctl( fd, DRM_IOCTL_MGA_SWAP, NULL );
   } while ( ret && errno == EBUSY && i++ < MGA_IDLE_RETRY );

   if ( ret == 0 ) {
      return 0;
   } else {
      return -errno;
   }
}

int drmMGAClear( int fd, unsigned int flags,
		 unsigned int clear_color, unsigned int clear_depth,
		 unsigned int color_mask, unsigned int depth_mask )
{
   drm_mga_clear_t clear;
   int ret, i = 0;

   clear.flags = flags;
   clear.clear_color = clear_color;
   clear.clear_depth = clear_depth;
   clear.color_mask = color_mask;
   clear.depth_mask = depth_mask;

   do {
      ret = ioctl( fd, DRM_IOCTL_MGA_CLEAR, &clear );
   } while ( ret && errno == EBUSY && i++ < MGA_IDLE_RETRY );

   if ( ret == 0 ) {
      return 0;
   } else {
      return -errno;
   }
}

int drmMGAFlushVertexBuffer( int fd, int index, int used, int discard )
{
   drm_mga_vertex_t vertex;

   vertex.idx = index;
   vertex.used = used;
   vertex.discard = discard;

   if ( ioctl( fd, DRM_IOCTL_MGA_VERTEX, &vertex ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmMGAFlushIndices( int fd, int index, int start, int end, int discard )
{
   drm_mga_indices_t indices;

   indices.idx = index;
   indices.start = start;
   indices.end = end;
   indices.discard = discard;

   if ( ioctl( fd, DRM_IOCTL_MGA_INDICES, &indices ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmMGATextureLoad( int fd, int index,
		       unsigned int dstorg, unsigned int length )
{
   drm_mga_iload_t iload;
   int ret, i = 0;

   iload.idx = index;
   iload.dstorg = dstorg;
   iload.length = length;

   do {
      ret = ioctl( fd, DRM_IOCTL_MGA_ILOAD, &iload );
   } while ( ret && errno == EBUSY && i++ < MGA_IDLE_RETRY );

   if ( ret == 0 ) {
      return 0;
   } else {
      return -errno;
   }
}

int drmMGAAgpBlit( int fd, unsigned int planemask,
		   unsigned int src_offset, int src_pitch,
		   unsigned int dst_offset, int dst_pitch,
		   int delta_sx, int delta_sy,
		   int delta_dx, int delta_dy,
		   int height, int ydir )
{
   drm_mga_blit_t blit;
   int ret, i = 0;

   blit.planemask = planemask;
   blit.srcorg = src_offset;
   blit.dstorg = dst_offset;
   blit.src_pitch = src_pitch;
   blit.dst_pitch = dst_pitch;
   blit.delta_sx = delta_sx;
   blit.delta_sy = delta_sy;
   blit.delta_dx = delta_dx;
   blit.delta_dx = delta_dx;
   blit.height = height;
   blit.ydir = ydir;

   do {
      ret = ioctl( fd, DRM_IOCTL_MGA_BLIT, &blit );
   } while ( ret && errno == EBUSY && i++ < MGA_IDLE_RETRY );

   if ( ret == 0 ) {
      return 0;
   } else {
      return -errno;
   }
}

/* R128 */

#define R128_BUFFER_RETRY	32
#define R128_IDLE_RETRY		32

int drmR128InitCCE( int fd, drmCompatR128Init *info )
{
   drm_r128_init_t init;

   memset( &init, 0, sizeof(drm_r128_init_t) );

   init.func			= R128_INIT_CCE;
   init.sarea_priv_offset	= info->sarea_priv_offset;
   init.is_pci			= info->is_pci;
   init.cce_mode		= info->cce_mode;
   init.cce_secure		= info->cce_secure;
   init.ring_size		= info->ring_size;
   init.usec_timeout		= info->usec_timeout;

   init.fb_bpp			= info->fb_bpp;
   init.front_offset		= info->front_offset;
   init.front_pitch		= info->front_pitch;
   init.back_offset		= info->back_offset;
   init.back_pitch		= info->back_pitch;

   init.depth_bpp		= info->depth_bpp;
   init.depth_offset		= info->depth_offset;
   init.depth_pitch		= info->depth_pitch;
   init.span_offset		= info->span_offset;

   init.fb_offset		= info->fb_offset;
   init.mmio_offset		= info->mmio_offset;
   init.ring_offset		= info->ring_offset;
   init.ring_rptr_offset	= info->ring_rptr_offset;
   init.buffers_offset		= info->buffers_offset;
   init.agp_textures_offset	= info->agp_textures_offset;

   if ( ioctl( fd, DRM_IOCTL_R128_INIT, &init ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128CleanupCCE( int fd )
{
   drm_r128_init_t init;

   memset( &init, 0, sizeof(drm_r128_init_t) );

   init.func = R128_CLEANUP_CCE;

   if ( ioctl( fd, DRM_IOCTL_R128_INIT, &init ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128StartCCE( int fd )
{
   if ( ioctl( fd, DRM_IOCTL_R128_CCE_START, NULL ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128StopCCE( int fd )
{
   drm_r128_cce_stop_t stop;
   int ret, i = 0;

   stop.flush = 1;
   stop.idle = 1;

   ret = ioctl( fd, DRM_IOCTL_R128_CCE_STOP, &stop );

   if ( ret == 0 ) {
      return 0;
   } else if ( errno != EBUSY ) {
      return -errno;
   }

   stop.flush = 0;

   do {
      ret = ioctl( fd, DRM_IOCTL_R128_CCE_STOP, &stop );
   } while ( ret && errno == EBUSY && i++ < R128_IDLE_RETRY );

   if ( ret == 0 ) {
      return 0;
   } else if ( errno != EBUSY ) {
      return -errno;
   }

   stop.idle = 0;

   if ( ioctl( fd, DRM_IOCTL_R128_CCE_STOP, &stop ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128ResetCCE( int fd )
{
   if ( ioctl( fd, DRM_IOCTL_R128_CCE_RESET, NULL ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128WaitForIdleCCE( int fd )
{
   int ret, i = 0;

   do {
      ret = ioctl( fd, DRM_IOCTL_R128_CCE_IDLE, NULL );
   } while ( ret && errno == EBUSY && i++ < R128_IDLE_RETRY );

   if ( ret == 0 ) {
      return 0;
   } else {
      return -errno;
   }
}

int drmR128EngineReset( int fd )
{
   if ( ioctl( fd, DRM_IOCTL_R128_RESET, NULL ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128FullScreen( int fd, int enable )
{
   drm_r128_fullscreen_t fs;

   if ( enable ) {
      fs.func = R128_INIT_FULLSCREEN;
   } else {
      fs.func = R128_CLEANUP_FULLSCREEN;
   }

   if ( ioctl( fd, DRM_IOCTL_R128_FULLSCREEN, &fs ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128SwapBuffers( int fd )
{
   if ( ioctl( fd, DRM_IOCTL_R128_SWAP, NULL ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128Clear( int fd, unsigned int flags,
		  unsigned int clear_color, unsigned int clear_depth,
		  unsigned int color_mask, unsigned int depth_mask )
{
   drm_r128_clear_t clear;

   clear.flags = flags;
   clear.clear_color = clear_color;
   clear.clear_depth = clear_depth;
   clear.color_mask = color_mask;
   clear.depth_mask = depth_mask;

   if ( ioctl( fd, DRM_IOCTL_R128_CLEAR, &clear ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128FlushVertexBuffer( int fd, int prim, int index,
			      int count, int discard )
{
   drm_r128_vertex_t v;

   v.prim = prim;
   v.idx = index;
   v.count = count;
   v.discard = discard;

   if ( ioctl( fd, DRM_IOCTL_R128_VERTEX, &v ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128FlushIndices( int fd, int prim, int index,
			 int start, int end, int discard )
{
   drm_r128_indices_t elts;

   elts.prim = prim;
   elts.idx = index;
   elts.start = start;
   elts.end = end;
   elts.discard = discard;

   if ( ioctl( fd, DRM_IOCTL_R128_INDICES, &elts ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128TextureBlit( int fd, int index,
			int offset, int pitch, int format,
			int x, int y, int width, int height )
{
   drm_r128_blit_t blit;

   blit.idx = index;
   blit.offset = offset;
   blit.pitch = pitch;
   blit.format = format;
   blit.x = x;
   blit.y = y;
   blit.width = width;
   blit.height = height;

   if ( ioctl( fd, DRM_IOCTL_R128_BLIT, &blit ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128WriteDepthSpan( int fd, int n, int x, int y,
			   const unsigned int depth[],
			   const unsigned char mask[] )
{
   drm_r128_depth_t d;

   d.func = R128_WRITE_SPAN;
   d.n = n;
   d.x = &x;
   d.y = &y;
   d.buffer = (unsigned int *)depth;
   d.mask = (unsigned char *)mask;

   if ( ioctl( fd, DRM_IOCTL_R128_DEPTH, &d ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128WriteDepthPixels( int fd, int n,
			     const int x[], const int y[],
			     const unsigned int depth[],
			     const unsigned char mask[] )
{
   drm_r128_depth_t d;

   d.func = R128_WRITE_PIXELS;
   d.n = n;
   d.x = (int *)x;
   d.y = (int *)y;
   d.buffer = (unsigned int *)depth;
   d.mask = (unsigned char *)mask;

   if ( ioctl( fd, DRM_IOCTL_R128_DEPTH, &d ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128ReadDepthSpan( int fd, int n, int x, int y )
{
   drm_r128_depth_t d;

   d.func = R128_READ_SPAN;
   d.n = n;
   d.x = &x;
   d.y = &y;
   d.buffer = NULL;
   d.mask = NULL;

   if ( ioctl( fd, DRM_IOCTL_R128_DEPTH, &d ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128ReadDepthPixels( int fd, int n,
			    const int x[], const int y[] )
{
   drm_r128_depth_t d;

   d.func = R128_READ_PIXELS;
   d.n = n;
   d.x = (int *)x;
   d.y = (int *)y;
   d.buffer = NULL;
   d.mask = NULL;

   if ( ioctl( fd, DRM_IOCTL_R128_DEPTH, &d ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128PolygonStipple( int fd, unsigned int *mask )
{
   drm_r128_stipple_t stipple;

   stipple.mask = mask;

   if ( ioctl( fd, DRM_IOCTL_R128_STIPPLE, &stipple ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmR128FlushIndirectBuffer( int fd, int index,
				int start, int end, int discard )
{
   drm_r128_indirect_t ind;

   ind.idx = index;
   ind.start = start;
   ind.end = end;
   ind.discard = discard;

   if ( ioctl( fd, DRM_IOCTL_R128_INDIRECT, &ind ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

/* Radeon */

#define RADEON_BUFFER_RETRY	32
#define RADEON_IDLE_RETRY	16

int drmRadeonInitCP( int fd, drmCompatRadeonInit *info )
{
   drm_radeon_init_t init;

   memset( &init, 0, sizeof(drm_radeon_init_t) );

   init.func			= RADEON_INIT_CP;
   init.sarea_priv_offset	= info->sarea_priv_offset;
   init.is_pci			= info->is_pci;
   init.cp_mode			= info->cp_mode;
   init.agp_size		= info->agp_size;
   init.ring_size		= info->ring_size;
   init.usec_timeout		= info->usec_timeout;

   init.fb_bpp			= info->fb_bpp;
   init.front_offset		= info->front_offset;
   init.front_pitch		= info->front_pitch;
   init.back_offset		= info->back_offset;
   init.back_pitch		= info->back_pitch;

   init.depth_bpp		= info->depth_bpp;
   init.depth_offset		= info->depth_offset;
   init.depth_pitch		= info->depth_pitch;

   init.fb_offset		= info->fb_offset;
   init.mmio_offset		= info->mmio_offset;
   init.ring_offset		= info->ring_offset;
   init.ring_rptr_offset	= info->ring_rptr_offset;
   init.buffers_offset		= info->buffers_offset;
   init.agp_textures_offset	= info->agp_textures_offset;

   if ( ioctl( fd, DRM_IOCTL_RADEON_CP_INIT, &init ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonCleanupCP( int fd )
{
   drm_radeon_init_t init;

   memset( &init, 0, sizeof(drm_radeon_init_t) );

   init.func = RADEON_CLEANUP_CP;

   if ( ioctl( fd, DRM_IOCTL_RADEON_CP_INIT, &init ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonStartCP( int fd )
{
   if ( ioctl( fd, DRM_IOCTL_RADEON_CP_START, NULL ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonStopCP( int fd )
{
   drm_radeon_cp_stop_t stop;
   int ret, i = 0;

   stop.flush = 1;
   stop.idle = 1;

   ret = ioctl( fd, DRM_IOCTL_RADEON_CP_STOP, &stop );

   if ( ret == 0 ) {
      return 0;
   } else if ( errno != EBUSY ) {
      return -errno;
   }

   stop.flush = 0;

   do {
      ret = ioctl( fd, DRM_IOCTL_RADEON_CP_STOP, &stop );
   } while ( ret && errno == EBUSY && i++ < RADEON_IDLE_RETRY );

   if ( ret == 0 ) {
      return 0;
   } else if ( errno != EBUSY ) {
      return -errno;
   }

   stop.idle = 0;

   if ( ioctl( fd, DRM_IOCTL_RADEON_CP_STOP, &stop ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonResetCP( int fd )
{
   if ( ioctl( fd, DRM_IOCTL_RADEON_CP_RESET, NULL ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonWaitForIdleCP( int fd )
{
   int ret, i = 0;

   do {
      ret = ioctl( fd, DRM_IOCTL_RADEON_CP_IDLE, NULL );
   } while ( ret && errno == EBUSY && i++ < RADEON_IDLE_RETRY );

   if ( ret == 0 ) {
      return 0;
   } else {
      return -errno;
   }
}

int drmRadeonEngineReset( int fd )
{
   if ( ioctl( fd, DRM_IOCTL_RADEON_RESET, NULL ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonFullScreen( int fd, int enable )
{
   drm_radeon_fullscreen_t fs;

   if ( enable ) {
      fs.func = RADEON_INIT_FULLSCREEN;
   } else {
      fs.func = RADEON_CLEANUP_FULLSCREEN;
   }

   if ( ioctl( fd, DRM_IOCTL_RADEON_FULLSCREEN, &fs ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonSwapBuffers( int fd )
{
   if ( ioctl( fd, DRM_IOCTL_RADEON_SWAP, NULL ) ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonClear( int fd, unsigned int flags,
		    unsigned int clear_color, unsigned int clear_depth,
		    unsigned int color_mask, unsigned int stencil,
		    void *b, int nbox )
{
   drm_radeon_clear_t clear;
   drm_radeon_clear_rect_t depth_boxes[RADEON_NR_SAREA_CLIPRECTS];
   drm_clip_rect_t *boxes = (drm_clip_rect_t *)b;
   int i;

   clear.flags = flags;
   clear.clear_color = clear_color;
   clear.clear_depth = clear_depth;
   clear.color_mask = color_mask;
   clear.depth_mask = stencil;	/* misnamed field in ioctl */
   clear.depth_boxes = depth_boxes;

   /* We can remove this when we do real depth clears, instead of
    * rendering a rectangle into the depth buffer.  This prevents
    * floating point calculations being done in the kernel.
    */
   for ( i = 0 ; i < nbox ; i++ ) {
      depth_boxes[i].f[CLEAR_X1] = (float)boxes[i].x1;
      depth_boxes[i].f[CLEAR_Y1] = (float)boxes[i].y1;
      depth_boxes[i].f[CLEAR_X2] = (float)boxes[i].x2;
      depth_boxes[i].f[CLEAR_Y2] = (float)boxes[i].y2;
      depth_boxes[i].f[CLEAR_DEPTH] = (float)clear_depth;
   }

   if ( ioctl( fd, DRM_IOCTL_RADEON_CLEAR, &clear ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonFlushVertexBuffer( int fd, int prim, int index,
				int count, int discard )
{
   drm_radeon_vertex_t v;

   v.prim = prim;
   v.idx = index;
   v.count = count;
   v.discard = discard;

   if ( ioctl( fd, DRM_IOCTL_RADEON_VERTEX, &v ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonFlushIndices( int fd, int prim, int index,
			   int start, int end, int discard )
{
   drm_radeon_indices_t elts;

   elts.prim = prim;
   elts.idx = index;
   elts.start = start;
   elts.end = end;
   elts.discard = discard;

   if ( ioctl( fd, DRM_IOCTL_RADEON_INDICES, &elts ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonLoadTexture( int fd, int offset, int pitch, int format, int width,
                          int height, drmCompatRadeonTexImage *image )
{
   drm_radeon_texture_t tex;
   drm_radeon_tex_image_t tmp;
   int ret;

   tex.offset = offset;
   tex.pitch = pitch;
   tex.format = format;
   tex.width = width;
   tex.height = height;
   tex.image = &tmp;

   /* This gets updated by the kernel when a multipass blit is needed.
    */
   memcpy( &tmp, image, sizeof(drm_radeon_tex_image_t) );

   do {
      ret = ioctl( fd, DRM_IOCTL_RADEON_TEXTURE, &tex );
   } while ( ret && errno == EAGAIN );

   if ( ret == 0 ) {
      return 0;
   } else {
      return -errno;
   }
}

int drmRadeonPolygonStipple( int fd, unsigned int *mask )
{
   drm_radeon_stipple_t stipple;

   stipple.mask = mask;

   if ( ioctl( fd, DRM_IOCTL_RADEON_STIPPLE, &stipple ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

int drmRadeonFlushIndirectBuffer( int fd, int index,
				  int start, int end, int discard )
{
   drm_radeon_indirect_t ind;

   ind.idx = index;
   ind.start = start;
   ind.end = end;
   ind.discard = discard;

   if ( ioctl( fd, DRM_IOCTL_RADEON_INDIRECT, &ind ) < 0 ) {
      return -errno;
   } else {
      return 0;
   }
}

#ifndef __FreeBSD__
/* SiS */

Bool drmSiSAgpInit(int driSubFD, int offset, int size)
{
   drm_sis_agp_t agp;
      
   agp.offset = offset;
   agp.size = size;
   ioctl(driSubFD, SIS_IOCTL_AGP_INIT, &agp);

   return 1; /* TRUE */
}

/* I830 */

Bool drmI830CleanupDma(int driSubFD)
{
   drm_i830_init_t init;

   memset(&init, 0, sizeof(drm_i830_init_t));
   init.func = I810_CLEANUP_DMA;

   if(ioctl(driSubFD, DRM_IOCTL_I830_INIT, &init)) {
      return 0; /* FALSE */
   }

   return 1; /* TRUE */
}

Bool drmI830InitDma(int driSubFD, drmCompatI830Init *info)
{
   drm_i830_init_t init;

   memset(&init, 0, sizeof(drm_i830_init_t));

   init.func = I810_INIT_DMA;
   init.mmio_offset = info->mmio_offset;
   init.buffers_offset = info->buffers_offset;
   init.ring_start = info->start;
   init.ring_end = info->end;
   init.ring_size = info->size;
   init.sarea_priv_offset = info->sarea_off;
   init.front_offset = info->front_offset;
   init.back_offset = info->back_offset;
   init.depth_offset = info->depth_offset;
   init.w = info->w;
   init.h = info->h;
   init.pitch = info->pitch;
   init.pitch_bits = info->pitch_bits;
   init.back_pitch = info->pitch;
   init.depth_pitch = info->pitch;
   init.cpp = info->cpp;

   if(ioctl(driSubFD, DRM_IOCTL_I830_INIT, &init)) {
      return 0; /* FALSE */
   }
   return 1; /* TRUE */
}
#endif /* __FreeBSD__ */

/* WARNING: Do not change, or add, anything to this file.  It is only provided
 * for binary backwards compatability with the old driver specific DRM
 * extensions used before XFree86 4.3.
 */
