// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/interactive_test_utils_aura.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/win/foreground_helper.h"
#include "ui/views/focus/focus_manager.h"

namespace ui_test_utils {

void HideNativeWindow(gfx::NativeWindow window) {
  if (chrome::GetHostDesktopTypeForNativeWindow(window) ==
      chrome::HOST_DESKTOP_TYPE_ASH) {
    HideNativeWindowAura(window);
    return;
  }
  HWND hwnd = window->GetHost()->GetAcceleratedWidget();
  ::ShowWindow(hwnd, SW_HIDE);
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
  if (chrome::GetHostDesktopTypeForNativeWindow(window) ==
      chrome::HOST_DESKTOP_TYPE_ASH)
    ShowAndFocusNativeWindowAura(window);
  window->Show();
  // Always make sure the window hosting ash is visible and focused.
  HWND hwnd = window->GetHost()->GetAcceleratedWidget();

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
