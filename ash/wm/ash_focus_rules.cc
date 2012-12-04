// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_focus_rules.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace wm {
namespace {

// These are the list of container ids of containers which may contain windows
// that need to be activated in the order that they should be activated.
const int kWindowContainerIds[] = {
    internal::kShellWindowId_LockSystemModalContainer,
    internal::kShellWindowId_SettingBubbleContainer,
    internal::kShellWindowId_LockScreenContainer,
    internal::kShellWindowId_SystemModalContainer,
    internal::kShellWindowId_AlwaysOnTopContainer,
    internal::kShellWindowId_AppListContainer,
    internal::kShellWindowId_DefaultContainer,

    // Panel, launcher and status are intentionally checked after other
    // containers even though these layers are higher. The user expects their
    // windows to be focused before these elements.
    internal::kShellWindowId_PanelContainer,
    internal::kShellWindowId_LauncherContainer,
    internal::kShellWindowId_StatusContainer,
};

bool BelongsToContainerWithEqualOrGreaterId(const aura::Window* window,
                                            int container_id) {
  for (; window; window = window->parent()) {
    if (window->id() >= container_id)
      return true;
  }
  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, public:

AshFocusRules::AshFocusRules() {
}

AshFocusRules::~AshFocusRules() {
}

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, views::corewm::FocusRules:

bool AshFocusRules::SupportsChildActivation(aura::Window* window) {
  if (window->id() == internal::kShellWindowId_WorkspaceContainer)
    return true;

  if (window->id() != internal::kShellWindowId_DefaultContainer)
    return false;

  for (size_t i = 0; i < arraysize(kWindowContainerIds); i++) {
    if (window->id() == kWindowContainerIds[i])
      return true;
  }
  return false;
}

bool AshFocusRules::IsWindowConsideredVisibleForActivation(
    aura::Window* window) {
  if (BaseFocusRules::IsWindowConsideredVisibleForActivation(window))
    return true;

  // Minimized windows are hidden in their minimized state, but they can always
  // be activated.
  if (wm::IsWindowMinimized(window))
    return true;

  return window->TargetVisibility() && (window->parent()->id() ==
      internal::kShellWindowId_WorkspaceContainer || window->parent()->id() ==
      internal::kShellWindowId_LockScreenContainer);
}

bool AshFocusRules::CanActivateWindow(aura::Window* window) {
  if (!BaseFocusRules::CanActivateWindow(window))
    return false;

  if (Shell::GetInstance()->IsSystemModalWindowOpen()) {
    return BelongsToContainerWithEqualOrGreaterId(
          window, internal::kShellWindowId_SystemModalContainer);
  }

  return true;
}

}  // namespace wm
}  // namespace ash
