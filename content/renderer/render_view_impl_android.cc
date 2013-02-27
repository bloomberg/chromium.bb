// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "cc/layer_tree_host.h"
#include "content/renderer/gpu/render_widget_compositor.h"

namespace content {

void RenderViewImpl::OnEnableHidingTopControls(bool enable) {
  DCHECK(compositor_ && compositor_->layer_tree_host());
  if (compositor_ && compositor_->layer_tree_host()) {
    compositor_->layer_tree_host()->enableHidingTopControls(enable);
  }
}

}  // namespace content
