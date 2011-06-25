// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_share_group.h"
#include "ui/gfx/gl/gl_surface.h"

#if defined(TOUCH_UI)
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gfx/gl/gl_surface_egl.h"
#include "ui/gfx/gl/gl_bindings.h"
#endif

using ::base::SharedMemory;

namespace gpu {

bool GpuScheduler::Initialize(
    gfx::PluginWindowHandle window,
    const gfx::Size& size,
    const gles2::DisallowedExtensions& disallowed_extensions,
    const char* allowed_extensions,
    const std::vector<int32>& attribs,
    gfx::GLShareGroup* share_group) {
  // Create either a view or pbuffer based GLSurface.
  scoped_refptr<gfx::GLSurface> surface;
#if defined(TOUCH_UI)
  surface = gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size(1, 1));
#else
  if (window)
    surface = gfx::GLSurface::CreateViewGLSurface(window);
  else
    surface = gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size(1, 1));
#endif

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

void GpuScheduler::WillSwapBuffers() {
#if defined(TOUCH_UI)
  front_surface_.swap(back_surface_);
  DCHECK_NE(front_surface_.get(), static_cast<AcceleratedSurface*>(NULL));

  gfx::Size expected_size = front_surface_->size();
  if (!back_surface_.get() || back_surface_->size() != expected_size) {
    wrapped_resize_callback_->Run(expected_size);
  } else {
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                              GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D,
                              back_surface_->texture(),
                              0);
  }
  ++swap_buffers_count_;
  glFlush();
#endif
  if (wrapped_swap_buffers_callback_.get()) {
    wrapped_swap_buffers_callback_->Run();
  }
}

#if defined(TOUCH_UI)
uint64 GpuScheduler::GetBackSurfaceId() {
  if (!back_surface_.get())
    return 0;
  return back_surface_->pixmap();
}

uint64 GpuScheduler::GetFrontSurfaceId() {
  if (!front_surface_.get())
    return 0;
  return front_surface_->pixmap();
}

uint64 GpuScheduler::swap_buffers_count() const {
  return swap_buffers_count_;
}

uint64 GpuScheduler::acknowledged_swap_buffers_count() const {
  return acknowledged_swap_buffers_count_;
}

void GpuScheduler::set_acknowledged_swap_buffers_count(
    uint64 acknowledged_swap_buffers_count) {
  acknowledged_swap_buffers_count_ = acknowledged_swap_buffers_count;
}

void GpuScheduler::CreateBackSurface(const gfx::Size& size) {
  decoder()->ResizeOffscreenFrameBuffer(size);
  decoder()->UpdateOffscreenFrameBufferSize();

  back_surface_ = new AcceleratedSurface(size);
  surfaces_[back_surface_->pixmap()] = back_surface_;

  glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D,
                            back_surface_->texture(),
                            0);
  glFlush();
}

void GpuScheduler::ReleaseSurface(uint64 surface_id) {
  DCHECK_NE(surfaces_[surface_id].get(), back_surface_.get());
  DCHECK_NE(surfaces_[surface_id].get(), front_surface_.get());
  surfaces_.erase(surface_id);
}

#endif

}  // namespace gpu
