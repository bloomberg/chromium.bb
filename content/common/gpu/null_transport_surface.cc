// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/null_transport_surface.h"

#include "base/command_line.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/context_group.h"

namespace content {

NullTransportSurface::NullTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle)
    : PassThroughImageTransportSurface(manager,
                                       stub,
                                       manager->GetDefaultOffscreenSurface()),
      parent_client_id_(handle.parent_client_id) {
}

NullTransportSurface::~NullTransportSurface() {
}

bool NullTransportSurface::Initialize() {
  if (!surface())
    return false;

  if (!PassThroughImageTransportSurface::Initialize())
    return false;

  GpuChannel* parent_channel =
      GetHelper()->manager()->LookupChannel(parent_client_id_);
  if (parent_channel) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUIPrioritizeInGpuProcess))
      GetHelper()->SetPreemptByFlag(parent_channel->GetPreemptionFlag());
  }

  return true;
}

void NullTransportSurface::Destroy() {
  // Do not destroy |surface_| since we use the shared offscreen surface.
}

bool NullTransportSurface::SwapBuffers() {
  NOTIMPLEMENTED();
  return false;
}

bool NullTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  NOTIMPLEMENTED();
  return false;
}

void NullTransportSurface::SendVSyncUpdateIfAvailable() {
  NOTREACHED();
}

bool NullTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  // Override PassThroughImageTransportSurface default behavior which
  // sets the swap interval.
  return true;
}

}  // namespace content
