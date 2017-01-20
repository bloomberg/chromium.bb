// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/direct_composition_surface_win.h"

#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"

namespace gpu {

DirectCompositionSurfaceWin::DirectCompositionSurfaceWin(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    HWND parent_window)
    : gl::GLSurfaceEGL(),
      child_window_(delegate, parent_window),
      window_(nullptr),
      surface_(0),
      first_swap_(true) {}

bool DirectCompositionSurfaceWin::InitializeNativeWindow() {
  if (window_)
    return true;

  bool result = child_window_.Initialize();
  window_ = child_window_.window();
  return result;
}

bool DirectCompositionSurfaceWin::Initialize(gl::GLSurfaceFormat format) {
  EGLDisplay display = GetDisplay();
  if (!window_) {
    if (!InitializeNativeWindow()) {
      LOG(ERROR) << "Failed to initialize native window";
      return false;
    }
  }
  std::vector<EGLint> pbuffer_attribs;
  pbuffer_attribs.push_back(EGL_WIDTH);
  pbuffer_attribs.push_back(1);
  pbuffer_attribs.push_back(EGL_HEIGHT);
  pbuffer_attribs.push_back(1);

  pbuffer_attribs.push_back(EGL_NONE);
  surface_ = eglCreatePbufferSurface(display, GetConfig(), &pbuffer_attribs[0]);
  CHECK(!!surface_);

  return true;
}

void DirectCompositionSurfaceWin::Destroy() {
  if (surface_) {
    if (!eglDestroySurface(GetDisplay(), surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << ui::GetLastEGLErrorString();
    }
    surface_ = NULL;
  }
}

gfx::Size DirectCompositionSurfaceWin::GetSize() {
  return size_;
}

bool DirectCompositionSurfaceWin::IsOffscreen() {
  return false;
}

void* DirectCompositionSurfaceWin::GetHandle() {
  return surface_;
}

bool DirectCompositionSurfaceWin::Resize(const gfx::Size& size,
                                         float scale_factor,
                                         bool has_alpha) {
  if (size == GetSize())
    return true;

  // Force a resize and redraw (but not a move, activate, etc.).
  if (!SetWindowPos(window_, nullptr, 0, 0, size.width(), size.height(),
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                        SWP_NOOWNERZORDER | SWP_NOZORDER)) {
    return false;
  }
  size_ = size;
  return true;
}

gfx::SwapResult DirectCompositionSurfaceWin::SwapBuffers() {
  CommitAndClearPendingOverlays();
  // Force the driver to finish drawing before clearing the contents to
  // transparent, to reduce or eliminate the period of time where the contents
  // have flashed black.
  if (first_swap_) {
    glFinish();
    first_swap_ = false;
  }
  child_window_.ClearInvalidContents();
  return gfx::SwapResult::SWAP_ACK;
}

gfx::SwapResult DirectCompositionSurfaceWin::PostSubBuffer(int x,
                                                           int y,
                                                           int width,
                                                           int height) {
  CommitAndClearPendingOverlays();
  child_window_.ClearInvalidContents();
  return gfx::SwapResult::SWAP_ACK;
}

gfx::VSyncProvider* DirectCompositionSurfaceWin::GetVSyncProvider() {
  return vsync_provider_.get();
}

bool DirectCompositionSurfaceWin::Initialize(
    std::unique_ptr<gfx::VSyncProvider> vsync_provider) {
  vsync_provider_ = std::move(vsync_provider);
  return Initialize(gl::GLSurfaceFormat());
}

bool DirectCompositionSurfaceWin::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    gl::GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect) {
  pending_overlays_.push_back(
      Overlay(z_order, transform, image, bounds_rect, crop_rect));
  return true;
}

bool DirectCompositionSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

DirectCompositionSurfaceWin::Overlay::Overlay(int z_order,
                                              gfx::OverlayTransform transform,
                                              scoped_refptr<gl::GLImage> image,
                                              gfx::Rect bounds_rect,
                                              gfx::RectF crop_rect)
    : z_order(z_order),
      transform(transform),
      image(image),
      bounds_rect(bounds_rect),
      crop_rect(crop_rect) {}

DirectCompositionSurfaceWin::Overlay::Overlay(const Overlay& overlay) = default;

DirectCompositionSurfaceWin::Overlay::~Overlay() {}

bool DirectCompositionSurfaceWin::CommitAndClearPendingOverlays() {
  pending_overlays_.clear();
  return true;
}

DirectCompositionSurfaceWin::~DirectCompositionSurfaceWin() {}

}  // namespace gpu
