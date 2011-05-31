// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_surface.h"

using ::base::SharedMemory;

namespace gpu {

bool GpuScheduler::Initialize(
    gfx::PluginWindowHandle window,
    const gfx::Size& size,
    const gles2::DisallowedExtensions& disallowed_extensions,
    const char* allowed_extensions,
    const std::vector<int32>& attribs,
    GpuScheduler* parent,
    uint32 parent_texture_id) {
  // Get the parent decoder and the GLContext to share IDs with, if any.
  gles2::GLES2Decoder* parent_decoder = NULL;
  gfx::GLContext* parent_context = NULL;
  if (parent) {
    parent_decoder = parent->decoder_.get();
    DCHECK(parent_decoder);

    parent_context = parent_decoder->GetGLContext();
    DCHECK(parent_context);
  }

  // Create either a view or pbuffer based GLSurface.
  scoped_ptr<gfx::GLSurface> surface;
  if (window) {
    surface.reset(gfx::GLSurface::CreateViewGLSurface(window));
  } else {
    surface.reset(gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size(1, 1)));
  }

  if (!surface.get()) {
    LOG(ERROR) << "GpuScheduler::Initialize failed.\n";
    Destroy();
    return false;
  }

  // Create a GLContext and attach the surface.
  scoped_ptr<gfx::GLContext> context(
      gfx::GLContext::CreateGLContext(parent_context, surface.get()));
  if (!context.get()) {
    LOG(ERROR) << "CreateGLContext failed.\n";
    Destroy();
    return false;
  }

  return InitializeCommon(surface.release(),
                          context.release(),
                          size,
                          disallowed_extensions,
                          allowed_extensions,
                          attribs,
                          parent_decoder,
                          parent_texture_id);
}

void GpuScheduler::Destroy() {
  DestroyCommon();
}

void GpuScheduler::WillSwapBuffers() {
  if (wrapped_swap_buffers_callback_.get()) {
    wrapped_swap_buffers_callback_->Run();
  }
}

}  // namespace gpu
