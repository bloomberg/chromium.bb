// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/child_window_surface_win.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {

ChildWindowSurfaceWin::ChildWindowSurfaceWin(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    HWND parent_window)
    : gl::NativeViewGLSurfaceEGL(0),
      child_window_(delegate, parent_window),
      alpha_(true),
      first_swap_(true) {
  // Don't use EGL_ANGLE_window_fixed_size so that we can avoid recreating the
  // window surface, which can cause flicker on DirectComposition.
  enable_fixed_size_angle_ = false;
}

EGLConfig ChildWindowSurfaceWin::GetConfig() {
  if (!config_) {
    int alpha_size = alpha_ ? 8 : EGL_DONT_CARE;
    int bits_per_channel = base::CommandLine::ForCurrentProcess()->HasSwitch(
                               switches::kEnableHDROutput)
                               ? 16
                               : 8;

    EGLint config_attribs[] = {EGL_ALPHA_SIZE,
                               alpha_size,
                               EGL_BLUE_SIZE,
                               bits_per_channel,
                               EGL_GREEN_SIZE,
                               bits_per_channel,
                               EGL_RED_SIZE,
                               bits_per_channel,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                               EGL_NONE};

    EGLDisplay display = GetHardwareDisplay();
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &config_, 1, &num_configs)) {
      LOG(ERROR) << "eglChooseConfig failed with error "
                 << ui::GetLastEGLErrorString();
      return NULL;
    }
  }

  return config_;
}

bool ChildWindowSurfaceWin::InitializeNativeWindow() {
  if (window_)
    return true;

  bool result = child_window_.Initialize();
  window_ = child_window_.window();
  return result;
}

bool ChildWindowSurfaceWin::Resize(const gfx::Size& size,
                                   float scale_factor,
                                   bool has_alpha) {
  if (!SupportsPostSubBuffer()) {
    if (!MoveWindow(window_, 0, 0, size.width(), size.height(), FALSE)) {
      return false;
    }
    alpha_ = has_alpha;
    return gl::NativeViewGLSurfaceEGL::Resize(size, scale_factor, has_alpha);
  } else {
    if (size == GetSize() && has_alpha == alpha_)
      return true;

    // Force a resize and redraw (but not a move, activate, etc.).
    if (!SetWindowPos(window_, nullptr, 0, 0, size.width(), size.height(),
                      SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                          SWP_NOOWNERZORDER | SWP_NOZORDER)) {
      return false;
    }
    size_ = size;
    if (has_alpha == alpha_) {
      // A 0-size PostSubBuffer doesn't swap but forces the swap chain to resize
      // to match the window.
      PostSubBuffer(0, 0, 0, 0);
    } else {
      alpha_ = has_alpha;
      config_ = nullptr;

      std::unique_ptr<ui::ScopedMakeCurrent> scoped_make_current;
      gl::GLContext* current_context = gl::GLContext::GetCurrent();
      bool was_current = current_context && current_context->IsCurrent(this);
      if (was_current) {
        scoped_make_current.reset(
            new ui::ScopedMakeCurrent(current_context, this));
        current_context->ReleaseCurrent(this);
      }

      Destroy();

      if (!Initialize()) {
        LOG(ERROR) << "Failed to resize window.";
        return false;
      }
    }
    return true;
  }
}

gfx::SwapResult ChildWindowSurfaceWin::SwapBuffers() {
  gfx::SwapResult result = NativeViewGLSurfaceEGL::SwapBuffers();
  // Force the driver to finish drawing before clearing the contents to
  // transparent, to reduce or eliminate the period of time where the contents
  // have flashed black.
  if (first_swap_) {
    glFinish();
    first_swap_ = false;
  }
  child_window_.ClearInvalidContents();
  return result;
}

gfx::SwapResult ChildWindowSurfaceWin::PostSubBuffer(int x,
                                                     int y,
                                                     int width,
                                                     int height) {
  gfx::SwapResult result =
      NativeViewGLSurfaceEGL::PostSubBuffer(x, y, width, height);
  child_window_.ClearInvalidContents();
  return result;
}

ChildWindowSurfaceWin::~ChildWindowSurfaceWin() {
}

}  // namespace gpu
