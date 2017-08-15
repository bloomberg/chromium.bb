// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_action_handler_layout_manager.h"

#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "ash/shell.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wm/lock_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

LockActionHandlerLayoutManager::LockActionHandlerLayoutManager(
    aura::Window* window,
    Shelf* shelf)
    : LockLayoutManager(window, shelf), tray_action_observer_(this) {
  TrayAction* tray_action = Shell::Get()->tray_action();
  tray_action_observer_.Add(tray_action);
}

LockActionHandlerLayoutManager::~LockActionHandlerLayoutManager() = default;

void LockActionHandlerLayoutManager::OnWindowAddedToLayout(
    aura::Window* child) {
  ::wm::SetWindowVisibilityAnimationType(
      child, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);

  wm::WindowState* window_state =
      LockWindowState::SetLockWindowStateWithShelfExcluded(child);
  wm::WMEvent event(wm::WM_EVENT_ADDED_TO_WORKSPACE);
  window_state->OnWMEvent(&event);
}

void LockActionHandlerLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  // Windows should be shown only in active state.
  if (visible && !Shell::Get()->tray_action()->IsLockScreenNoteActive())
    child->Hide();
}

void LockActionHandlerLayoutManager::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {
  // Make sure the container is properly stacked relative to lock screen
  // container - lock action handler should be above lock screen only when a
  // lock screen action is active.
  if (state == mojom::TrayActionState::kActive) {
    window()->parent()->StackChildAbove(
        window(),
        root_window()->GetChildById(kShellWindowId_LockScreenContainer));
  } else {
    window()->parent()->StackChildBelow(
        window(),
        root_window()->GetChildById(kShellWindowId_LockScreenContainer));
  }

  // Update children state:
  // * a child can be visible only in background and active states
  // * children should not be active in background state
  // * on transition to active state:
  //     * show hidden windows, so children that were added when action was not
  //       in active state are shown
  //     * activate a container child to ensure the container gets focus when
  //       moving from background state.
  for (aura::Window* child : window()->children()) {
    if (state == mojom::TrayActionState::kActive) {
      child->Show();
    } else if (state != mojom::TrayActionState::kBackground) {
      child->Hide();
    } else if (wm::IsActiveWindow(child)) {
      wm::DeactivateWindow(child);
    }
  }

  if (!window()->children().empty() &&
      state == mojom::TrayActionState::kActive) {
    wm::ActivateWindow(window()->children().back());
  }
}

}  // namespace ash
