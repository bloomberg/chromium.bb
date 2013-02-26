// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "cc/layer_tree_host.h"
#include "cc/switches.h"
#include "content/common/view_messages.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

namespace content {

void RenderViewImpl::ScheduleUpdateFrameInfo() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      cc::switches::kEnableCompositorFrameMessage))
    return;

  if (update_frame_info_scheduled_)
    return;
  update_frame_info_scheduled_ = true;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RenderViewImpl::SendUpdateFrameInfo, this));
}

void RenderViewImpl::SendUpdateFrameInfo() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      cc::switches::kEnableCompositorFrameMessage))
    return;

  update_frame_info_scheduled_ = false;

  if (!webview() || !webview()->mainFrame())
    return;

  Send(new ViewHostMsg_UpdateFrameInfo(
      routing_id_,
      GetScrollOffset(),
      webview()->pageScaleFactor(),
      webview()->minimumPageScaleFactor(),
      webview()->maximumPageScaleFactor(),
      gfx::Size(webview()->mainFrame()->contentsSize())));
}

void RenderViewImpl::OnEnableHidingTopControls(bool enable) {
  DCHECK(compositor_ && compositor_->layer_tree_host());
  if (compositor_ && compositor_->layer_tree_host()) {
    compositor_->layer_tree_host()->enableHidingTopControls(enable);
  }
}

}  // namespace content
