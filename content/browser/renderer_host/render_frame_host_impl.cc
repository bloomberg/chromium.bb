// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_host_impl.h"

#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {

RenderFrameHostImpl::RenderFrameHostImpl(
    RenderViewHostImpl* render_view_host,
    int routing_id,
    bool swapped_out)
    : render_view_host_(render_view_host),
      routing_id_(routing_id) {
}

RenderFrameHostImpl::~RenderFrameHostImpl() {
}

bool RenderFrameHostImpl::Send(IPC::Message* message) {
  // Use the RenderViewHost object to send the message. It inherits it from
  // RenderWidgetHost, which ultimately uses the current process's |Send|.
  return render_view_host_->Send(message);
}

bool RenderFrameHostImpl::OnMessageReceived(const IPC::Message &msg) {
  // Pass the message up to the RenderViewHost, until we have enough
  // infrastructure to start processing messages in this object.
  return render_view_host_->OnMessageReceived(msg);
}

}  // namespace content
