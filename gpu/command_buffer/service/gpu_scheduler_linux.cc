// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_share_group.h"
#include "ui/gfx/gl/gl_surface.h"

using ::base::SharedMemory;

namespace gpu {

bool GpuScheduler::Initialize(
    gfx::PluginWindowHandle window,
    const gfx::Size& size,
    bool software,
    const gles2::DisallowedExtensions& disallowed_extensions,
    const char* allowed_extensions,
    const std::vector<int32>& attribs,
    gfx::GLShareGroup* share_group) {
#if defined(TOUCH_UI)
  NOTIMPLEMENTED();
  return false;
#endif

  // Create either a view or pbuffer based GLSurface.
  scoped_refptr<gfx::GLSurface> surface;
  if (window)
    surface = gfx::GLSurface::CreateViewGLSurface(software, window);
  else
    surface = gfx::GLSurface::CreateOffscreenGLSurface(software,
                                                       gfx::Size(1, 1));
  if (!surface.get()) {
    LOG(ERROR) << "GpuScheduler::Initialize failed.\n";
    Destroy();
    return false;
  }

  // Create a GLContext and attach the surface.
  scoped_refptr<gfx::GLContext> context(
      gfx::GLContext::CreateGLContext(share_group, surface.get()));
  if (!context.get()) {
    LOG(ERROR) << "CreateGLContext failed.\n";
    Destroy();
    return false;
  }

  return InitializeCommon(surface,
                          context,
                          size,
                          disallowed_extensions,
                          allowed_extensions,
                          attribs);
}

void GpuScheduler::Destroy() {
  DestroyCommon();
}

}  // namespace gpu
