/* radeon_state.c -- State support for Radeon -*- linux-c -*-
 *
 * Copyright 2000 VA Linux Systems, Inc., Fremont, California.
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
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *    Kevin E. Martin <martin@valinux.com>
 */

#define __NO_VERSION__
#include "radeon.h"
#include "drmP.h"
#include "radeon_drv.h"
#include "drm.h"
#include <linux/delay.h>


/* ================================================================
 * CP hardware state programming functions
 */

static inline void radeon_emit_clip_rect( drm_radeon_private_t *dev_priv,
					  drm_clip_rect_t *box )
{
	RING_LOCALS;

	DRM_DEBUG( "   box:  x1=%d y1=%d  x2=%d y2=%d\n",
		   box->x1, box->y1, box->x2, box->y2 );

	BEGIN_RING( 4 );

	OUT_RING( CP_PACKET0( RADEON_RE_TOP_LEFT, 0 ) );
	OUT_RING( (box->y1 << 16) | box->x1 );

	OUT_RING( CP_PACKET0( RADEON_RE_WIDTH_HEIGHT, 0 ) );
	OUT_RING( ((box->y2 - 1) << 16) | (box->x2 - 1) );

	ADVANCE_RING();
}

static inline void radeon_emit_context( drm_radeon_private_t *dev_priv,
					drm_radeon_context_regs_t *ctx )
{
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 14 );

	OUT_RING( CP_PACKET0( RADEON_PP_MISC, 6 ) );
	OUT_RING( ctx->pp_misc );
	OUT_RING( ctx->pp_fog_color );
	OUT_RING( ctx->re_solid_color );
	OUT_RING( ctx->rb3d_blendcntl );
	OUT_RING( ctx->rb3d_depthoffset );
	OUT_RING( ctx->rb3d_depthpitch );
	OUT_RING( ctx->rb3d_zstencilcntl );

	OUT_RING( CP_PACKET0( RADEON_PP_CNTL, 2 ) );
	OUT_RING( ctx->pp_cntl );
	OUT_RING( ctx->rb3d_cntl );
	OUT_RING( ctx->rb3d_coloroffset );

	OUT_RING( CP_PACKET0( RADEON_RB3D_COLORPITCH, 0 ) );
	OUT_RING( ctx->rb3d_colorpitch );

	ADVANCE_RING();
}

static inline void radeon_emit_vertfmt( drm_radeon_private_t *dev_priv,
					drm_radeon_context_regs_t *ctx )
{
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 2 );

	OUT_RING( CP_PACKET0( RADEON_SE_COORD_FMT, 0 ) );
	OUT_RING( ctx->se_coord_fmt );

	ADVANCE_RING();
}

static inline void radeon_emit_line( drm_radeon_private_t *dev_priv,
					drm_radeon_context_regs_t *ctx )
{
	RING_LOCALS;
/*  	printk( "    %s %x %x %x\n", __FUNCTION__,  */
/*  		ctx->re_line_pattern, */
/*  		ctx->re_line_state, */
/*  		ctx->se_line_width); */

	BEGIN_RING( 5 );

	OUT_RING( CP_PACKET0( RADEON_RE_LINE_PATTERN, 1 ) );
	OUT_RING( ctx->re_line_pattern );
	OUT_RING( ctx->re_line_state );

	OUT_RING( CP_PACKET0( RADEON_SE_LINE_WIDTH, 0 ) );
	OUT_RING( ctx->se_line_width );

	ADVANCE_RING();
}

static inline void radeon_emit_bumpmap( drm_radeon_private_t *dev_priv,
					drm_radeon_context_regs_t *ctx )
{
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 5 );

	OUT_RING( CP_PACKET0( RADEON_PP_LUM_MATRIX, 0 ) );
	OUT_RING( ctx->pp_lum_matrix );

	OUT_RING( CP_PACKET0( RADEON_PP_ROT_MATRIX_0, 1 ) );
	OUT_RING( ctx->pp_rot_matrix_0 );
	OUT_RING( ctx->pp_rot_matrix_1 );

	ADVANCE_RING();
}

static inline void radeon_emit_masks( drm_radeon_private_t *dev_priv,
				      drm_radeon_context_regs_t *ctx )
{
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 4 );

	OUT_RING( CP_PACKET0( RADEON_RB3D_STENCILREFMASK, 2 ) );
	OUT_RING( ctx->rb3d_stencilrefmask );
	OUT_RING( ctx->rb3d_ropcntl );
	OUT_RING( ctx->rb3d_planemask );

	ADVANCE_RING();
}

static inline void radeon_emit_viewport( drm_radeon_private_t *dev_priv,
					 drm_radeon_context_regs_t *ctx )
{
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 7 );

	OUT_RING( CP_PACKET0( RADEON_SE_VPORT_XSCALE, 5 ) );
	OUT_RING( ctx->se_vport_xscale );
	OUT_RING( ctx->se_vport_xoffset );
	OUT_RING( ctx->se_vport_yscale );
	OUT_RING( ctx->se_vport_yoffset );
	OUT_RING( ctx->se_vport_zscale );
	OUT_RING( ctx->se_vport_zoffset );
	ADVANCE_RING();
}

static inline void radeon_emit_setup( drm_radeon_private_t *dev_priv,
				      drm_radeon_context_regs_t *ctx )
{
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 4 );

	OUT_RING( CP_PACKET0( RADEON_SE_CNTL, 0 ) );
	OUT_RING( ctx->se_cntl );
	OUT_RING( CP_PACKET0( RADEON_SE_CNTL_STATUS, 0 ) );
	OUT_RING( ctx->se_cntl_status );

	ADVANCE_RING();
}


static inline void radeon_emit_misc( drm_radeon_private_t *dev_priv,
				     drm_radeon_context_regs_t *ctx )
{
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 2 );

	OUT_RING( CP_PACKET0( RADEON_RE_MISC, 0 ) );
	OUT_RING( ctx->re_misc );

	ADVANCE_RING();
}

static inline void radeon_emit_tex0( drm_radeon_private_t *dev_priv,
				     drm_radeon_texture_regs_t *tex )
{
	RING_LOCALS;
	DRM_DEBUG( "    %s: offset=0x%x\n", __FUNCTION__, tex->pp_txoffset );

	BEGIN_RING( 9 );

	OUT_RING( CP_PACKET0( RADEON_PP_TXFILTER_0, 5 ) );
	OUT_RING( tex->pp_txfilter );
	OUT_RING( tex->pp_txformat );
	OUT_RING( tex->pp_txoffset );
	OUT_RING( tex->pp_txcblend );
	OUT_RING( tex->pp_txablend );
	OUT_RING( tex->pp_tfactor );

	OUT_RING( CP_PACKET0( RADEON_PP_BORDER_COLOR_0, 0 ) );
	OUT_RING( tex->pp_border_color );

	ADVANCE_RING();
}

static inline void radeon_emit_tex1( drm_radeon_private_t *dev_priv,
				     drm_radeon_texture_regs_t *tex )
{
	RING_LOCALS;
	DRM_DEBUG( "    %s: offset=0x%x\n", __FUNCTION__, tex->pp_txoffset );

	BEGIN_RING( 9 );

	OUT_RING( CP_PACKET0( RADEON_PP_TXFILTER_1, 5 ) );
	OUT_RING( tex->pp_txfilter );
	OUT_RING( tex->pp_txformat );
	OUT_RING( tex->pp_txoffset );
	OUT_RING( tex->pp_txcblend );
	OUT_RING( tex->pp_txablend );
	OUT_RING( tex->pp_tfactor );

	OUT_RING( CP_PACKET0( RADEON_PP_BORDER_COLOR_1, 0 ) );
	OUT_RING( tex->pp_border_color );

	ADVANCE_RING();
}

static inline void radeon_emit_tex2( drm_radeon_private_t *dev_priv,
				     drm_radeon_texture_regs_t *tex )
{
	RING_LOCALS;
	DRM_DEBUG( "    %s\n", __FUNCTION__ );

	BEGIN_RING( 9 );

	OUT_RING( CP_PACKET0( RADEON_PP_TXFILTER_2, 5 ) );
	OUT_RING( tex->pp_txfilter );
	OUT_RING( tex->pp_txformat );
	OUT_RING( tex->pp_txoffset );
	OUT_RING( tex->pp_txcblend );
	OUT_RING( tex->pp_txablend );
	OUT_RING( tex->pp_tfactor );

	OUT_RING( CP_PACKET0( RADEON_PP_BORDER_COLOR_2, 0 ) );
	OUT_RING( tex->pp_border_color );

	ADVANCE_RING();
}

#if 0
static void radeon_print_dirty( const char *msg, unsigned int flags )
{
	DRM_DEBUG( "%s: (0x%x) %s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		   msg,
		   flags,
		   (flags & RADEON_UPLOAD_CONTEXT)     ? "context, " : "",
		   (flags & RADEON_UPLOAD_VERTFMT)     ? "vertfmt, " : "",
		   (flags & RADEON_UPLOAD_LINE)        ? "line, " : "",
		   (flags & RADEON_UPLOAD_BUMPMAP)     ? "bumpmap, " : "",
		   (flags & RADEON_UPLOAD_MASKS)       ? "masks, " : "",
		   (flags & RADEON_UPLOAD_VIEWPORT)    ? "viewport, " : "",
		   (flags & RADEON_UPLOAD_SETUP)       ? "setup, " : "",
		   (flags & RADEON_UPLOAD_MISC)        ? "misc, " : "",
		   (flags & RADEON_UPLOAD_TEX0)        ? "tex0, " : "",
		   (flags & RADEON_UPLOAD_TEX1)        ? "tex1, " : "",
		   (flags & RADEON_UPLOAD_TEX2)        ? "tex2, " : "",
		   (flags & RADEON_UPLOAD_CLIPRECTS)   ? "cliprects, " : "",
		   (flags & RADEON_REQUIRE_QUIESCENCE) ? "quiescence, " : "" );
}
#endif

static inline void radeon_emit_state( drm_radeon_private_t *dev_priv,
				      drm_radeon_context_regs_t *ctx,
				      drm_radeon_texture_regs_t *tex,
				      unsigned int dirty )
{
	DRM_DEBUG( "%s: dirty=0x%08x\n", __FUNCTION__, dirty );

	if ( dirty & RADEON_UPLOAD_CONTEXT ) {
		radeon_emit_context( dev_priv, ctx );
	}

	if ( dirty & RADEON_UPLOAD_VERTFMT ) {
		radeon_emit_vertfmt( dev_priv, ctx );
	}

	if ( dirty & RADEON_UPLOAD_LINE ) {
		radeon_emit_line( dev_priv, ctx );
	}

	if ( dirty & RADEON_UPLOAD_BUMPMAP ) {
		radeon_emit_bumpmap( dev_priv, ctx );
	}

	if ( dirty & RADEON_UPLOAD_MASKS ) {
		radeon_emit_masks( dev_priv, ctx );
	}

	if ( dirty & RADEON_UPLOAD_VIEWPORT ) {
		radeon_emit_viewport( dev_priv, ctx );
	}

	if ( dirty & RADEON_UPLOAD_SETUP ) {
		radeon_emit_setup( dev_priv, ctx );
	}

	if ( dirty & RADEON_UPLOAD_MISC ) {
		radeon_emit_misc( dev_priv, ctx );
	}

	if ( dirty & RADEON_UPLOAD_TEX0 ) {
		radeon_emit_tex0( dev_priv, &tex[0] );
	}

	if ( dirty & RADEON_UPLOAD_TEX1 ) {
		radeon_emit_tex1( dev_priv, &tex[1] );
	}

	if ( dirty & RADEON_UPLOAD_TEX2 ) {
		radeon_emit_tex2( dev_priv, &tex[2] );
	}
}



static inline void radeon_emit_zbias( drm_radeon_private_t *dev_priv,
				      drm_radeon_context2_regs_t *ctx )
{
	RING_LOCALS;
/*  	printk( "    %s %x %x\n", __FUNCTION__, */
/*  		ctx->se_zbias_factor, */
/*  		ctx->se_zbias_constant ); */

	BEGIN_RING( 3 );
	OUT_RING( CP_PACKET0( RADEON_SE_ZBIAS_FACTOR, 1 ) );
  	OUT_RING( ctx->se_zbias_factor ); 
  	OUT_RING( ctx->se_zbias_constant ); 
	ADVANCE_RING();
}

static inline void radeon_emit_state2( drm_radeon_private_t *dev_priv,
				       drm_radeon_state_t *state )
{
	if (state->dirty & RADEON_UPLOAD_ZBIAS)
		radeon_emit_zbias( dev_priv, &state->context2 );

	radeon_emit_state( dev_priv, &state->context, 
			   state->tex, state->dirty );
}

#if RADEON_PERFORMANCE_BOXES
/* ================================================================
 * Performance monitoring functions
 */

static void radeon_clear_box( drm_radeon_private_t *dev_priv,
			      int x, int y, int w, int h,
			      int r, int g, int b )
{
	u32 pitch, offset;
	u32 color;
	RING_LOCALS;

	switch ( dev_priv->color_fmt ) {
	case RADEON_COLOR_FORMAT_RGB565:
		color = (((r & 0xf8) << 8) |
			 ((g & 0xfc) << 3) |
			 ((b & 0xf8) >> 3));
		break;
	case RADEON_COLOR_FORMAT_ARGB8888:
	default:
		color = (((0xff) << 24) | (r << 16) | (g <<  8) | b);
		break;
	}

	offset = dev_priv->back_offset;
	pitch = dev_priv->back_pitch >> 3;

	BEGIN_RING( 6 );

	OUT_RING( CP_PACKET3( RADEON_CNTL_PAINT_MULTI, 4 ) );
	OUT_RING( RADEON_GMC_DST_PITCH_OFFSET_CNTL |
		  RADEON_GMC_BRUSH_SOLID_COLOR |
		  (dev_priv->color_fmt << 8) |
		  RADEON_GMC_SRC_DATATYPE_COLOR |
		  RADEON_ROP3_P |
		  RADEON_GMC_CLR_CMP_CNTL_DIS );

	OUT_RING( (pitch << 22) | (offset >> 5) );
	OUT_RING( color );

	OUT_RING( (x << 16) | y );
	OUT_RING( (w << 16) | h );

	ADVANCE_RING();
}

static void radeon_cp_performance_boxes( drm_radeon_private_t *dev_priv )
{
	if ( atomic_read( &dev_priv->idle_count ) == 0 ) {
		radeon_clear_box( dev_priv, 64, 4, 8, 8, 0, 255, 0 );
	} else {
		atomic_set( &dev_priv->idle_count, 0 );
	}
}

#endif


/* ================================================================
 * CP command dispatch functions
 */

static void radeon_cp_dispatch_clear( drm_device_t *dev,
				      drm_radeon_clear_t *clear,
				      drm_radeon_clear_rect_t *depth_boxes )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_depth_clear_t *depth_clear = &dev_priv->depth_clear;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	unsigned int flags = clear->flags;
	u32 rb3d_cntl = 0, rb3d_stencilrefmask= 0;
	int i;
	RING_LOCALS;
	DRM_DEBUG( __FUNCTION__": flags = 0x%x\n", flags );

	if ( dev_priv->page_flipping && dev_priv->current_page == 1 ) {
		unsigned int tmp = flags;

		flags &= ~(RADEON_FRONT | RADEON_BACK);
		if ( tmp & RADEON_FRONT ) flags |= RADEON_BACK;
		if ( tmp & RADEON_BACK )  flags |= RADEON_FRONT;
	}

	/* We have to clear the depth and/or stencil buffers by
	 * rendering a quad into just those buffers.  Thus, we have to
	 * make sure the 3D engine is configured correctly.
	 */
	if ( flags & (RADEON_DEPTH | RADEON_STENCIL) ) {
		rb3d_cntl = depth_clear->rb3d_cntl;

		if ( flags & RADEON_DEPTH ) {
			rb3d_cntl |=  RADEON_Z_ENABLE;
		} else {
			rb3d_cntl &= ~RADEON_Z_ENABLE;
		}

		if ( flags & RADEON_STENCIL ) {
			rb3d_cntl |=  RADEON_STENCIL_ENABLE;
			rb3d_stencilrefmask = clear->depth_mask; /* misnamed field */
		} else {
			rb3d_cntl &= ~RADEON_STENCIL_ENABLE;
			rb3d_stencilrefmask = 0x00000000;
		}
	}

	for ( i = 0 ; i < nbox ; i++ ) {
		int x = pbox[i].x1;
		int y = pbox[i].y1;
		int w = pbox[i].x2 - x;
		int h = pbox[i].y2 - y;

		DRM_DEBUG( "dispatch clear %d,%d-%d,%d flags 0x%x\n",
			   x, y, w, h, flags );

		if ( flags & (RADEON_FRONT | RADEON_BACK) ) {
			BEGIN_RING( 4 );

			/* Ensure the 3D stream is idle before doing a
			 * 2D fill to clear the front or back buffer.
			 */
			RADEON_WAIT_UNTIL_3D_IDLE();

			OUT_RING( CP_PACKET0( RADEON_DP_WRITE_MASK, 0 ) );
			OUT_RING( clear->color_mask );

			ADVANCE_RING();

			/* Make sure we restore the 3D state next time.
			 */
			dev_priv->sarea_priv->ctx_owner = 0;
		}

		if ( flags & RADEON_FRONT ) {
			BEGIN_RING( 6 );

			OUT_RING( CP_PACKET3( RADEON_CNTL_PAINT_MULTI, 4 ) );
			OUT_RING( RADEON_GMC_DST_PITCH_OFFSET_CNTL |
				  RADEON_GMC_BRUSH_SOLID_COLOR |
				  (dev_priv->color_fmt << 8) |
				  RADEON_GMC_SRC_DATATYPE_COLOR |
				  RADEON_ROP3_P |
				  RADEON_GMC_CLR_CMP_CNTL_DIS );

			OUT_RING( dev_priv->front_pitch_offset );
			OUT_RING( clear->clear_color );

			OUT_RING( (x << 16) | y );
			OUT_RING( (w << 16) | h );

			ADVANCE_RING();
		}

		if ( flags & RADEON_BACK ) {
			BEGIN_RING( 6 );

			OUT_RING( CP_PACKET3( RADEON_CNTL_PAINT_MULTI, 4 ) );
			OUT_RING( RADEON_GMC_DST_PITCH_OFFSET_CNTL |
				  RADEON_GMC_BRUSH_SOLID_COLOR |
				  (dev_priv->color_fmt << 8) |
				  RADEON_GMC_SRC_DATATYPE_COLOR |
				  RADEON_ROP3_P |
				  RADEON_GMC_CLR_CMP_CNTL_DIS );

			OUT_RING( dev_priv->back_pitch_offset );
			OUT_RING( clear->clear_color );

			OUT_RING( (x << 16) | y );
			OUT_RING( (w << 16) | h );

			ADVANCE_RING();
		}

		if ( flags & (RADEON_DEPTH | RADEON_STENCIL) ) {

			radeon_emit_clip_rect( dev_priv,
					       &sarea_priv->boxes[i] );

			BEGIN_RING( 25 );

			RADEON_WAIT_UNTIL_2D_IDLE();

			OUT_RING( CP_PACKET0( RADEON_PP_CNTL, 1 ) );
			OUT_RING( 0x00000000 );
			OUT_RING( rb3d_cntl );

			OUT_RING_REG( RADEON_RB3D_ZSTENCILCNTL,
				      depth_clear->rb3d_zstencilcntl );
			OUT_RING_REG( RADEON_RB3D_STENCILREFMASK,
				      rb3d_stencilrefmask );
			OUT_RING_REG( RADEON_RB3D_PLANEMASK,
				      0x00000000 );
			OUT_RING_REG( RADEON_SE_CNTL,
				      depth_clear->se_cntl );

			OUT_RING( CP_PACKET3( RADEON_3D_DRAW_IMMD, 10 ) );
			OUT_RING( RADEON_VTX_Z_PRESENT );
			OUT_RING( (RADEON_PRIM_TYPE_RECT_LIST |
				   RADEON_PRIM_WALK_RING |
				   RADEON_MAOS_ENABLE |
				   RADEON_VTX_FMT_RADEON_MODE |
				   (3 << RADEON_NUM_VERTICES_SHIFT)) );

/*  			printk( "depth box %d: %x %x %x %x\n",  */
/*  				i, */
/*  				depth_boxes[i].ui[CLEAR_X1], */
/*  				depth_boxes[i].ui[CLEAR_Y1], */
/*  				depth_boxes[i].ui[CLEAR_X2], */
/*  				depth_boxes[i].ui[CLEAR_Y2]); */

			OUT_RING( depth_boxes[i].ui[CLEAR_X1] );
			OUT_RING( depth_boxes[i].ui[CLEAR_Y1] );
			OUT_RING( depth_boxes[i].ui[CLEAR_DEPTH] );

			OUT_RING( depth_boxes[i].ui[CLEAR_X1] );
			OUT_RING( depth_boxes[i].ui[CLEAR_Y2] );
			OUT_RING( depth_boxes[i].ui[CLEAR_DEPTH] );

			OUT_RING( depth_boxes[i].ui[CLEAR_X2] );
			OUT_RING( depth_boxes[i].ui[CLEAR_Y2] );
			OUT_RING( depth_boxes[i].ui[CLEAR_DEPTH] );

			ADVANCE_RING();

			/* Make sure we restore the 3D state next time.
			 */
			dev_priv->sarea_priv->ctx_owner = 0;
		}
	}

	/* Increment the clear counter.  The client-side 3D driver must
	 * wait on this value before performing the clear ioctl.  We
	 * need this because the card's so damned fast...
	 */
	dev_priv->sarea_priv->last_clear++;

	BEGIN_RING( 4 );

	RADEON_CLEAR_AGE( dev_priv->sarea_priv->last_clear );
	RADEON_WAIT_UNTIL_IDLE();

	ADVANCE_RING();
}

static void radeon_cp_dispatch_swap( drm_device_t *dev )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int nbox = sarea_priv->nbox;
	drm_clip_rect_t *pbox = sarea_priv->boxes;
	int i;
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

#if RADEON_PERFORMANCE_BOXES
	/* Do some trivial performance monitoring...
	 */
	radeon_cp_performance_boxes( dev_priv );
#endif

	/* Wait for the 3D stream to idle before dispatching the bitblt.
	 * This will prevent data corruption between the two streams.
	 */
	BEGIN_RING( 2 );

	RADEON_WAIT_UNTIL_3D_IDLE();

	ADVANCE_RING();

	for ( i = 0 ; i < nbox ; i++ ) {
		int x = pbox[i].x1;
		int y = pbox[i].y1;
		int w = pbox[i].x2 - x;
		int h = pbox[i].y2 - y;

		DRM_DEBUG( "dispatch swap %d,%d-%d,%d\n",
			   x, y, w, h );

		BEGIN_RING( 7 );

		OUT_RING( CP_PACKET3( RADEON_CNTL_BITBLT_MULTI, 5 ) );
		OUT_RING( RADEON_GMC_SRC_PITCH_OFFSET_CNTL |
			  RADEON_GMC_DST_PITCH_OFFSET_CNTL |
			  RADEON_GMC_BRUSH_NONE |
			  (dev_priv->color_fmt << 8) |
			  RADEON_GMC_SRC_DATATYPE_COLOR |
			  RADEON_ROP3_S |
			  RADEON_DP_SRC_SOURCE_MEMORY |
			  RADEON_GMC_CLR_CMP_CNTL_DIS |
			  RADEON_GMC_WR_MSK_DIS );

		OUT_RING( dev_priv->back_pitch_offset );
		OUT_RING( dev_priv->front_pitch_offset );

		OUT_RING( (x << 16) | y );
		OUT_RING( (x << 16) | y );
		OUT_RING( (w << 16) | h );

		ADVANCE_RING();
	}

	/* Increment the frame counter.  The client-side 3D driver must
	 * throttle the framerate by waiting for this value before
	 * performing the swapbuffer ioctl.
	 */
	dev_priv->sarea_priv->last_frame++;

	BEGIN_RING( 4 );

	RADEON_FRAME_AGE( dev_priv->sarea_priv->last_frame );
	RADEON_WAIT_UNTIL_2D_IDLE();

	ADVANCE_RING();
}

static void radeon_cp_dispatch_flip( drm_device_t *dev )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;
	DRM_DEBUG( "%s: page=%d\n", __FUNCTION__, dev_priv->current_page );

#if RADEON_PERFORMANCE_BOXES
	/* Do some trivial performance monitoring...
	 */
	radeon_cp_performance_boxes( dev_priv );
#endif

	BEGIN_RING( 6 );

	RADEON_WAIT_UNTIL_3D_IDLE();
	RADEON_WAIT_UNTIL_PAGE_FLIPPED();

	OUT_RING( CP_PACKET0( RADEON_CRTC_OFFSET, 0 ) );

	if ( dev_priv->current_page == 0 ) {
		OUT_RING( dev_priv->back_offset );
		dev_priv->current_page = 1;
	} else {
		OUT_RING( dev_priv->front_offset );
		dev_priv->current_page = 0;
	}

	ADVANCE_RING();

	/* Increment the frame counter.  The client-side 3D driver must
	 * throttle the framerate by waiting for this value before
	 * performing the swapbuffer ioctl.
	 */
	dev_priv->sarea_priv->last_frame++;

	BEGIN_RING( 2 );

	RADEON_FRAME_AGE( dev_priv->sarea_priv->last_frame );

	ADVANCE_RING();
}


static void radeon_cp_dispatch_vertex( drm_device_t *dev,
				       drm_buf_t *buf,
				       drm_radeon_prim_t *prim )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
	int offset = dev_priv->agp_buffers_offset + buf->offset + prim->start;
	int numverts = (int)prim->numverts;
	int i = 0;
	RING_LOCALS;

	DRM_DEBUG( __FUNCTION__": nbox=%d %d..%d prim %x nvert %d\n",
		   sarea_priv->nbox, prim->start, prim->finish,
		   prim->prim, numverts );

	switch (prim->prim & RADEON_PRIM_TYPE_MASK) {
	case RADEON_PRIM_TYPE_NONE:
	case RADEON_PRIM_TYPE_POINT:
		if (prim->numverts < 1) {
			DRM_ERROR( "Bad nr verts for line %d\n",
				   prim->numverts);
			return;
		}
		break;
	case RADEON_PRIM_TYPE_LINE:
		if ((prim->numverts & 1) || prim->numverts == 0) {
			DRM_ERROR( "Bad nr verts for line %d\n",
				   prim->numverts);
			return;
		}
		break;
	case RADEON_PRIM_TYPE_LINE_STRIP:
		if (prim->numverts < 2) {
			DRM_ERROR( "Bad nr verts for line_strip %d\n",
				   prim->numverts);
			return;
		}
		break;
	case RADEON_PRIM_TYPE_TRI_LIST:
	case RADEON_PRIM_TYPE_3VRT_POINT_LIST:
	case RADEON_PRIM_TYPE_3VRT_LINE_LIST:
	case RADEON_PRIM_TYPE_RECT_LIST:
		if (prim->numverts % 3 || prim->numverts == 0) {
			DRM_ERROR( "Bad nr verts for tri %d\n",
				   prim->numverts);
			return;
		}
		break;
	case RADEON_PRIM_TYPE_TRI_FAN:
	case RADEON_PRIM_TYPE_TRI_STRIP:
		if (prim->numverts < 3) {
			DRM_ERROR( "Bad nr verts for strip/fan %d\n",
				   prim->numverts);
			return;
		}
		break;
	default:
		DRM_ERROR( "buffer prim %x start %x\n", 
			   prim->prim, prim->start );
		return;
	}	


	buf_priv->dispatched = 1;

	do {
		/* Emit the next cliprect */
		if ( i < sarea_priv->nbox ) {
			radeon_emit_clip_rect( dev_priv,
					       &sarea_priv->boxes[i] );
		}

		/* Emit the vertex buffer rendering commands */
		BEGIN_RING( 5 );

		OUT_RING( CP_PACKET3( RADEON_3D_RNDR_GEN_INDX_PRIM, 3 ) );
		OUT_RING( offset );
		OUT_RING( numverts );
		OUT_RING( prim->vc_format );
		OUT_RING( prim->prim | RADEON_PRIM_WALK_LIST |
			  RADEON_COLOR_ORDER_RGBA |
			  RADEON_VTX_FMT_RADEON_MODE |
			  (numverts << RADEON_NUM_VERTICES_SHIFT) );

		ADVANCE_RING();

		i++;
	} while ( i < sarea_priv->nbox );

	dev_priv->sarea_priv->last_dispatch++;
}


static void radeon_cp_discard_buffer( drm_device_t *dev, drm_buf_t *buf )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
	RING_LOCALS;

	buf_priv->age = dev_priv->sarea_priv->last_dispatch;

	/* Emit the vertex buffer age */
	BEGIN_RING( 2 );
	RADEON_DISPATCH_AGE( buf_priv->age );
	ADVANCE_RING();

	buf->pending = 1;
	buf->used = 0;
	/* FIXME: Check dispatched field */
	buf_priv->dispatched = 0;
}

static void radeon_cp_dispatch_indirect( drm_device_t *dev,
					 drm_buf_t *buf,
					 int start, int end )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
	RING_LOCALS;
	DRM_DEBUG( "indirect: buf=%d s=0x%x e=0x%x\n",
		   buf->idx, start, end );

	if ( start != end ) {
		int offset = (dev_priv->agp_buffers_offset
			      + buf->offset + start);
		int dwords = (end - start + 3) / sizeof(u32);

		/* Indirect buffer data must be an even number of
		 * dwords, so if we've been given an odd number we must
		 * pad the data with a Type-2 CP packet.
		 */
		if ( dwords & 1 ) {
			u32 *data = (u32 *)
				((char *)dev_priv->buffers->handle
				 + buf->offset + start);
			data[dwords++] = RADEON_CP_PACKET2;
		}

		buf_priv->dispatched = 1;

		/* Fire off the indirect buffer */
		BEGIN_RING( 3 );

		OUT_RING( CP_PACKET0( RADEON_CP_IB_BASE, 1 ) );
		OUT_RING( offset );
		OUT_RING( dwords );

		ADVANCE_RING();
	}

	dev_priv->sarea_priv->last_dispatch++;
}

static void radeon_cp_dispatch_indices( drm_device_t *dev,
					drm_buf_t *elt_buf,
					drm_radeon_prim_t *prim )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_buf_priv_t *buf_priv = elt_buf->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	int offset = dev_priv->agp_buffers_offset + prim->numverts * 64;
	u32 *data;
	int dwords;
	int i = 0;
	int start = prim->start + RADEON_INDEX_PRIM_OFFSET;
	int count = (prim->finish - start) / sizeof(u16);

  	DRM_DEBUG( "indices: start=%x/%x end=%x count=%d nv %d offset %x\n",
		   prim->start, start, prim->finish,
		   count, prim->numverts, offset );

	switch (prim->prim & RADEON_PRIM_TYPE_MASK) {
	case RADEON_PRIM_TYPE_NONE:
	case RADEON_PRIM_TYPE_POINT:
		if (count < 1) {
			DRM_ERROR( "Bad nr verts %d\n",
				   count);
			return;
		}
		break;
	case RADEON_PRIM_TYPE_LINE:
		if ((count & 1) || count == 0) {
			DRM_ERROR( "Bad nr verts for line %d\n",
				   count);
			return;
		}
		break;
	case RADEON_PRIM_TYPE_LINE_STRIP:
		if (count < 2) {
			DRM_ERROR( "Bad nr verts for line_strip %d\n",
				   count);
			return;
		}
		break;
	case RADEON_PRIM_TYPE_TRI_LIST:
	case RADEON_PRIM_TYPE_3VRT_POINT_LIST:
	case RADEON_PRIM_TYPE_3VRT_LINE_LIST:
	case RADEON_PRIM_TYPE_RECT_LIST:
		if (count % 3 || count == 0) {
			DRM_ERROR( "Bad nr verts for tri %d\n", count);
			return;
		}
		break;
	case RADEON_PRIM_TYPE_TRI_FAN:
	case RADEON_PRIM_TYPE_TRI_STRIP:
		if (count < 3) {
			DRM_ERROR( "Bad nr verts for strip/fan %d\n", count);
			return;
		}
		break;
	default:
		DRM_ERROR( "buffer prim %x start %x\n", 
			   prim->prim, prim->start );
		return;
	}	

	if ( start < prim->finish ) {
		buf_priv->dispatched = 1;

		dwords = (prim->finish - prim->start + 3) / sizeof(u32);

		data = (u32 *)((char *)dev_priv->buffers->handle +
			       elt_buf->offset + prim->start);

		data[0] = CP_PACKET3( RADEON_3D_RNDR_GEN_INDX_PRIM, dwords-2 );
		data[1] = offset;
		data[2] = RADEON_MAX_VB_VERTS;
		data[3] = prim->vc_format;
		data[4] = (prim->prim |
			   RADEON_PRIM_WALK_IND |
			   RADEON_COLOR_ORDER_RGBA |
			   RADEON_VTX_FMT_RADEON_MODE |
			   (count << RADEON_NUM_VERTICES_SHIFT) );

		if ( count & 0x1 ) {
			/* unnecessary? */
			data[dwords-1] &= 0x0000ffff;
		}

		do {
			/* Emit the next set of up to three cliprects */
			if ( i < sarea_priv->nbox ) {
				radeon_emit_clip_rect( dev_priv,
						       &sarea_priv->boxes[i] );
			}

			radeon_cp_dispatch_indirect( dev, elt_buf,
						     prim->start,
						     prim->finish );

			i++;
		} while ( i < sarea_priv->nbox );
	}

	sarea_priv->last_dispatch++;
}

#define RADEON_MAX_TEXTURE_SIZE (RADEON_BUFFER_SIZE - 8 * sizeof(u32))

static int radeon_cp_dispatch_texture( drm_device_t *dev,
				       drm_radeon_texture_t *tex,
				       drm_radeon_tex_image_t *image )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_buf_t *buf;
	drm_radeon_buf_priv_t *buf_priv;
	u32 format;
	u32 *buffer;
	u8 *data;
	int size, dwords, tex_width, blit_width;
	u32 y, height;
	int ret = 0, i;
	RING_LOCALS;

	/* FIXME: Be smarter about this...
	 */
	buf = radeon_freelist_get( dev );
	if ( !buf ) return -EAGAIN;

	DRM_DEBUG( "tex: ofs=0x%x p=%d f=%d x=%hd y=%hd w=%hd h=%hd\n",
		   tex->offset >> 10, tex->pitch, tex->format,
		   image->x, image->y, image->width, image->height );

	buf_priv = buf->dev_private;

	/* The compiler won't optimize away a division by a variable,
	 * even if the only legal values are powers of two.  Thus, we'll
	 * use a shift instead.
	 */
	switch ( tex->format ) {
	case RADEON_TXFORMAT_ARGB8888:
	case RADEON_TXFORMAT_RGBA8888:
		format = RADEON_COLOR_FORMAT_ARGB8888;
		tex_width = tex->width * 4;
		blit_width = image->width * 4;
		break;
	case RADEON_TXFORMAT_AI88:
	case RADEON_TXFORMAT_ARGB1555:
	case RADEON_TXFORMAT_RGB565:
	case RADEON_TXFORMAT_ARGB4444:
		format = RADEON_COLOR_FORMAT_RGB565;
		tex_width = tex->width * 2;
		blit_width = image->width * 2;
		break;
	case RADEON_TXFORMAT_I8:
	case RADEON_TXFORMAT_RGB332:
		format = RADEON_COLOR_FORMAT_CI8;
		tex_width = tex->width * 1;
		blit_width = image->width * 1;
		break;
	default:
		DRM_ERROR( "invalid texture format %d\n", tex->format );
		return -EINVAL;
	}

	DRM_DEBUG( "   tex=%dx%d  blit=%d\n",
		   tex_width, tex->height, blit_width );

	/* Flush the pixel cache.  This ensures no pixel data gets mixed
	 * up with the texture data from the host data blit, otherwise
	 * part of the texture image may be corrupted.
	 */
	BEGIN_RING( 4 );

	RADEON_FLUSH_CACHE();
	RADEON_WAIT_UNTIL_IDLE();

	ADVANCE_RING();

	/* Make a copy of the parameters in case we have to update them
	 * for a multi-pass texture blit.
	 */
	y = image->y;
	height = image->height;
	data = (u8 *)image->data;

	size = height * blit_width;

	if ( size > RADEON_MAX_TEXTURE_SIZE ) {
		/* Texture image is too large, do a multipass upload */
		ret = -EAGAIN;

		/* Adjust the blit size to fit the indirect buffer */
		height = RADEON_MAX_TEXTURE_SIZE / blit_width;
		size = height * blit_width;

		/* Update the input parameters for next time */
		image->y += height;
		image->height -= height;
		image->data = (char *)image->data + size;

		if ( copy_to_user( tex->image, image, sizeof(*image) ) ) {
			DRM_ERROR( "EFAULT on tex->image\n" );
			return -EFAULT;
		}
	} else if ( size < 4 && size > 0 ) {
		size = 4;
	}

	dwords = size / 4;

	/* Dispatch the indirect buffer.
	 */
	buffer = (u32 *)((char *)dev_priv->buffers->handle + buf->offset);

	buffer[0] = CP_PACKET3( RADEON_CNTL_HOSTDATA_BLT, dwords + 6 );
	buffer[1] = (RADEON_GMC_DST_PITCH_OFFSET_CNTL |
		     RADEON_GMC_BRUSH_NONE |
		     (format << 8) |
		     RADEON_GMC_SRC_DATATYPE_COLOR |
		     RADEON_ROP3_S |
		     RADEON_DP_SRC_SOURCE_HOST_DATA |
		     RADEON_GMC_CLR_CMP_CNTL_DIS |
		     RADEON_GMC_WR_MSK_DIS);

	buffer[2] = (tex->pitch << 22) | (tex->offset >> 10);
	buffer[3] = 0xffffffff;
	buffer[4] = 0xffffffff;
	buffer[5] = (y << 16) | image->x;
	buffer[6] = (height << 16) | image->width;
	buffer[7] = dwords;

	buffer += 8;

	if ( tex_width >= 32 ) {
		/* Texture image width is larger than the minimum, so we
		 * can upload it directly.
		 */
		if ( copy_from_user( buffer, data, dwords * sizeof(u32) ) ) {
			DRM_ERROR( "EFAULT on data, %d dwords\n", dwords );
			return -EFAULT;
		}
	} else {
		/* Texture image width is less than the minimum, so we
		 * need to pad out each image scanline to the minimum
		 * width.
		 */
		for ( i = 0 ; i < tex->height ; i++ ) {
			if ( copy_from_user( buffer, data, tex_width ) ) {
				DRM_ERROR( "EFAULT on pad, %d bytes\n",
					   tex_width );
				return -EFAULT;
			}
			buffer += 8;
			data += tex_width;
		}
	}

	buf->pid = current->pid;
	buf->used = (dwords + 8) * sizeof(u32);
	buf_priv->discard = 1;

	radeon_cp_dispatch_indirect( dev, buf, 0, buf->used );
	radeon_cp_discard_buffer( dev, buf );

	/* Flush the pixel cache after the blit completes.  This ensures
	 * the texture data is written out to memory before rendering
	 * continues.
	 */
	BEGIN_RING( 4 );

	RADEON_FLUSH_CACHE();
	RADEON_WAIT_UNTIL_2D_IDLE();

	ADVANCE_RING();

	return ret;
}

static void radeon_cp_dispatch_stipple( drm_device_t *dev, u32 *stipple )
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int i;
	RING_LOCALS;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	BEGIN_RING( 35 );

	OUT_RING( CP_PACKET0( RADEON_RE_STIPPLE_ADDR, 0 ) );
	OUT_RING( 0x00000000 );

	OUT_RING( CP_PACKET0_TABLE( RADEON_RE_STIPPLE_DATA, 31 ) );
	for ( i = 0 ; i < 32 ; i++ ) {
		OUT_RING( stipple[i] );
	}

	ADVANCE_RING();
}


/* ================================================================
 * IOCTL functions
 */

int radeon_cp_clear( struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_radeon_clear_t clear;
	drm_radeon_clear_rect_t depth_boxes[RADEON_NR_SAREA_CLIPRECTS];
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	LOCK_TEST_WITH_RETURN( dev );

	if ( copy_from_user( &clear, (drm_radeon_clear_t *)arg,
			     sizeof(clear) ) )
		return -EFAULT;

	RING_SPACE_TEST_WITH_RETURN( dev_priv );

	if ( sarea_priv->nbox > RADEON_NR_SAREA_CLIPRECTS )
		sarea_priv->nbox = RADEON_NR_SAREA_CLIPRECTS;

	if ( copy_from_user( &depth_boxes, clear.depth_boxes,
			     sarea_priv->nbox * sizeof(depth_boxes[0]) ) )
		return -EFAULT;

	/* Needed for depth clears via triangles???
	 */
	if ( sarea_priv->dirty & ~RADEON_UPLOAD_CLIPRECTS ) {
		radeon_emit_state( dev_priv,
				   &sarea_priv->context_state,
				   sarea_priv->tex_state,
				   sarea_priv->dirty );

		sarea_priv->dirty &= ~(RADEON_UPLOAD_TEX0IMAGES |
				       RADEON_UPLOAD_TEX1IMAGES |
				       RADEON_UPLOAD_TEX2IMAGES |
				       RADEON_REQUIRE_QUIESCENCE);
	}

	radeon_cp_dispatch_clear( dev, &clear, depth_boxes );

	return 0;
}

int radeon_cp_swap( struct inode *inode, struct file *filp,
		    unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	DRM_DEBUG( "%s\n", __FUNCTION__ );

	LOCK_TEST_WITH_RETURN( dev );

	RING_SPACE_TEST_WITH_RETURN( dev_priv );

	if ( sarea_priv->nbox > RADEON_NR_SAREA_CLIPRECTS )
		sarea_priv->nbox = RADEON_NR_SAREA_CLIPRECTS;

	if ( !dev_priv->page_flipping ) {
		radeon_cp_dispatch_swap( dev );
		dev_priv->sarea_priv->ctx_owner = 0;
	} else {
		radeon_cp_dispatch_flip( dev );
	}

	return 0;
}

int radeon_cp_vertex( struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_buf_priv_t *buf_priv;
	drm_radeon_vertex_t vertex;
	drm_radeon_prim_t prim;

	LOCK_TEST_WITH_RETURN( dev );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( copy_from_user( &vertex, (drm_radeon_vertex_t *)arg,
			     sizeof(vertex) ) )
		return -EFAULT;

	DRM_DEBUG( "%s: pid=%d index=%d count=%d discard=%d\n",
		   __FUNCTION__, current->pid,
		   vertex.idx, vertex.count, vertex.discard );

	if ( vertex.idx < 0 || vertex.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   vertex.idx, dma->buf_count - 1 );
		return -EINVAL;
	}
	if ( vertex.prim < 0 ||
	     vertex.prim > RADEON_PRIM_TYPE_3VRT_LINE_LIST ) {
		DRM_ERROR( "buffer prim %d\n", vertex.prim );
		return -EINVAL;
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	buf = dma->buflist[vertex.idx];
	buf_priv = buf->dev_private;

	if ( buf->pid != current->pid ) {
		DRM_ERROR( "process %d using buffer owned by %d\n",
			   current->pid, buf->pid );
		return -EINVAL;
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", vertex.idx );
		return -EINVAL;
	}

	buf->used = vertex.count; /* not used? */

	if (vertex.count) {
		if ( sarea_priv->dirty & ~RADEON_UPLOAD_CLIPRECTS ) {
			radeon_emit_state( dev_priv,
					   &sarea_priv->context_state,
					   sarea_priv->tex_state,
					   sarea_priv->dirty );
			
			sarea_priv->dirty &= ~(RADEON_UPLOAD_TEX0IMAGES |
					       RADEON_UPLOAD_TEX1IMAGES |
					       RADEON_UPLOAD_TEX2IMAGES |
					       RADEON_REQUIRE_QUIESCENCE);
		}

		/* Build up a prim_t record:
		 */
		prim.start = 0;
		prim.finish = vertex.count; /* unused */
		prim.prim = vertex.prim;
		prim.stateidx = 0xff;	/* unused */
		prim.numverts = vertex.count;
		prim.vc_format = dev_priv->sarea_priv->vc_format;
		
		radeon_cp_dispatch_vertex( dev, buf, &prim );
	}

	if (vertex.discard) {
		radeon_cp_discard_buffer( dev, buf );
	}

	return 0;
}

int radeon_cp_indices( struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_sarea_t *sarea_priv = dev_priv->sarea_priv;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_buf_priv_t *buf_priv;
	drm_radeon_indices_t elts;
	drm_radeon_prim_t prim;
	int count;

	LOCK_TEST_WITH_RETURN( dev );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( copy_from_user( &elts, (drm_radeon_indices_t *)arg,
			     sizeof(elts) ) )
		return -EFAULT;

	DRM_DEBUG( "%s: pid=%d index=%d start=%d end=%d discard=%d\n",
		   __FUNCTION__, current->pid,
		   elts.idx, elts.start, elts.end, elts.discard );

	if ( elts.idx < 0 || elts.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   elts.idx, dma->buf_count - 1 );
		return -EINVAL;
	}
	if ( elts.prim < 0 ||
	     elts.prim > RADEON_PRIM_TYPE_3VRT_LINE_LIST ) {
		DRM_ERROR( "buffer prim %d\n", elts.prim );
		return -EINVAL;
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	buf = dma->buflist[elts.idx];
	buf_priv = buf->dev_private;

	if ( buf->pid != current->pid ) {
		DRM_ERROR( "process %d using buffer owned by %d\n",
			   current->pid, buf->pid );
		return -EINVAL;
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", elts.idx );
		return -EINVAL;
	}

	count = (elts.end - elts.start) / sizeof(u16);
	elts.start -= RADEON_INDEX_PRIM_OFFSET;

	if ( elts.start & 0x7 ) {
		DRM_ERROR( "misaligned buffer 0x%x\n", elts.start );
		return -EINVAL;
	}
	if ( elts.start < buf->used ) {
		DRM_ERROR( "no header 0x%x - 0x%x\n", elts.start, buf->used );
		return -EINVAL;
	}

	buf->used = elts.end;

	if ( sarea_priv->dirty & ~RADEON_UPLOAD_CLIPRECTS ) {
		radeon_emit_state( dev_priv,
				   &sarea_priv->context_state,
				   sarea_priv->tex_state,
				   sarea_priv->dirty );

		sarea_priv->dirty &= ~(RADEON_UPLOAD_TEX0IMAGES |
				       RADEON_UPLOAD_TEX1IMAGES |
				       RADEON_UPLOAD_TEX2IMAGES |
				       RADEON_REQUIRE_QUIESCENCE);
	}


	/* Build up a prim_t record:
	 */
	prim.start = elts.start;
	prim.finish = elts.end; 
	prim.prim = elts.prim;
	prim.stateidx = 0xff;	/* unused */
	prim.numverts = 0;	/* indexed from start of dma area */
	prim.vc_format = dev_priv->sarea_priv->vc_format;
	
	radeon_cp_dispatch_indices( dev, buf, &prim );
	if (elts.discard) {
	   radeon_cp_discard_buffer( dev, buf );
	}

	return 0;
}

int radeon_cp_texture( struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_texture_t tex;
	drm_radeon_tex_image_t image;

	LOCK_TEST_WITH_RETURN( dev );

	if ( copy_from_user( &tex, (drm_radeon_texture_t *)arg, sizeof(tex) ) )
		return -EFAULT;

	if ( tex.image == NULL ) {
		DRM_ERROR( "null texture image!\n" );
		return -EINVAL;
	}

	if ( copy_from_user( &image,
			     (drm_radeon_tex_image_t *)tex.image,
			     sizeof(image) ) )
		return -EFAULT;

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	return radeon_cp_dispatch_texture( dev, &tex, &image );
}

int radeon_cp_stipple( struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_stipple_t stipple;
	u32 mask[32];

	LOCK_TEST_WITH_RETURN( dev );

	if ( copy_from_user( &stipple, (drm_radeon_stipple_t *)arg,
			     sizeof(stipple) ) )
		return -EFAULT;

	if ( copy_from_user( &mask, stipple.mask, 32 * sizeof(u32) ) )
		return -EFAULT;

	RING_SPACE_TEST_WITH_RETURN( dev_priv );

	radeon_cp_dispatch_stipple( dev, mask );

	return 0;
}

int radeon_cp_indirect( struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_buf_priv_t *buf_priv;
	drm_radeon_indirect_t indirect;
	RING_LOCALS;

	LOCK_TEST_WITH_RETURN( dev );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( copy_from_user( &indirect, (drm_radeon_indirect_t *)arg,
			     sizeof(indirect) ) )
		return -EFAULT;

	DRM_DEBUG( "indirect: idx=%d s=%d e=%d d=%d\n",
		   indirect.idx, indirect.start,
		   indirect.end, indirect.discard );

	if ( indirect.idx < 0 || indirect.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   indirect.idx, dma->buf_count - 1 );
		return -EINVAL;
	}

	buf = dma->buflist[indirect.idx];
	buf_priv = buf->dev_private;

	if ( buf->pid != current->pid ) {
		DRM_ERROR( "process %d using buffer owned by %d\n",
			   current->pid, buf->pid );
		return -EINVAL;
	}
	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", indirect.idx );
		return -EINVAL;
	}

	if ( indirect.start < buf->used ) {
		DRM_ERROR( "reusing indirect: start=0x%x actual=0x%x\n",
			   indirect.start, buf->used );
		return -EINVAL;
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	buf->used = indirect.end;
	buf_priv->discard = indirect.discard;

	/* Wait for the 3D stream to idle before the indirect buffer
	 * containing 2D acceleration commands is processed.
	 */
	BEGIN_RING( 2 );

	RADEON_WAIT_UNTIL_3D_IDLE();

	ADVANCE_RING();

	/* Dispatch the indirect buffer full of commands from the
	 * X server.  This is insecure and is thus only available to
	 * privileged clients.
	 */
	radeon_cp_dispatch_indirect( dev, buf, indirect.start, indirect.end );
	if (indirect.discard) {
	   radeon_cp_discard_buffer( dev, buf );
	}


	return 0;
}

int radeon_cp_vertex2( struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg )
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf;
	drm_radeon_buf_priv_t *buf_priv;
	drm_radeon_vertex2_t vertex;
	int i;
	unsigned char laststate;

	LOCK_TEST_WITH_RETURN( dev );

	if ( !dev_priv ) {
		DRM_ERROR( "%s called with no initialization\n", __FUNCTION__ );
		return -EINVAL;
	}

	if ( copy_from_user( &vertex, (drm_radeon_vertex_t *)arg,
			     sizeof(vertex) ) )
		return -EFAULT;

	DRM_DEBUG( __FUNCTION__": pid=%d index=%d discard=%d\n",
		   current->pid, vertex.idx, vertex.discard );

	if ( vertex.idx < 0 || vertex.idx >= dma->buf_count ) {
		DRM_ERROR( "buffer index %d (of %d max)\n",
			   vertex.idx, dma->buf_count - 1 );
		return -EINVAL;
	}

	RING_SPACE_TEST_WITH_RETURN( dev_priv );
	VB_AGE_TEST_WITH_RETURN( dev_priv );

	buf = dma->buflist[vertex.idx];
	buf_priv = buf->dev_private;

	if ( buf->pid != current->pid ) {
		DRM_ERROR( "process %d using buffer owned by %d\n",
			   current->pid, buf->pid );
		return -EINVAL;
	}

	if ( buf->pending ) {
		DRM_ERROR( "sending pending buffer %d\n", vertex.idx );
		return -EINVAL;
	}

	for (laststate = 0xff, i = 0 ; i < vertex.nr_prims ; i++) {
		drm_radeon_prim_t prim;
		
		if ( copy_from_user( &prim, &vertex.prim[i], sizeof(prim) ) )
			return -EFAULT;
		
/*    		printk( "prim %d vfmt %x hwprim %x start %d finish %d\n", */
/*  			   i, prim.vc_format, prim.prim, */
/*  			   prim.start, prim.finish ); */

		if (  (prim.prim & RADEON_PRIM_TYPE_MASK) > 
		      RADEON_PRIM_TYPE_3VRT_LINE_LIST ) {
			DRM_ERROR( "buffer prim %d\n", prim.prim );
			return -EINVAL;
		}

		if ( prim.stateidx != laststate ) {
			drm_radeon_state_t state;			       
				
			if ( copy_from_user( &state, 
					     &vertex.state[prim.stateidx], 
					     sizeof(state) ) )
				return -EFAULT;

/*  			printk("emit state %d (%p) dirty %x\n", */
/*  			       prim.stateidx, */
/*  			       &vertex.state[prim.stateidx], */
/*  			       state.dirty); */

			radeon_emit_state2( dev_priv, &state );

			laststate = prim.stateidx;
		}

		if ( prim.finish <= prim.start )
			continue;

		if ( prim.start & 0x7 ) {
			DRM_ERROR( "misaligned buffer 0x%x\n", prim.start );
			return -EINVAL;
		}

		if ( prim.prim & RADEON_PRIM_WALK_IND ) {
			radeon_cp_dispatch_indices( dev, buf, &prim );
		} else {
			radeon_cp_dispatch_vertex( dev, buf, &prim );
		}
	}

	if ( vertex.discard ) {
		radeon_cp_discard_buffer( dev, buf );
	}

	return 0;
}
