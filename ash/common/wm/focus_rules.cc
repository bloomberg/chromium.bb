// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/focus_rules.h"

#include "ash/common/shell_delegate.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"

namespace ash {

bool IsToplevelWindow(WmWindow* window) {
  DCHECK(window);
  // The window must in a valid hierarchy.
  if (!window->GetRootWindow())
    return false;

  // The window must exist within a container that supports activation.
  // The window cannot be blocked by a modal transient.
  return IsActivatableShellWindowId(window->GetParent()->GetShellWindowId());
}

bool IsWindowConsideredActivatable(WmWindow* window) {
  DCHECK(window);
  // Only toplevel windows can be activated.
  if (!IsToplevelWindow(window))
    return false;

  // The window must be visible.
  return IsWindowConsideredVisibleForActivation(window);
}

bool IsWindowConsideredVisibleForActivation(WmWindow* window) {
  DCHECK(window);
  // If the |window| doesn't belong to the current active user and also doesn't
  // show for the current active user, then it should not be activated.
  if (!WmShell::Get()->delegate()->CanShowWindowForUser(window))
    return false;

  if (window->IsVisible())
    return true;

  // Minimized windows are hidden in their minimized state, but they can always
  // be activated.
  if (window->GetWindowState()->IsMinimized())
    return true;

  if (!window->GetTargetVisibility())
    return false;

  const int parent_shell_window_id = window->GetParent()->GetShellWindowId();
  return parent_shell_window_id == kShellWindowId_DefaultContainer ||
         parent_shell_window_id == kShellWindowId_LockScreenContainer;
}

}  // namespace ash
