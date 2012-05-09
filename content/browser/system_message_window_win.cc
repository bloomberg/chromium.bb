// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/system_message_window_win.h"

#include <windows.h>
#include <dbt.h>

#include "base/logging.h"
#include "base/system_monitor/system_monitor.h"
#include "base/win/wrapped_window_proc.h"

static const wchar_t* const WindowClassName = L"Chrome_SystemMessageWindow";


SystemMessageWindowWin::SystemMessageWindowWin() {
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      WindowClassName,
      &base::win::WrappedWindowProc<SystemMessageWindowWin::WndProcThunk>,
      0, 0, 0, NULL, NULL, NULL, NULL, NULL,
      &window_class);
  instance_ = window_class.hInstance;
  ATOM clazz = RegisterClassEx(&window_class);
  DCHECK(clazz);

  window_ = CreateWindow(WindowClassName,
                         0, 0, 0, 0, 0, 0, 0, 0, instance_, 0);
  SetWindowLongPtr(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

SystemMessageWindowWin::~SystemMessageWindowWin() {
  if (window_) {
    DestroyWindow(window_);
    UnregisterClass(WindowClassName, instance_);
  }
}

LRESULT SystemMessageWindowWin::OnDeviceChange(UINT event_type, DWORD data) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  switch (event_type) {
    case DBT_DEVNODES_CHANGED:
      monitor->ProcessDevicesChanged();
      break;
  }
  return TRUE;
}

LRESULT CALLBACK SystemMessageWindowWin::WndProc(HWND hwnd, UINT message,
                                                 WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_DEVICECHANGE:
      return OnDeviceChange(static_cast<UINT>(wparam),
                            static_cast<DWORD>(lparam));
    default:
      break;
  }

  return ::DefWindowProc(hwnd, message, wparam, lparam);
}
