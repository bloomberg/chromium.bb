/* mga_state.c -- State support for mga g200/g400 -*- linux-c -*-
 * Created: Thu Jan 27 02:53:43 2000 by jhartmann@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 * Authors: Jeff Hartmann <jhartmann@precisioninsight.com>
 * 	    Keith Whitwell <keithw@precisioninsight.com>
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/mga_state.c,v 1.1 2000/02/11 17:26:08 dawes Exp $
 *
 */
 
#define __NO_VERSION__
#include "drmP.h"
#include "mga_drv.h"
#include "mgareg_flags.h"
#include "mga_dma.h"
#include "mga_state.h"
#include "drm.h"

void mgaEmitClipRect( drm_mga_private_t *dev_priv, xf86drmClipRectRec *box )
{
	PRIMLOCALS;

	PRIMGETPTR( dev_priv );

	/* The G400 seems to have an issue with the second WARP not
	 * stalling clipper register writes.  This bothers me, but the only
	 * way I could get it to never clip the last triangle under any
	 * circumstances is by inserting TWO dwgsync commands.
	 */
 	if (dev_priv->chipset == MGA_CARD_TYPE_G400) { 
		PRIMOUTREG( MGAREG_DWGSYNC, 0 );
		PRIMOUTREG( MGAREG_DWGSYNC, 0 );
	}

	PRIMOUTREG( MGAREG_CXBNDRY, ((box->x2)<<16)|(box->x1) );
	PRIMOUTREG( MGAREG_YTOP, box->y1 * dev_priv->stride );
	PRIMOUTREG( MGAREG_YBOT, box->y2 * dev_priv->stride );
	PRIMADVANCE( dev_priv );
}


static void mgaEmitContext(drm_mga_private_t *dev_priv, 
			   drm_mga_buf_priv_t *buf_priv)
{
	unsigned int *regs = buf_priv->ContextState;
	PRIMLOCALS;
	
	PRIMGETPTR( dev_priv );
	PRIMOUTREG( MGAREG_DSTORG, regs[MGA_CTXREG_DSTORG] );
	PRIMOUTREG( MGAREG_MACCESS, regs[MGA_CTXREG_MACCESS] );
	PRIMOUTREG( MGAREG_PLNWT, regs[MGA_CTXREG_PLNWT] );
	PRIMOUTREG( MGAREG_DWGCTL, regs[MGA_CTXREG_DWGCTL] );
	PRIMOUTREG( MGAREG_ALPHACTRL, regs[MGA_CTXREG_ALPHACTRL] );
	PRIMOUTREG( MGAREG_FOGCOL, regs[MGA_CTXREG_FOGCOLOR] );
	PRIMOUTREG( MGAREG_WFLAG, regs[MGA_CTXREG_WFLAG] );

 	if (dev_priv->chipset == MGA_CARD_TYPE_G400) { 
		PRIMOUTREG( MGAREG_WFLAG1, regs[MGA_CTXREG_WFLAG] );
		PRIMOUTREG( MGAREG_TDUALSTAGE0, regs[MGA_CTXREG_TDUAL0] );
		PRIMOUTREG( MGAREG_TDUALSTAGE1, regs[MGA_CTXREG_TDUAL1] );      
	}   
   
	PRIMADVANCE( dev_priv );
}

static void mgaG200EmitTex( drm_mga_private_t *dev_priv, 
			    drm_mga_buf_priv_t *buf_priv )
{
	unsigned int *regs = buf_priv->TexState[0];
	PRIMLOCALS;

	PRIMGETPTR( dev_priv );
	PRIMOUTREG(MGAREG_TEXCTL2, regs[MGA_TEXREG_CTL2] );
	PRIMOUTREG(MGAREG_TEXCTL, regs[MGA_TEXREG_CTL] );
	PRIMOUTREG(MGAREG_TEXFILTER, regs[MGA_TEXREG_FILTER] );
	PRIMOUTREG(MGAREG_TEXBORDERCOL, regs[MGA_TEXREG_BORDERCOL] );
	PRIMOUTREG(MGAREG_TEXORG, regs[MGA_TEXREG_ORG] );
	PRIMOUTREG(MGAREG_TEXORG1, regs[MGA_TEXREG_ORG1] );
	PRIMOUTREG(MGAREG_TEXORG2, regs[MGA_TEXREG_ORG2] );
	PRIMOUTREG(MGAREG_TEXORG3, regs[MGA_TEXREG_ORG3] );
	PRIMOUTREG(MGAREG_TEXORG4, regs[MGA_TEXREG_ORG4] );		
	PRIMOUTREG(MGAREG_TEXWIDTH, regs[MGA_TEXREG_WIDTH] );
	PRIMOUTREG(MGAREG_TEXHEIGHT, regs[MGA_TEXREG_HEIGHT] );
   
	PRIMOUTREG(0x2d00 + 24*4, regs[MGA_TEXREG_WIDTH] );
	PRIMOUTREG(0x2d00 + 34*4, regs[MGA_TEXREG_HEIGHT] );

	PRIMADVANCE( dev_priv );  
}

static void mgaG400EmitTex0( drm_mga_private_t *dev_priv, 
			     drm_mga_buf_priv_t *buf_priv )
{
	unsigned int *regs = buf_priv->TexState[0];
	int multitex = buf_priv->WarpPipe & MGA_T2;

	PRIMLOCALS;
	PRIMGETPTR( dev_priv );

	PRIMOUTREG(MGAREG_TEXCTL2, regs[MGA_TEXREG_CTL2] );
	PRIMOUTREG(MGAREG_TEXCTL, regs[MGA_TEXREG_CTL] );
	PRIMOUTREG(MGAREG_TEXFILTER, regs[MGA_TEXREG_FILTER] );
	PRIMOUTREG(MGAREG_TEXBORDERCOL, regs[MGA_TEXREG_BORDERCOL] );
	PRIMOUTREG(MGAREG_TEXORG, regs[MGA_TEXREG_ORG] );
	PRIMOUTREG(MGAREG_TEXORG1, regs[MGA_TEXREG_ORG1] );
	PRIMOUTREG(MGAREG_TEXORG2, regs[MGA_TEXREG_ORG2] );
	PRIMOUTREG(MGAREG_TEXORG3, regs[MGA_TEXREG_ORG3] );
	PRIMOUTREG(MGAREG_TEXORG4, regs[MGA_TEXREG_ORG4] );		
	PRIMOUTREG(MGAREG_TEXWIDTH, regs[MGA_TEXREG_WIDTH] );
	PRIMOUTREG(MGAREG_TEXHEIGHT, regs[MGA_TEXREG_HEIGHT] );
   
	PRIMOUTREG(0x2d00 + 49*4, 0);
	PRIMOUTREG(0x2d00 + 57*4, 0);
	PRIMOUTREG(0x2d00 + 53*4, 0);
	PRIMOUTREG(0x2d00 + 61*4, 0);

	if (!multitex) {
		PRIMOUTREG(0x2d00 + 52*4, 0x40 ); 
		PRIMOUTREG(0x2d00 + 60*4, 0x40 );
	} 

	PRIMOUTREG(0x2d00 + 54*4, regs[MGA_TEXREG_WIDTH] | 0x40 );
	PRIMOUTREG(0x2d00 + 62*4, regs[MGA_TEXREG_HEIGHT] | 0x40 );

	PRIMADVANCE( dev_priv );  
}

#define TMC_map1_enable 		0x80000000 	


static void mgaG400EmitTex1( drm_mga_private_t *dev_priv, 
			     drm_mga_buf_priv_t *buf_priv )
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int *regs = sarea_priv->TexState[1];

	PRIMLOCALS;
	PRIMGETPTR(dev_priv);

	PRIMOUTREG(MGAREG_TEXCTL2, regs[MGA_TEXREG_CTL2] | TMC_map1_enable);
	PRIMOUTREG(MGAREG_TEXCTL, regs[MGA_TEXREG_CTL] );
	PRIMOUTREG(MGAREG_TEXFILTER, regs[MGA_TEXREG_FILTER] );
	PRIMOUTREG(MGAREG_TEXBORDERCOL, regs[MGA_TEXREG_BORDERCOL] );
	PRIMOUTREG(MGAREG_TEXORG, regs[MGA_TEXREG_ORG] );
	PRIMOUTREG(MGAREG_TEXORG1, regs[MGA_TEXREG_ORG1] );
	PRIMOUTREG(MGAREG_TEXORG2, regs[MGA_TEXREG_ORG2] );
	PRIMOUTREG(MGAREG_TEXORG3, regs[MGA_TEXREG_ORG3] );
	PRIMOUTREG(MGAREG_TEXORG4, regs[MGA_TEXREG_ORG4] );		
	PRIMOUTREG(MGAREG_TEXWIDTH, regs[MGA_TEXREG_WIDTH] );
	PRIMOUTREG(MGAREG_TEXHEIGHT, regs[MGA_TEXREG_HEIGHT] );
   
	PRIMOUTREG(0x2d00 + 49*4, 0);
	PRIMOUTREG(0x2d00 + 57*4, 0);
	PRIMOUTREG(0x2d00 + 53*4, 0);
	PRIMOUTREG(0x2d00 + 61*4, 0);

	PRIMOUTREG(0x2d00 + 52*4, regs[MGA_TEXREG_WIDTH] | 0x40 ); 
	PRIMOUTREG(0x2d00 + 60*4, regs[MGA_TEXREG_HEIGHT] | 0x40 );

	PRIMOUTREG(MGAREG_TEXCTL2, regs[MGA_TEXREG_CTL2] );

	PRIMADVANCE( dev_priv );     
}


static void mgaG400EmitPipe(drm_mga_private_t *dev_priv, 
			    drm_mga_buf_priv_t *buf_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int pipe = sarea_priv->WarpPipe;
	float fParam = 12800.0f;
	PRIMLOCALS;
   
	PRIMGETPTR(dev_priv);
	PRIMOUTREG(MGAREG_WIADDR2, WIA_wmode_suspend);
   
	/* Establish vertex size.  
	 */
	if (pipe & MGA_T2) {
		PRIMOUTREG(MGAREG_WVRTXSZ, 0x00001e09);
		PRIMOUTREG(MGAREG_WACCEPTSEQ, 0x1e000000);
	} else {
		PRIMOUTREG(MGAREG_WVRTXSZ, 0x00001807);
		PRIMOUTREG(MGAREG_WACCEPTSEQ, 0x18000000);
	}
   
	PRIMOUTREG(MGAREG_WFLAG, 0);
	PRIMOUTREG(MGAREG_WFLAG1, 0);
   
	PRIMOUTREG(0x2d00 + 56*4, *((u32 *)(&fParam)));
	PRIMOUTREG(MGAREG_DMAPAD, 0);
	PRIMOUTREG(MGAREG_DMAPAD, 0);
   
	PRIMOUTREG(0x2d00 + 49*4, 0);  /* Tex stage 0 */
	PRIMOUTREG(0x2d00 + 57*4, 0);  /* Tex stage 0 */
	PRIMOUTREG(0x2d00 + 53*4, 0);  /* Tex stage 1 */
	PRIMOUTREG(0x2d00 + 61*4, 0);  /* Tex stage 1 */
   
	PRIMOUTREG(0x2d00 + 54*4, 0x40); /* Tex stage 0 : w */
	PRIMOUTREG(0x2d00 + 62*4, 0x40); /* Tex stage 0 : h */
	PRIMOUTREG(0x2d00 + 52*4, 0x40); /* Tex stage 1 : w */
	PRIMOUTREG(0x2d00 + 60*4, 0x40); /* Tex stage 1 : h */
   
	/* Dma pading required due to hw bug */
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_WIADDR2, (dev_priv->WarpIndex[pipe].phys_addr | 
				   WIA_wmode_start | WIA_wagp_agp));
	PRIMADVANCE(dev_priv);
}

static void mgaG200EmitPipe( drm_mga_private_t *dev_priv,
			     drm_mga_buf_priv_t *buf_priv )
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int pipe = sarea_priv->WarpPipe;
	PRIMLOCALS;

	PRIMGETPTR(dev_priv);
	PRIMOUTREG(MGAREG_WIADDR, WIA_wmode_suspend);
	PRIMOUTREG(MGAREG_WVRTXSZ, 7);
	PRIMOUTREG(MGAREG_WFLAG, 0);
	PRIMOUTREG(0x2d00 + 24*4, 0); /* tex w/h */
   
	PRIMOUTREG(0x2d00 + 25*4, 0x100);
	PRIMOUTREG(0x2d00 + 34*4, 0); /* tex w/h */
	PRIMOUTREG(0x2d00 + 42*4, 0xFFFF);
	PRIMOUTREG(0x2d00 + 60*4, 0xFFFF);
   
	/* Dma pading required due to hw bug */
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_DMAPAD, 0xffffffff);
	PRIMOUTREG(MGAREG_WIADDR, (dev_priv->WarpIndex[pipe].phys_addr | 
				  WIA_wmode_start | WIA_wagp_agp));

	PRIMADVANCE(dev_priv);
}

void mgaEmitState( drm_mga_private_t *dev_priv, drm_mga_buf_priv_t *buf_priv )
{
	unsigned int dirty = buf_priv->dirty;

	if (dev_priv->chipset == MGA_CARD_TYPE_G400) {	   
		if (dirty & MGASAREA_NEW_CONTEXT)
			mgaEmitContext( dev_priv, buf_priv );

		if (dirty & MGASAREA_NEW_TEX1)
			mgaG400EmitTex1( dev_priv, buf_priv );
	   
		if (dirty & MGASAREA_NEW_TEX0)
			mgaG400EmitTex0( dev_priv, buf_priv );

		if (dirty & MGASAREA_NEW_PIPE) 
			mgaG400EmitPipe( dev_priv, buf_priv );
	} else {
		if (dirty & MGASAREA_NEW_CONTEXT)
			mgaEmitContext( dev_priv, buf_priv );

		if (dirty & MGASAREA_NEW_TEX0)
			mgaG200EmitTex( dev_priv, buf_priv );

		if (dirty & MGASAREA_NEW_PIPE) 
			mgaG200EmitPipe( dev_priv, buf_priv );
	}	  
}



/* Disallow all write destinations except the front and backbuffer.
 */
static int mgaCopyContext(drm_mga_private_t *dev_priv, 
			  drm_mga_buf_priv_t *buf_priv)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int *regs = sarea_priv->ContextState;
	
	if (regs[MGA_CTXREG_DSTORG] != dev_priv->frontOrg &&
	    regs[MGA_CTXREG_DSTORG] != dev_priv->backOrg)
		return -1;

	memcpy(buf_priv->ContextState, sarea_priv->ContextState,
	       sizeof(buf_priv->ContextState));
	return 0;
}


/* Disallow texture reads from PCI space.
 */
static int mgaCopyTex(drm_mga_private_t *dev_priv, 
		      drm_mga_buf_priv_t *buf_priv,
		      int unit)
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;

	if ((sarea_priv->TexState[unit][MGA_TEXREG_ORG] & 0x3) == 0x1)
		return -1;

	memcpy(buf_priv->TexState[unit], sarea_priv->TexState[unit],
	       sizeof(buf_priv->TexState[0]));

	return 0;
}


int mgaCopyAndVerifyState( drm_mga_private_t *dev_priv, 
			   drm_mga_buf_priv_t *buf_priv )
{
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned int dirty = sarea_priv->dirty ;
	int rv = 0;

	buf_priv->dirty = sarea_priv->dirty;
	buf_priv->WarpPipe = sarea_priv->WarpPipe;

	if (dirty & MGASAREA_NEW_CONTEXT)
		rv |= mgaCopyContext( dev_priv, buf_priv );

	if (dirty & MGASAREA_NEW_TEX0)
		rv |= mgaCopyTex( dev_priv, buf_priv, 0 );

	if (dev_priv->chipset == MGA_CARD_TYPE_G400) 
	{	   
		if (dirty & MGASAREA_NEW_TEX1)
			rv |= mgaCopyTex( dev_priv, buf_priv, 1 );
	   
		if (dirty & MGASAREA_NEW_PIPE) 
			rv |= (buf_priv->WarpPipe > MGA_MAX_G400_PIPES); 
	} 
	else 
	{
		if (dirty & MGASAREA_NEW_PIPE) 
			rv |= (buf_priv->WarpPipe > MGA_MAX_G200_PIPES); 
	}	  

	return rv == 0;
}


