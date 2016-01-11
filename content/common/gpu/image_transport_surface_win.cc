// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/win/windows_version.h"
#include "content/common/gpu/child_window_surface_win.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/vsync_provider_win.h"

namespace content {

// static
scoped_refptr<gfx::GLSurface> ImageTransportSurface::CreateNativeSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle) {
  DCHECK(handle.handle);
  DCHECK_EQ(handle.transport_type, gfx::NATIVE_DIRECT);

  scoped_refptr<gfx::GLSurface> surface;
  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2 &&
      gfx::GLSurfaceEGL::IsDirectCompositionSupported()) {
    scoped_refptr<ChildWindowSurfaceWin> egl_surface(
        new ChildWindowSurfaceWin(manager, handle.handle));
    surface = egl_surface;

    // TODO(jbauman): Get frame statistics from DirectComposition
    scoped_ptr<gfx::VSyncProvider> vsync_provider(
        new gfx::VSyncProviderWin(handle.handle));
    if (!egl_surface->Initialize(std::move(vsync_provider)))
      return nullptr;
  } else {
    surface = gfx::GLSurface::CreateViewGLSurface(handle.handle);
  }

  return scoped_refptr<gfx::GLSurface>(new PassThroughImageTransportSurface(
      manager, stub, surface.get()));
}

}  // namespace content
