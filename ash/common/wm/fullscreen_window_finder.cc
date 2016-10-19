// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/fullscreen_window_finder.h"

#include "ash/common/wm/switchable_windows.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace wm {

WmWindow* GetWindowForFullscreenMode(WmWindow* context) {
  WmWindow* topmost_window = nullptr;
  WmWindow* active_window = context->GetShell()->GetActiveWindow();
  if (active_window &&
      active_window->GetRootWindow() == context->GetRootWindow() &&
      IsSwitchableContainer(active_window->GetParent())) {
    // Use the active window when it is on the current root window to determine
    // the fullscreen state to allow temporarily using a panel or docked window
    // (which are always above the default container) while a fullscreen
    // window is open. We only use the active window when in a switchable
    // container as the launcher should not exit fullscreen mode.
    topmost_window = active_window;
  } else {
    // Otherwise, use the topmost window on the root window's default container
    // when there is no active window on this root window.
    std::vector<WmWindow*> windows =
        context->GetRootWindow()
            ->GetChildByShellWindowId(kShellWindowId_DefaultContainer)
            ->GetChildren();
    for (auto iter = windows.rbegin(); iter != windows.rend(); ++iter) {
      if ((*iter)->GetWindowState()->IsUserPositionable() &&
          (*iter)->GetLayerTargetVisibility()) {
        topmost_window = *iter;
        break;
      }
    }
  }
  while (topmost_window) {
    if (topmost_window->GetWindowState()->IsFullscreen())
      return topmost_window;
    topmost_window = topmost_window->GetTransientParent();
  }
  return nullptr;
}

}  // namespace wm
}  // namespace ash
