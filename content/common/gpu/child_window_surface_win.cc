// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/child_window_surface_win.h"

#include "base/compiler_specific.h"
#include "base/win/wrapped_window_proc.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface_egl.h"

namespace content {

namespace {

ATOM g_window_class;

LRESULT CALLBACK IntermediateWindowProc(HWND window,
                                        UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  switch (message) {
    case WM_ERASEBKGND:
      // Prevent windows from erasing the background.
      return 1;
    case WM_PAINT:
      // Do not paint anything.
      PAINTSTRUCT paint;
      if (BeginPaint(window, &paint)) {
        // DirectComposition composites with the contents under the SwapChain,
        // so ensure that's cleared.
        if (!IsRectEmpty(&paint.rcPaint)) {
          FillRect(paint.hdc, &paint.rcPaint,
                   (HBRUSH)GetStockObject(BLACK_BRUSH));
        }
        EndPaint(window, &paint);
      }
      return 0;
    default:
      return DefWindowProc(window, message, w_param, l_param);
  }
}

void InitializeWindowClass() {
  if (g_window_class)
    return;

  WNDCLASSEX intermediate_class;
  base::win::InitializeWindowClass(
      L"Intermediate D3D Window",
      &base::win::WrappedWindowProc<IntermediateWindowProc>, CS_OWNDC, 0, 0,
      nullptr, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr,
      nullptr, nullptr, &intermediate_class);
  g_window_class = RegisterClassEx(&intermediate_class);
  if (!g_window_class) {
    LOG(ERROR) << "RegisterClass failed.";
    return;
  }
}
}

ChildWindowSurfaceWin::ChildWindowSurfaceWin(GpuChannelManager* manager,
                                             HWND parent_window)
    : gfx::NativeViewGLSurfaceEGL(0),
      parent_window_(parent_window),
      manager_(manager) {
  // Don't use EGL_ANGLE_window_fixed_size so that we can avoid recreating the
  // window surface, which can cause flicker on DirectComposition.
  enable_fixed_size_angle_ = false;
}

bool ChildWindowSurfaceWin::InitializeNativeWindow() {
  if (window_)
    return true;
  InitializeWindowClass();
  DCHECK(g_window_class);

  RECT windowRect;
  GetClientRect(parent_window_, &windowRect);

  window_ = CreateWindowEx(
      WS_EX_NOPARENTNOTIFY, reinterpret_cast<wchar_t*>(g_window_class), L"",
      WS_CHILDWINDOW | WS_DISABLED | WS_VISIBLE, 0, 0,
      windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
      ui::GetHiddenWindow(), NULL, NULL, NULL);
  manager_->Send(new GpuHostMsg_AcceleratedSurfaceCreatedChildWindow(
      parent_window_, window_));
  return true;
}

bool ChildWindowSurfaceWin::Resize(const gfx::Size& size,
                                   float scale_factor,
                                   bool has_alpha) {
  if (!SupportsPostSubBuffer()) {
    if (!MoveWindow(window_, 0, 0, size.width(), size.height(), FALSE)) {
      return false;
    }
    return gfx::NativeViewGLSurfaceEGL::Resize(size, scale_factor, has_alpha);
  } else {
    if (size == GetSize() && has_alpha == alpha_)
      return true;

    alpha_ = has_alpha;
    size_ = size;
    if (!MoveWindow(window_, 0, 0, size.width(), size.height(), FALSE)) {
      return false;
    }
    // A 0-size PostSubBuffer doesn't swap but forces the swap chain to resize
    // to match the window.
    PostSubBuffer(0, 0, 0, 0);
    return true;
  }
}

ChildWindowSurfaceWin::~ChildWindowSurfaceWin() {
  DestroyWindow(window_);
}

}  // namespace content
