// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/child_frame.h"

#include "cc/output/compositor_frame.h"

namespace android_webview {

ChildFrame::ChildFrame(scoped_ptr<cc::CompositorFrame> frame,
                       bool viewport_rect_for_tile_priority_empty,
                       const gfx::Transform& transform_for_tile_priority,
                       bool offscreen_pre_raster,
                       bool is_layer)
    : frame(frame.Pass()),
      viewport_rect_for_tile_priority_empty(
          viewport_rect_for_tile_priority_empty),
      transform_for_tile_priority(transform_for_tile_priority),
      offscreen_pre_raster(offscreen_pre_raster),
      is_layer(is_layer) {}

ChildFrame::~ChildFrame() {
}

}  // namespace webview
