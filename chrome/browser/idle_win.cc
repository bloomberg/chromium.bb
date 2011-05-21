// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include <limits.h>
#include <windows.h>

static bool IsScreensaverRunning();
static bool IsWorkstationLocked();

IdleState CalculateIdleState(unsigned int idle_threshold) {
  if (IsScreensaverRunning() || IsWorkstationLocked())
    return IDLE_STATE_LOCKED;

  LASTINPUTINFO last_input_info = {0};
  last_input_info.cbSize = sizeof(LASTINPUTINFO);
  DWORD current_idle_time = 0;
  if (::GetLastInputInfo(&last_input_info)) {
    DWORD now = ::GetTickCount();
    if (now < last_input_info.dwTime) {
      // GetTickCount() wraps around every 49.7 days -- assume it wrapped just
      // once.
      const DWORD kMaxDWORD = static_cast<DWORD>(-1);
      DWORD time_before_wrap = kMaxDWORD - last_input_info.dwTime;
      DWORD time_after_wrap = now;
      // The sum is always smaller than kMaxDWORD.
      current_idle_time = time_before_wrap + time_after_wrap;
    } else {
      current_idle_time = now - last_input_info.dwTime;
    }

    // Convert from ms to seconds.
    current_idle_time /= 1000;
  }

  if (current_idle_time >= idle_threshold)
    return IDLE_STATE_IDLE;
  return IDLE_STATE_ACTIVE;
}

bool IsScreensaverRunning() {
  DWORD result = 0;
  if (::SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, 0, &result, 0))
    return result != FALSE;
  return false;
}

bool IsWorkstationLocked() {
  bool is_locked = true;
  HDESK input_desk = ::OpenInputDesktop(0, 0, GENERIC_READ);
  if (input_desk)  {
    wchar_t name[256] = {0};
    DWORD needed = 0;
    if (::GetUserObjectInformation(input_desk,
      UOI_NAME,
      name,
      sizeof(name),
      &needed)) {
        is_locked = lstrcmpi(name, L"default") != 0;
    }
    ::CloseDesktop(input_desk);
  }
  return is_locked;
}
