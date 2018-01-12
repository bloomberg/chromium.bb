// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/resize_params.h"

namespace content {

ResizeParams::ResizeParams()
    : browser_controls_shrink_blink_size(false),
      scroll_focused_node_into_view(false),
      top_controls_height(0.f),
      bottom_controls_height(0.f),
      is_fullscreen_granted(false),
      display_mode(blink::kWebDisplayModeUndefined),
      needs_resize_ack(false) {}

ResizeParams::ResizeParams(const ResizeParams& other) = default;

ResizeParams::~ResizeParams() {}

}  // namespace content
