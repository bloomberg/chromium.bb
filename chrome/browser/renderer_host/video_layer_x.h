// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_VIDEO_LAYER_X_H_
#define CHROME_BROWSER_RENDERER_HOST_VIDEO_LAYER_X_H_
#pragma once

#include "app/x11_util.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/renderer_host/video_layer.h"
#include "gfx/rect.h"

// Implements a YUV data layer using X to hold the RGB data.
class VideoLayerX : public VideoLayer {
 public:
  VideoLayerX(RenderWidgetHost* widget, const gfx::Size& size, void* visual,
              int depth);
  virtual ~VideoLayerX();

  // VideoLayer implementation.
  virtual void CopyTransportDIB(RenderProcessHost* process,
                                TransportDIB::Id bitmap,
                                const gfx::Rect& bitmap_rect);

  // Copy from the server-side video layer to the target window.
  // Unlike BackingStore, we maintain the absolute position and destination
  // size so passing in a rect is not required.
  void XShow(XID target);

 private:
  // X Visual to get RGB mask information.
  void* const visual_;
  // Depth of the target window.
  int depth_;
  // Connection to the X server where this video layer will be displayed.
  Display* const display_;

  // Handle to the server side pixmap which is our video layer.
  XID pixmap_;
  // Graphics context for painting our video layer.
  void* pixmap_gc_;
  // Server side bits-per-pixel for |pixmap_|.
  int pixmap_bpp_;

  // Most recently converted frame stored as 32-bit ARGB.
  scoped_array<uint8> rgb_frame_;
  size_t rgb_frame_size_;

  // Destination size and absolution position of the converted frame.
  gfx::Rect rgb_rect_;

  DISALLOW_COPY_AND_ASSIGN(VideoLayerX);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_VIDEO_LAYER_X_H_
