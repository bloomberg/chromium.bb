// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/stacking_controller.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ash {
namespace {

// Find a root window that matches the |bounds|. If the virtual screen
// coordinates is enabled and the bounds is specified, the root window
// that matches the window's bound will be used. Otherwise, it'll
// return the active root window.
aura::RootWindow* FindContainerRoot(const gfx::Rect& bounds) {
  if (bounds.x() == 0 && bounds.y() == 0 && bounds.IsEmpty())
    return Shell::GetActiveRootWindow();
  return wm::GetRootWindowMatching(bounds);
}

aura::Window* GetContainerById(aura::RootWindow* root, int id) {
  return Shell::GetContainer(root, id);
}

aura::Window* GetContainerForWindow(aura::Window* window) {
  aura::Window* container = window->parent();
  while (container && container->type() != aura::client::WINDOW_TYPE_UNKNOWN)
    container = container->parent();
  return container;
}

bool IsSystemModal(aura::Window* window) {
  return window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_SYSTEM;
}

bool HasTransientParentWindow(aura::Window* window) {
  return window->transient_parent() &&
      window->transient_parent()->type() != aura::client::WINDOW_TYPE_UNKNOWN;
}

bool IsPanelAttached(aura::Window* window) {
  return window->GetProperty(internal::kPanelAttachedKey);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// StackingController, public:

StackingController::StackingController() {
}

StackingController::~StackingController() {
}

////////////////////////////////////////////////////////////////////////////////
// StackingController, aura::StackingClient implementation:

aura::Window* StackingController::GetDefaultParent(aura::Window* context,
                                                   aura::Window* window,
                                                   const gfx::Rect& bounds) {
  aura::RootWindow* target_root = NULL;
  if (window->transient_parent()) {
    // Transient window should use the same root as its transient parent.
    target_root = window->transient_parent()->GetRootWindow();
  } else {
    target_root = FindContainerRoot(bounds);
  }

  switch (window->type()) {
    case aura::client::WINDOW_TYPE_NORMAL:
    case aura::client::WINDOW_TYPE_POPUP:
      if (IsSystemModal(window))
        return GetSystemModalContainer(target_root, window);
      else if (HasTransientParentWindow(window))
        return GetContainerForWindow(window->transient_parent());
      return GetAlwaysOnTopController(target_root)->GetContainer(window);
    case aura::client::WINDOW_TYPE_CONTROL:
      return GetContainerById(
          target_root, internal::kShellWindowId_UnparentedControlContainer);
    case aura::client::WINDOW_TYPE_PANEL:
      if (IsPanelAttached(window))
        return GetContainerById(target_root,
                                internal::kShellWindowId_PanelContainer);
      else
        return GetAlwaysOnTopController(target_root)->GetContainer(window);
    case aura::client::WINDOW_TYPE_MENU:
      return GetContainerById(
          target_root, internal::kShellWindowId_MenuContainer);
    case aura::client::WINDOW_TYPE_TOOLTIP:
      return GetContainerById(
          target_root, internal::kShellWindowId_DragImageAndTooltipContainer);
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
    aura::RootWindow* root,
    aura::Window* window) const {
  DCHECK(IsSystemModal(window));

  // If screen lock is not active and user session is active,
  // all modal windows are placed into the normal modal container.
  if (!Shell::GetInstance()->delegate()->IsScreenLocked() &&
      Shell::GetInstance()->delegate()->IsSessionStarted()) {
    return GetContainerById(root,
                            internal::kShellWindowId_SystemModalContainer);
  }

  // Otherwise those that originate from LockScreen container and above are
  // placed in the screen lock modal container.
  aura::Window* lock_container =
      GetContainerById(root, internal::kShellWindowId_LockScreenContainer);
  int lock_container_id = lock_container->id();
  int window_container_id = window->transient_parent()->parent()->id();

  aura::Window* container = NULL;
  if (window_container_id < lock_container_id) {
    container = GetContainerById(
        root, internal::kShellWindowId_SystemModalContainer);
  } else {
    container = GetContainerById(
        root, internal::kShellWindowId_LockSystemModalContainer);
  }

  return container;
}

// TODO(oshima): Remove this once extended desktop is on by default.
internal::AlwaysOnTopController*
StackingController::GetAlwaysOnTopController(aura::RootWindow* root_window) {
  internal::AlwaysOnTopController* controller =
      root_window->GetProperty(internal::kAlwaysOnTopControllerKey);
  if (!controller) {
    controller = new internal::AlwaysOnTopController;
    controller->SetAlwaysOnTopContainer(
        root_window->GetChildById(
            internal::kShellWindowId_AlwaysOnTopContainer));
    // RootWindow owns the AlwaysOnTopController object.
    root_window->SetProperty(internal::kAlwaysOnTopControllerKey, controller);
  }
  return controller;
}

}  // namespace ash
