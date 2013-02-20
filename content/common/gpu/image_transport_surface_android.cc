// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "base/logging.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "content/common/gpu/texture_image_transport_surface.h"
#include "ui/gl/gl_surface_egl.h"

namespace content {

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  scoped_refptr<gfx::GLSurface> surface;
  if (handle.transport_type == gfx::TEXTURE_TRANSPORT) {
    surface = new TextureImageTransportSurface(manager, stub, handle);
  } else if (handle.transport_type == gfx::NATIVE_DIRECT) {
    DCHECK(GpuSurfaceLookup::GetInstance());
    ANativeWindow* window =
        GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(
            stub->surface_id());
    surface = new gfx::NativeViewGLSurfaceEGL(false, window);
    bool initialize_success = surface->Initialize();
    if (window)
      ANativeWindow_release(window);
    if (!initialize_success)
      return NULL;

    surface = new PassThroughImageTransportSurface(
        manager, stub, surface.get(), false);
  } else {
    NOTIMPLEMENTED();
    return NULL;
  }

  if (surface->Initialize()) {
    return surface;
  } else {
    LOG(ERROR) << "Failed to initialize ImageTransportSurface";
    return NULL;
  }
}

}  // namespace content
