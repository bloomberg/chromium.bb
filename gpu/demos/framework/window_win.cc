// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/demo.h"
#include "gpu/demos/framework/window.h"

namespace {
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
                            WPARAM w_param, LPARAM l_param) {
  LRESULT result = 0;
  switch (msg) {
    case WM_CLOSE:
      ::DestroyWindow(hwnd);
      break;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      break;
    case WM_ERASEBKGND:
      // Return a non-zero value to indicate that the background has been
      // erased.
      result = 1;
      break;
    case WM_PAINT: {
      gpu::demos::Window* window = reinterpret_cast<gpu::demos::Window*>(
          GetWindowLongPtr(hwnd, GWL_USERDATA));
      if (window != NULL) window->OnPaint();
      ::ValidateRect(hwnd, NULL);
      break;
    }
    default:
      result = ::DefWindowProc(hwnd, msg, w_param, l_param);
      break;
  }
  return result;
}
}  // namespace.

namespace gpu {
namespace demos {

void Window::MainLoop() {
  // Set this to the GWL_USERDATA so that it is available to WindowProc.
  SetWindowLongPtr(window_handle_, GWL_USERDATA,
                   reinterpret_cast<LONG_PTR>(this));

  MSG msg;
  bool done = false;
  while (!done) {
    while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) done = true;
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
    // Message queue is empty and application has not quit yet - keep painting.
    if (!done && demo_->IsAnimated())
      ::InvalidateRect(window_handle_,NULL, FALSE);
  }
}

gfx::NativeWindow Window::CreateNativeWindow(const wchar_t* title,
                                             int width, int height) {
  WNDCLASS wnd_class = {0};
  HINSTANCE instance = GetModuleHandle(NULL);
  wnd_class.style = CS_OWNDC;
  wnd_class.lpfnWndProc = WindowProc;
  wnd_class.hInstance = instance;
  wnd_class.hbrBackground =
      reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  wnd_class.lpszClassName = L"gpu_demo";
  if (!RegisterClass(&wnd_class)) return NULL;

  DWORD wnd_style = WS_OVERLAPPED | WS_SYSMENU;
  RECT wnd_rect;
  wnd_rect.left = 0;
  wnd_rect.top = 0;
  wnd_rect.right = width;
  wnd_rect.bottom = height;
  AdjustWindowRect(&wnd_rect, wnd_style, FALSE);

  HWND hwnd = CreateWindow(
      wnd_class.lpszClassName,
      title,
      wnd_style,
      0,
      0,
      wnd_rect.right - wnd_rect.left,
      wnd_rect.bottom - wnd_rect.top,
      NULL,
      NULL,
      instance,
      NULL);
  if (hwnd == NULL) return NULL;

  ShowWindow(hwnd, SW_SHOWNORMAL);
  return hwnd;
}

gfx::AcceleratedWidget Window::PluginWindow(gfx::NativeWindow hwnd) {
  return hwnd;
}

}  // namespace demos
}  // namespace gpu

