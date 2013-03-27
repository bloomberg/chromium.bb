// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "base/logging.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "ui/gl/gl_surface_egl.h"

namespace content {

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  DCHECK(GpuSurfaceLookup::GetInstance());
  DCHECK_EQ(handle.transport_type, gfx::NATIVE_DIRECT);
  ANativeWindow* window =
      GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(
          stub->surface_id());
  scoped_refptr<gfx::GLSurface> surface =
      new gfx::NativeViewGLSurfaceEGL(false, window);
  bool initialize_success = surface->Initialize();
  if (window)
    ANativeWindow_release(window);
  if (!initialize_success)
    return scoped_refptr<gfx::GLSurface>();

  return scoped_refptr<gfx::GLSurface>(new PassThroughImageTransportSurface(
      manager, stub, surface.get(), false));
}

}  // namespace content
