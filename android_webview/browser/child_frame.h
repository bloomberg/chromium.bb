// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_CHILD_FRAME_H_
#define ANDROID_WEBVIEW_BROWSER_CHILD_FRAME_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace cc {
class CompositorFrame;
}

namespace android_webview {

class ChildFrame {
 public:
  ChildFrame(scoped_ptr<cc::CompositorFrame> frame,
             const gfx::Rect& viewport_rect_for_tile_priority,
             const gfx::Transform& transform_for_tile_priority,
             bool offscreen_pre_raster,
             bool is_layer);
  ~ChildFrame();

  scoped_ptr<cc::CompositorFrame> frame;
  const gfx::Rect viewport_rect_for_tile_priority;
  const gfx::Transform transform_for_tile_priority;
  const bool offscreen_pre_raster;
  const bool is_layer;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildFrame);
};

}  // namespace webview

#endif  // ANDROID_WEBVIEW_BROWSER_CHILD_FRAME_H_
