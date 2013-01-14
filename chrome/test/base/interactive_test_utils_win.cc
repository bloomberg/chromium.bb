// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "chrome/test/base/ui_controls.h"
#include "ui/base/win/foreground_helper.h"
#include "ui/views/focus/focus_manager.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/interactive_test_utils_aura.h"
#include "ui/aura/root_window.h"
#endif

namespace ui_test_utils {

void HideNativeWindow(gfx::NativeWindow window) {
#if defined(USE_AURA)
  if (chrome::GetHostDesktopTypeForNativeWindow(window) ==
      chrome::HOST_DESKTOP_TYPE_ASH) {
    HideNativeWindowAura(window);
    return;
  }
  HWND hwnd = window->GetRootWindow()->GetAcceleratedWidget();
#else
  HWND hwnd = window;
#endif
  ::ShowWindow(hwnd, SW_HIDE);
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
#if defined(USE_AURA)
  if (chrome::GetHostDesktopTypeForNativeWindow(window) ==
      chrome::HOST_DESKTOP_TYPE_ASH)
    ShowAndFocusNativeWindowAura(window);
  // Always make sure the window hosting ash is visible and focused.
  HWND hwnd = window->GetRootWindow()->GetAcceleratedWidget();
#else
  HWND hwnd = window;
#endif

  ::ShowWindow(hwnd, SW_SHOW);

  if (GetForegroundWindow() != hwnd) {
    VLOG(1) << "Forcefully refocusing front window";
    ui::ForegroundHelper::SetForeground(hwnd);
  }

  // ShowWindow does not necessarily activate the window. In particular if a
  // window from another app is the foreground window then the request to
  // activate the window fails. See SetForegroundWindow for details.
  return GetForegroundWindow() == hwnd;
}

}  // namespace ui_test_utils
