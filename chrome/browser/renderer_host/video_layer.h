// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_VIDEO_LAYER_H_
#define CHROME_BROWSER_RENDERER_HOST_VIDEO_LAYER_H_
#pragma once

#include "app/surface/transport_dib.h"
#include "gfx/size.h"

class RenderProcessHost;
class RenderWidgetHost;

namespace gfx {
class Rect;
}

// Represents a layer of YUV data owned by RenderWidgetHost and composited with
// the backing store.  VideoLayer is responsible for converting to RGB as
// needed.
class VideoLayer {
 public:
  virtual ~VideoLayer();

  RenderWidgetHost* render_widget_host() const { return render_widget_host_; }
  const gfx::Size& size() { return size_; }

  // Copy the incoming bitmap into this video layer. |bitmap| contains YUV
  // pixel data in YV12 format and must be the same dimensions as this video
  // layer. |bitmap_rect| specifies the absolute position and destination size
  // of the bitmap on the backing store.
  virtual void CopyTransportDIB(RenderProcessHost* process,
                                TransportDIB::Id bitmap,
                                const gfx::Rect& bitmap_rect) = 0;

 protected:
  // Can only be constructed via subclasses.
  VideoLayer(RenderWidgetHost* widget, const gfx::Size& size);

 private:
  // The owner of this video layer.
  RenderWidgetHost* render_widget_host_;

  // The size of the video layer.
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(VideoLayer);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_VIDEO_LAYER_H_
