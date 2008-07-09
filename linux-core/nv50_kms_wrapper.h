/*
 * Copyright (C) 2008 Maarten Maathuis.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __NV50_KMS_WRAPPER_H__
#define __NV50_KMS_WRAPPER_H__

#include "drmP.h"
#include "drm.h"

#include "nv50_display.h"
#include "nv50_crtc.h"
#include "nv50_cursor.h"
#include "nv50_lut.h"
#include "nv50_fb.h"
#include "nv50_output.h"
#include "nv50_connector.h"

#include "drm_crtc.h"
#include "drm_edid.h"

/* Link internal modesetting structure to interface. */

struct nv50_kms_crtc {
	struct list_head item;

	struct nv50_crtc priv;
	struct drm_crtc pub;
};

struct nv50_kms_encoder {
	struct list_head item;

	struct nv50_output priv;
	struct drm_encoder pub;
};

struct nv50_kms_connector {
	struct list_head item;

	struct nv50_connector priv;
	struct drm_connector pub;
};

struct nv50_kms_priv {
	struct list_head crtcs;
	struct list_head encoders;
	struct list_head connectors;
};

/* Get private functions. */
#define from_nv50_kms_crtc(x) container_of(x, struct nv50_kms_crtc, pub)
#define from_nv50_crtc(x) container_of(x, struct nv50_kms_crtc, priv)
#define from_nv50_kms_encoder(x) container_of(x, struct nv50_kms_encoder, pub)
#define from_nv50_output(x) container_of(x, struct nv50_kms_encoder, priv)
#define from_nv50_kms_connector(x) container_of(x, struct nv50_kms_connector, pub)
#define from_nv50_connector(x) container_of(x, struct nv50_kms_connector, priv)

#define to_nv50_crtc(x) (&(from_nv50_kms_crtc(x)->priv))
#define to_nv50_kms_crtc(x) (&(from_nv50_crtc(x)->pub))
#define to_nv50_output(x) (&(from_nv50_kms_encoder(x)->priv))
#define to_nv50_kms_encoder(x) (&(from_nv50_output(x)->pub))
#define to_nv50_connector(x) (&(from_nv50_kms_connector(x)->priv))
#define to_nv50_kms_connector(x) (&(from_nv50_connector(x)->pub))

struct nv50_kms_priv *nv50_get_kms_priv(struct drm_device *dev);
void nv50_kms_connector_detect_all(struct drm_device *dev);
bool nv50_kms_connector_is_digital(struct drm_connector *drm_connector);

int nv50_kms_init(struct drm_device *dev);
int nv50_kms_destroy(struct drm_device *dev);

#endif /* __NV50_KMS_WRAPPER_H__ */
