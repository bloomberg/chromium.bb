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

#ifndef __NV50_CONNECTOR_H__
#define __NV50_CONNECTOR_H__

#include "nv50_output.h"
#include "nv50_i2c.h"

#define CONNECTOR_UNKNOWN 0
#define CONNECTOR_VGA 1
#define CONNECTOR_DVI_D 2
#define CONNECTOR_DVI_I 3
#define CONNECTOR_LVDS 4
#define CONNECTOR_TV 5

struct nv50_connector {
	struct list_head item;

	struct drm_device *dev;
	int type;

	int bus;
	struct nv50_i2c_channel *i2c_chan;
	struct nv50_output *output;

	int requested_scaling_mode;

	bool use_dithering;

	bool (*detect) (struct nv50_connector *connector);
	int (*destroy) (struct nv50_connector *connector);
	struct nv50_output *(*to_output) (struct nv50_connector *connector, bool digital);
};

int nv50_connector_create(struct drm_device *dev, int bus, int i2c_index, int type);

#endif /* __NV50_CONNECTOR_H__ */
