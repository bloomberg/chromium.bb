// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "content/common/gpu/texture_image_transport_surface.h"

namespace content {

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  scoped_refptr<gfx::GLSurface> surface;
  if (!handle.handle) {
    DCHECK(handle.transport);
    surface = new TextureImageTransportSurface(manager, stub, handle);
  } else {
    surface = gfx::GLSurface::CreateViewGLSurface(false, handle.handle);
    if (!surface.get())
      return NULL;
    surface = new PassThroughImageTransportSurface(manager,
                                                   stub,
                                                   surface.get(),
                                                   handle.transport);
  }

  if (surface->Initialize())
    return surface;
  else
    return NULL;
}

}  // namespace content
