// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor/system_monitor.h"

#include "base/win/wrapped_window_proc.h"

namespace base {

namespace {

const wchar_t kWindowClassName[] = L"Base_PowerMessageWindow";

}  // namespace

// Function to query the system to see if it is currently running on
// battery power.  Returns true if running on battery.
bool SystemMonitor::IsBatteryPower() {
  SYSTEM_POWER_STATUS status;
  if (!GetSystemPowerStatus(&status)) {
    DLOG_GETLASTERROR(ERROR) << "GetSystemPowerStatus failed";
    return false;
  }
  return (status.ACLineStatus == 0);
}

SystemMonitor::PowerMessageWindow::PowerMessageWindow()
    : instance_(NULL), message_hwnd_(NULL) {
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      kWindowClassName,
      &base::win::WrappedWindowProc<
          SystemMonitor::PowerMessageWindow::WndProcThunk>,
      0, 0, 0, NULL, NULL, NULL, NULL, NULL,
      &window_class);
  instance_ = window_class.hInstance;
  ATOM clazz = RegisterClassEx(&window_class);
  DCHECK(clazz);

  message_hwnd_ = CreateWindowEx(WS_EX_NOACTIVATE, kWindowClassName,
      NULL, WS_POPUP, 0, 0, 0, 0, NULL, NULL, instance_, NULL);
  SetWindowLongPtr(message_hwnd_, GWLP_USERDATA,
                   reinterpret_cast<LONG_PTR>(this));
}

SystemMonitor::PowerMessageWindow::~PowerMessageWindow() {
  if (message_hwnd_) {
    DestroyWindow(message_hwnd_);
    UnregisterClass(kWindowClassName, instance_);
  }
}

void SystemMonitor::PowerMessageWindow::ProcessWmPowerBroadcastMessage(
    int event_id) {
  SystemMonitor::PowerEvent power_event;
  switch (event_id) {
    case PBT_APMPOWERSTATUSCHANGE:  // The power status changed.
      power_event = SystemMonitor::POWER_STATE_EVENT;
      break;
    case PBT_APMRESUMEAUTOMATIC:  // Resume from suspend.
    //case PBT_APMRESUMESUSPEND:  // User-initiated resume from suspend.
                                  // We don't notify for this latter event
                                  // because if it occurs it is always sent as a
                                  // second event after PBT_APMRESUMEAUTOMATIC.
      power_event = SystemMonitor::RESUME_EVENT;
      break;
    case PBT_APMSUSPEND:  // System has been suspended.
      power_event = SystemMonitor::SUSPEND_EVENT;
      break;
    default:
      return;

    // Other Power Events:
    // PBT_APMBATTERYLOW - removed in Vista.
    // PBT_APMOEMEVENT - removed in Vista.
    // PBT_APMQUERYSUSPEND - removed in Vista.
    // PBT_APMQUERYSUSPENDFAILED - removed in Vista.
    // PBT_APMRESUMECRITICAL - removed in Vista.
    // PBT_POWERSETTINGCHANGE - user changed the power settings.
  }

  SystemMonitor::Get()->ProcessPowerMessage(power_event);
}

LRESULT CALLBACK SystemMonitor::PowerMessageWindow::WndProc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_POWERBROADCAST: {
      DWORD power_event = static_cast<DWORD>(message);
      ProcessWmPowerBroadcastMessage(power_event);
      return TRUE;
    }
    default:
      break;
  }
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

// static
LRESULT CALLBACK SystemMonitor::PowerMessageWindow::WndProcThunk(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  SystemMonitor::PowerMessageWindow* message_hwnd =
      reinterpret_cast<SystemMonitor::PowerMessageWindow*>(
          GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (message_hwnd)
    return message_hwnd->WndProc(hwnd, message, wparam, lparam);
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

}  // namespace base
