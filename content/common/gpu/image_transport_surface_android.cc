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
  if (!handle.handle && handle.transport) {
    DCHECK(handle.parent_client_id);
    surface = new TextureImageTransportSurface(manager, stub, handle);
  } else if (handle.handle == gfx::kDummyPluginWindow && !handle.transport) {
    DCHECK(GpuSurfaceLookup::GetInstance());
    ANativeWindow* window = GpuSurfaceLookup::GetInstance()->GetNativeWidget(
        stub->surface_id());
    DCHECK(window);
    surface = new gfx::NativeViewGLSurfaceEGL(false, window);
    if (!surface.get() || !surface->Initialize())
      return NULL;

    surface = new PassThroughImageTransportSurface(
        manager, stub, surface.get(), handle.transport);
  } else {
    NOTIMPLEMENTED();
    return NULL;
  }

  if (surface->Initialize())
    return surface;
  else {
    LOG(ERROR) << "Failed to initialize ImageTransportSurface";
    return NULL;
  }
}

}  // namespace content
