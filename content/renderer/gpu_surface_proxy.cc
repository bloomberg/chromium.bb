// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu_surface_proxy.h"
#include "ui/gfx/size.h"

GpuSurfaceProxy::GpuSurfaceProxy(
    IPC::Channel::Sender* channel,
    int route_id)
    : channel_(channel),
      route_id_(route_id) {
}

GpuSurfaceProxy::~GpuSurfaceProxy() {
}

bool GpuSurfaceProxy::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  //IPC_BEGIN_MESSAGE_MAP(GpuSurfaceProxy, message)
  //  IPC_MESSAGE_UNHANDLED(handled = false)
  //IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void GpuSurfaceProxy::OnChannelError() {
  // Prevent any further messages from being sent.
  channel_ = NULL;
}
