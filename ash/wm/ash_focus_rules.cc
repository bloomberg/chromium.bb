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

bool AshFocusRules::SupportsChildActivation(aura::Window* window) const {
  if (window->id() == internal::kShellWindowId_WorkspaceContainer)
    return true;

  if (window->id() == internal::kShellWindowId_DefaultContainer)
    return false;

  for (size_t i = 0; i < arraysize(kWindowContainerIds); i++) {
    if (window->id() == kWindowContainerIds[i])
      return true;
  }
  return false;
}

bool AshFocusRules::IsWindowConsideredVisibleForActivation(
    aura::Window* window) const {
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

bool AshFocusRules::CanActivateWindow(aura::Window* window) const {
  // Clearing activation is always permissible.
  if (!window)
    return true;

  if (!BaseFocusRules::CanActivateWindow(window))
    return false;

  if (Shell::GetInstance()->IsSystemModalWindowOpen()) {
    return BelongsToContainerWithEqualOrGreaterId(
          window, internal::kShellWindowId_SystemModalContainer);
  }

  return true;
}

aura::Window* AshFocusRules::GetNextActivatableWindow(
    aura::Window* ignore) const {
  DCHECK(ignore);

  int starting_container_index = 0;
  // If the container of the window losing focus is in the list, start from that
  // container.
  aura::RootWindow* root = ignore->GetRootWindow();
  if (!root)
    root = Shell::GetActiveRootWindow();
  int container_count = static_cast<int>(arraysize(kWindowContainerIds));
  for (int i = 0; ignore && i < container_count; i++) {
    aura::Window* container = Shell::GetContainer(root, kWindowContainerIds[i]);
    if (container && container->Contains(ignore)) {
      starting_container_index = i;
      break;
    }
  }

  // Look for windows to focus in |ignore|'s container. If none are found, we
  // look in all the containers in front of |ignore|'s container, then all
  // behind.
  aura::Window* window = NULL;
  for (int i = starting_container_index; !window && i < container_count; i++)
    window = GetTopmostWindowToActivateForContainerIndex(i, ignore);
  if (!window && starting_container_index > 0) {
    for (int i = starting_container_index - 1; !window && i >= 0; i--)
      window = GetTopmostWindowToActivateForContainerIndex(i, ignore);
  }
  return window;
}

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, private:

aura::Window* AshFocusRules::GetTopmostWindowToActivateForContainerIndex(
    int index,
    aura::Window* ignore) const {
  aura::Window* window = NULL;
  aura::RootWindow* root = ignore ? ignore->GetRootWindow() : NULL;
  aura::Window::Windows containers = Shell::GetContainersFromAllRootWindows(
      kWindowContainerIds[index], root);
  for (aura::Window::Windows::const_iterator iter = containers.begin();
        iter != containers.end() && !window; ++iter) {
    window = GetTopmostWindowToActivateInContainer((*iter), ignore);
  }
  return window;
}

aura::Window* AshFocusRules::GetTopmostWindowToActivateInContainer(
    aura::Window* container,
    aura::Window* ignore) const {
  // Workspace has an extra level of windows that needs to be special cased.
  if (container->id() == internal::kShellWindowId_DefaultContainer) {
    for (aura::Window::Windows::const_reverse_iterator i =
             container->children().rbegin();
         i != container->children().rend(); ++i) {
      if ((*i)->IsVisible()) {
        aura::Window* window =
            GetTopmostWindowToActivateInContainer(*i, ignore);
        if (window)
          return window;
      }
    }
    return NULL;
  }
  for (aura::Window::Windows::const_reverse_iterator i =
           container->children().rbegin();
       i != container->children().rend();
       ++i) {
    if (*i != ignore && CanActivateWindow(*i) && !wm::IsWindowMinimized(*i))
      return *i;
  }
  return NULL;
}

}  // namespace wm
}  // namespace ash
