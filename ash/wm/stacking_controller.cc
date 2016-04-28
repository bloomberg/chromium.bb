// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/stacking_controller.h"

#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/root_window_finder.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// Find a root window that matches the |bounds|. If the virtual screen
// coordinates is enabled and the bounds is specified, the root window
// that matches the window's bound will be used. Otherwise, it'll
// return the active root window.
aura::Window* FindContainerRoot(const gfx::Rect& bounds) {
  if (bounds.x() == 0 && bounds.y() == 0 && bounds.IsEmpty())
    return Shell::GetTargetRootWindow();
  return wm::WmWindowAura::GetAuraWindow(wm::GetRootWindowMatching(bounds));
}

aura::Window* GetContainerById(aura::Window* root, int id) {
  return Shell::GetContainer(root, id);
}

bool IsSystemModal(aura::Window* window) {
  return window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_SYSTEM;
}

bool HasTransientParentWindow(const aura::Window* window) {
  return ::wm::GetTransientParent(window) &&
      ::wm::GetTransientParent(window)->type() !=
      ui::wm::WINDOW_TYPE_UNKNOWN;
}

AlwaysOnTopController* GetAlwaysOnTopController(aura::Window* root_window) {
  return GetRootWindowController(root_window)->always_on_top_controller();
}

aura::Window* GetContainerFromAlwaysOnTopController(aura::Window* root,
                                                    aura::Window* window) {
  return wm::WmWindowAura::GetAuraWindow(
      GetAlwaysOnTopController(root)->GetContainer(
          wm::WmWindowAura::Get(window)));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// StackingController, public:

StackingController::StackingController() {
}

StackingController::~StackingController() {
}

////////////////////////////////////////////////////////////////////////////////
// StackingController, aura::client::WindowTreeClient implementation:

aura::Window* StackingController::GetDefaultParent(aura::Window* context,
                                                   aura::Window* window,
                                                   const gfx::Rect& bounds) {
  aura::Window* target_root = NULL;
  aura::Window* transient_parent = ::wm::GetTransientParent(window);
  if (transient_parent) {
    // Transient window should use the same root as its transient parent.
    target_root = transient_parent->GetRootWindow();
  } else {
    target_root = FindContainerRoot(bounds);
  }

  switch (window->type()) {
    case ui::wm::WINDOW_TYPE_NORMAL:
    case ui::wm::WINDOW_TYPE_POPUP:
      if (IsSystemModal(window))
        return GetSystemModalContainer(target_root, window);
      else if (HasTransientParentWindow(window))
        return RootWindowController::GetContainerForWindow(
            ::wm::GetTransientParent(window));
      return GetContainerFromAlwaysOnTopController(target_root, window);
    case ui::wm::WINDOW_TYPE_CONTROL:
      return GetContainerById(target_root,
                              kShellWindowId_UnparentedControlContainer);
    case ui::wm::WINDOW_TYPE_PANEL:
      if (wm::GetWindowState(window)->panel_attached())
        return GetContainerById(target_root, kShellWindowId_PanelContainer);
      else
        return GetContainerFromAlwaysOnTopController(target_root, window);
    case ui::wm::WINDOW_TYPE_MENU:
      return GetContainerById(target_root, kShellWindowId_MenuContainer);
    case ui::wm::WINDOW_TYPE_TOOLTIP:
      return GetContainerById(target_root,
                              kShellWindowId_DragImageAndTooltipContainer);
    default:
      NOTREACHED() << "Window " << window->id()
                   << " has unhandled type " << window->type();
      break;
  }
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// StackingController, private:

aura::Window* StackingController::GetSystemModalContainer(
    aura::Window* root,
    aura::Window* window) const {
  DCHECK(IsSystemModal(window));

  // If screen lock is not active and user session is active,
  // all modal windows are placed into the normal modal container.
  // In case of missing transient parent (it could happen for alerts from
  // background pages) assume that the window belongs to user session.
  SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();
  if (!session_state_delegate->IsUserSessionBlocked() ||
      !::wm::GetTransientParent(window)) {
    return GetContainerById(root, kShellWindowId_SystemModalContainer);
  }

  // Otherwise those that originate from LockScreen container and above are
  // placed in the screen lock modal container.
  int window_container_id =
      ::wm::GetTransientParent(window)->parent()->id();
  aura::Window* container = NULL;
  if (window_container_id < kShellWindowId_LockScreenContainer) {
    container = GetContainerById(root, kShellWindowId_SystemModalContainer);
  } else {
    container = GetContainerById(root, kShellWindowId_LockSystemModalContainer);
  }

  return container;
}

}  // namespace ash
