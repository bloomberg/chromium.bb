// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_CHILD_FRAME_H_
#define ANDROID_WEBVIEW_BROWSER_CHILD_FRAME_H_

#include <deque>
#include <memory>

#include "android_webview/browser/compositor_id.h"
#include "base/macros.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace cc {
class CompositorFrame;
}

namespace android_webview {

class ChildFrame {
 public:
  ChildFrame(
      scoped_refptr<content::SynchronousCompositor::FrameFuture> frame_future,
      uint32_t layer_tree_frame_sink_id,
      std::unique_ptr<cc::CompositorFrame> frame,
      const CompositorID& compositor_id,
      bool viewport_rect_for_tile_priority_empty,
      const gfx::Transform& transform_for_tile_priority,
      bool offscreen_pre_raster,
      bool is_layer);
  ~ChildFrame();

  // Helper to move frame from |frame_future| to |frame|.
  void WaitOnFutureIfNeeded();

  // This is used in async ondraw path. The frame is either in |frame_future|
  // or |frame|. It's illegal if both are non-null.
  scoped_refptr<content::SynchronousCompositor::FrameFuture> frame_future;
  // These two fields are not const to make async path easier.
  uint32_t layer_tree_frame_sink_id;
  std::unique_ptr<cc::CompositorFrame> frame;
  // The id of the compositor this |frame| comes from.
  const CompositorID compositor_id;
  const bool viewport_rect_for_tile_priority_empty;
  const gfx::Transform transform_for_tile_priority;
  const bool offscreen_pre_raster;
  const bool is_layer;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildFrame);
};

using ChildFrameQueue = std::deque<std::unique_ptr<ChildFrame>>;

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_CHILD_FRAME_H_
