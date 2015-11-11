// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "content/public/common/content_switches.h"
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
      GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(handle.handle);
  if (!window) {
    LOG(WARNING) << "Failed to acquire native widget.";
    return scoped_refptr<gfx::GLSurface>();
  }
  scoped_refptr<gfx::GLSurface> surface =
      new gfx::NativeViewGLSurfaceEGL(window);
  bool initialize_success = surface->Initialize();
  ANativeWindow_release(window);
  if (!initialize_success)
    return scoped_refptr<gfx::GLSurface>();

  return scoped_refptr<gfx::GLSurface>(
      new PassThroughImageTransportSurface(manager, stub, surface.get()));
}

}  // namespace content
