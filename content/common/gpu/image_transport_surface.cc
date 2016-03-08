// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "content/common/gpu/gpu_channel_manager.h"

namespace content {

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle,
    gfx::GLSurface::Format format) {
  scoped_refptr<gfx::GLSurface> surface;
  if (handle.transport_type == gfx::NULL_TRANSPORT) {
    surface = manager->GetDefaultOffscreenSurface();
  } else {
    surface = CreateNativeSurface(manager, stub, handle, format);
    if (!surface.get() || !surface->Initialize(format))
      return NULL;
  }

  return surface;
}

}  // namespace content
