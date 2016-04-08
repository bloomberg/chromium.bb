// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface.h"

#include "gpu/ipc/service/pass_through_image_transport_surface.h"

namespace gpu {

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    SurfaceHandle surface_handle,
    gfx::GLSurface::Format format) {
  DCHECK_NE(surface_handle, kNullSurfaceHandle);
  scoped_refptr<gfx::GLSurface> surface;
#if defined(USE_OZONE)
  surface = gfx::GLSurface::CreateSurfacelessViewGLSurface(surface_handle);
#endif
  if (!surface)
    surface = gfx::GLSurface::CreateViewGLSurface(surface_handle);
  if (!surface)
    return surface;
  return scoped_refptr<gfx::GLSurface>(new PassThroughImageTransportSurface(
      manager, stub, surface.get()));
}

}  // namespace gpu
