#ifndef MGA_STATE_H
#define MGA_STATE_H

#include "mga_drv.h"

int mgaCopyAndVerifyState( drm_mga_private_t *dev_priv, 
			   drm_mga_buf_priv_t *buf_priv );

void mgaEmitClipRect( drm_mga_private_t *dev_priv, xf86drmClipRectRec *box );

void mgaEmitState( drm_mga_private_t *dev_priv, drm_mga_buf_priv_t *buf_priv );

#endif
