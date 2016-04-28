// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/fullscreen_window_finder.h"

#include "ash/wm/common/switchable_windows.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/common/wm_globals.h"
#include "ash/wm/common/wm_shell_window_ids.h"
#include "ash/wm/common/wm_window.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace wm {

WmWindow* GetWindowForFullscreenMode(WmWindow* context) {
  WmWindow* topmost_window = nullptr;
  WmWindow* active_window = context->GetGlobals()->GetActiveWindow();
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
          (*iter)->GetLayer()->GetTargetVisibility()) {
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
