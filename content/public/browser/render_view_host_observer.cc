// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/render_view_host_observer.h"

#include "content/browser/renderer_host/render_view_host.h"

namespace content {

RenderViewHostObserver::RenderViewHostObserver(RenderViewHost* render_view_host)
    : render_view_host_(render_view_host),
      routing_id_(render_view_host->routing_id()) {
  render_view_host_->AddObserver(this);
}

RenderViewHostObserver::~RenderViewHostObserver() {
  if (render_view_host_)
    render_view_host_->RemoveObserver(this);
}

void RenderViewHostObserver::RenderViewHostInitialized() {
}

void RenderViewHostObserver::RenderViewHostDestroyed(RenderViewHost* rvh) {
  delete this;
}

void RenderViewHostObserver::Navigate(const GURL& url) {
}

bool RenderViewHostObserver::OnMessageReceived(const IPC::Message& message) {
  return false;
}

bool RenderViewHostObserver::Send(IPC::Message* message) {
  if (!render_view_host_) {
    delete message;
    return false;
  }

  return render_view_host_->Send(message);
}

void RenderViewHostObserver::RenderViewHostDestruction() {
  render_view_host_->RemoveObserver(this);
  RenderViewHost* rvh = render_view_host_;
  render_view_host_ = NULL;
  RenderViewHostDestroyed(rvh);
}

}  // namespace content
