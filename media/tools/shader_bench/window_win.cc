// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/shader_bench/window.h"

#include "media/tools/shader_bench/painter.h"

namespace media {

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg,
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
      Window* window =
          reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
      if (window != NULL)
        window->OnPaint();
      ::ValidateRect(hwnd, NULL);
      break;
    }
    default:
      result = ::DefWindowProc(hwnd, msg, w_param, l_param);
      break;
  }
  return result;
}

gfx::NativeWindow Window::CreateNativeWindow(int width, int height) {
  WNDCLASS wnd_class = {0};
  HINSTANCE instance = GetModuleHandle(NULL);
  wnd_class.style = CS_OWNDC;
  wnd_class.lpfnWndProc = WindowProc;
  wnd_class.hInstance = instance;
  wnd_class.hbrBackground =
      reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  wnd_class.lpszClassName = L"gpu_demo";
  if (!RegisterClass(&wnd_class))
    return NULL;

  DWORD wnd_style = WS_OVERLAPPED | WS_SYSMENU;
  RECT wnd_rect;
  wnd_rect.left = 0;
  wnd_rect.top = 0;
  wnd_rect.right = width;
  wnd_rect.bottom = height;
  AdjustWindowRect(&wnd_rect, wnd_style, FALSE);

  HWND hwnd = CreateWindow(
      wnd_class.lpszClassName,
      L"",
      wnd_style,
      0,
      0,
      wnd_rect.right - wnd_rect.left,
      wnd_rect.bottom - wnd_rect.top,
      NULL,
      NULL,
      instance,
      NULL);
  if (hwnd == NULL)
    return NULL;

  return hwnd;
}

gfx::PluginWindowHandle Window::PluginWindow() {
  return window_handle_;
}

void Window::Start(int limit, const base::Closure& callback,
                   Painter* painter) {
  running_ = true;
  count_ = 0;
  limit_ = limit;
  callback_ = callback;
  painter_ = painter;

  SetWindowLongPtr(window_handle_, GWLP_USERDATA,
                   reinterpret_cast<LONG_PTR>(this));

  ShowWindow(window_handle_, SW_SHOWNORMAL);

  // Post first invalidate call to kick off painting.
  ::InvalidateRect(window_handle_, NULL, FALSE);

  MainLoop();
}

void Window::OnPaint() {
  if (!running_)
    return;

  if (count_ < limit_) {
    painter_->OnPaint();
    count_++;
  } else {
    running_ = false;
    if (!callback_.is_null()) {
      ShowWindow(window_handle_, SW_HIDE);
      callback_.Run();
      callback_.Reset();
    }
  }
}

void Window::MainLoop() {
  MSG msg;
  bool done = false;
  while (!done) {
    while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT || !running_)
        done = true;
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      if (!done)
        ::InvalidateRect(window_handle_, NULL, FALSE);
    }
  }
}

}  // namespace media
