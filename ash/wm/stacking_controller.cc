// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/stacking_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ash {
namespace internal {
namespace {

aura::Window* GetContainerById(int id) {
  return Shell::GetContainer(Shell::GetActiveRootWindow(), id);
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

bool IsWindowModal(aura::Window* window) {
  return window->transient_parent() &&
      window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_WINDOW;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// StackingController, public:

StackingController::StackingController() {
  aura::client::SetStackingClient(this);
}

StackingController::~StackingController() {
}

////////////////////////////////////////////////////////////////////////////////
// StackingController, aura::StackingClient implementation:

aura::Window* StackingController::GetDefaultParent(aura::Window* window) {
  switch (window->type()) {
    case aura::client::WINDOW_TYPE_NORMAL:
    case aura::client::WINDOW_TYPE_POPUP:
      if (IsSystemModal(window))
        return GetSystemModalContainer(window);
      else if (IsWindowModal(window))
        return GetContainerForWindow(window->transient_parent());
      return GetAlwaysOnTopController()->GetContainer(window);
    case aura::client::WINDOW_TYPE_PANEL:
      return GetContainerById(internal::kShellWindowId_PanelContainer);
    case aura::client::WINDOW_TYPE_MENU:
      return GetContainerById(internal::kShellWindowId_MenuContainer);
    case aura::client::WINDOW_TYPE_TOOLTIP:
      return GetContainerById(
          internal::kShellWindowId_DragImageAndTooltipContainer);
    case aura::client::WINDOW_TYPE_CONTROL:
      return GetContainerById(
          internal::kShellWindowId_UnparentedControlContainer);
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
    aura::Window* window) const {
  if (!IsSystemModal(window))
    return NULL;

  // If screen lock is not active, all modal windows are placed into the
  // normal modal container.
  aura::Window* lock_container =
      GetContainerById(internal::kShellWindowId_LockScreenContainer);
  if (!lock_container->children().size())
    return GetContainerById(internal::kShellWindowId_SystemModalContainer);

  // Otherwise those that originate from LockScreen container and above are
  // placed in the screen lock modal container.
  int lock_container_id = lock_container->id();
  int window_container_id = window->transient_parent()->parent()->id();

  aura::Window* container = NULL;
  if (window_container_id < lock_container_id) {
    container = GetContainerById(
        internal::kShellWindowId_SystemModalContainer);
  } else {
    container = GetContainerById(
        internal::kShellWindowId_LockSystemModalContainer);
  }

  return container;
}

internal::AlwaysOnTopController*
StackingController::GetAlwaysOnTopController() {
  aura::RootWindow* root_window = Shell::GetActiveRootWindow();
  internal::AlwaysOnTopController* controller =
      root_window->GetProperty(internal::kAlwaysOnTopControllerKey);
  if (!controller) {
    controller = new internal::AlwaysOnTopController;
    controller->SetContainers(
        root_window->GetChildById(internal::kShellWindowId_DefaultContainer),
        root_window->GetChildById(
            internal::kShellWindowId_AlwaysOnTopContainer));
    // RootWindow owns the AlwaysOnTopController object.
    root_window->SetProperty(kAlwaysOnTopControllerKey, controller);
  }
  return controller;
}

}  // namespace internal
}  // namespace ash
