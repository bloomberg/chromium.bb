// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_impl.h"

#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"

namespace content {

RenderFrameImpl::RenderFrameImpl(RenderViewImpl* render_view, int routing_id)
    : render_view_(render_view),
      routing_id_(routing_id) {
}

RenderFrameImpl::~RenderFrameImpl() {
}

bool RenderFrameImpl::Send(IPC::Message* message) {
  // TODO(nasko): Move away from using the RenderView's Send method once we
  // have enough infrastructure and state to make the right checks here.
  return render_view_->Send(message);
}

bool RenderFrameImpl::OnMessageReceived(const IPC::Message &msg) {
  // Pass the message up to the RenderView, until we have enough
  // infrastructure to start processing messages in this object.
  return render_view_->OnMessageReceived(msg);
}

}  // namespace content
