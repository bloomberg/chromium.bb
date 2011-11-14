// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/system_message_window_win.h"

#include <windows.h>
#include <dbt.h>

#include "base/system_monitor/system_monitor.h"
#include "base/win/wrapped_window_proc.h"

static const wchar_t* const WindowClassName = L"Chrome_SystemMessageWindow";

SystemMessageWindowWin::SystemMessageWindowWin() {
  HINSTANCE hinst = GetModuleHandle(NULL);

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc =
      base::win::WrappedWindowProc<&SystemMessageWindowWin::WndProcThunk>;
  wc.hInstance = hinst;
  wc.lpszClassName = WindowClassName;
  ATOM clazz = RegisterClassEx(&wc);
  DCHECK(clazz);

  window_ = CreateWindow(WindowClassName,
                         0, 0, 0, 0, 0, 0, 0, 0, hinst, 0);
  SetWindowLongPtr(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

SystemMessageWindowWin::~SystemMessageWindowWin() {
  if (window_) {
    DestroyWindow(window_);
    UnregisterClass(WindowClassName, GetModuleHandle(NULL));
  }
}

LRESULT SystemMessageWindowWin::OnDeviceChange(UINT event_type, DWORD data) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor && event_type == DBT_DEVNODES_CHANGED)
    monitor->ProcessDevicesChanged();
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
