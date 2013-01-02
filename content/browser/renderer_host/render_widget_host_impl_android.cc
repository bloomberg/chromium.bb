// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/port/browser/render_widget_host_view_port.h"

namespace content {

void RenderWidgetHostImpl::OnUpdateFrameInfo(
    const gfx::Vector2d& scroll_offset,
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor,
    const gfx::Size& content_size) {
  if (view_)
    view_->UpdateFrameInfo(scroll_offset,
                           page_scale_factor,
                           min_page_scale_factor,
                           max_page_scale_factor,
                           content_size);
}

}  // namespace content
