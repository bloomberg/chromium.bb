// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "content/common/gpu/gpu_surface_stub.h"

#include "content/common/gpu/gpu_channel.h"
#include "ipc/ipc_message_macros.h"

GpuSurfaceStub::GpuSurfaceStub(GpuChannel* channel,
                               int route_id,
                               gfx::GLSurface* surface)
    : channel_(channel),
      route_id_(route_id),
      surface_(surface) {
}

GpuSurfaceStub::~GpuSurfaceStub() {
}

bool GpuSurfaceStub::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  //IPC_BEGIN_MESSAGE_MAP(GpuSurfaceStub, message)
  //  IPC_MESSAGE_UNHANDLED(handled = false)
  //IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

bool GpuSurfaceStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}


#endif  // defined(ENABLE_GPU)
