// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_view_observer.h"

#include "content/renderer/render_view_impl.h"

using WebKit::WebFrame;

namespace content {

RenderViewObserver::RenderViewObserver(RenderView* render_view)
    : render_view_(render_view),
      routing_id_(MSG_ROUTING_NONE) {
  // |render_view| can be NULL on unit testing.
  if (render_view) {
    RenderViewImpl* impl = static_cast<RenderViewImpl*>(render_view);
    routing_id_ = impl->routing_id();
    impl->AddObserver(this);
  }
}

RenderViewObserver::~RenderViewObserver() {
  if (render_view_) {
    RenderViewImpl* impl = static_cast<RenderViewImpl*>(render_view_);
    impl->RemoveObserver(this);
  }
}

void RenderViewObserver::OnDestruct() {
  delete this;
}

bool RenderViewObserver::OnMessageReceived(const IPC::Message& message) {
  return false;
}

bool RenderViewObserver::Send(IPC::Message* message) {
  if (render_view_)
    return render_view_->Send(message);

  delete message;
  return false;
}

RenderView* RenderViewObserver::render_view() const {
  return render_view_;
}

void RenderViewObserver::RenderViewGone() {
  render_view_ = NULL;
}

}  // namespace content
