// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include <vector>

#include "ash/root_window_controller.h"
#include "chrome/browser/ui/host_desktop.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"
#include "ui/views/widget/widget.h"

bool IsFullScreenMode() {
#if defined(USE_ASH)
  if (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH) {
    ash::RootWindowController* controller =
        ash::RootWindowController::ForTargetRootWindow();
    return controller && controller->GetWindowForFullscreenMode();
  }
#endif

  std::vector<aura::Window*> all_windows =
      views::DesktopWindowTreeHostX11::GetAllOpenWindows();
  // Only the topmost window is checked. This works fine in the most cases, but
  // it may return false when there are multiple displays and one display has
  // a fullscreen window but others don't. See: crbug.com/345484
  if (all_windows.empty())
    return false;

  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(all_windows[0]);
  return widget && widget->IsFullscreen();
}
