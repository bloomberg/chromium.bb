// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/stacking_controller.h"

#include "ash/monitor/monitor_controller.h"
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

// Find a root window that matches the |bounds|. If the virtual screen
// coordinates is enabled and the bounds is specified, the root window
// that matches the window's bound will be used. Otherwise, it'll
// return the active root window.
aura::RootWindow* FindContainerRoot(const gfx::Rect& bounds) {
  if (!MonitorController::IsVirtualScreenCoordinatesEnabled() ||
      (bounds.origin().x() == 0 && bounds.origin().y() == 0
       && bounds.IsEmpty())) {
    return Shell::GetActiveRootWindow();
  }
  return Shell::GetRootWindowMatching(bounds);
}

aura::Window* GetContainerById(const gfx::Rect& bounds, int id) {
  return Shell::GetContainer(FindContainerRoot(bounds), id);
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

aura::Window* StackingController::GetDefaultParent(aura::Window* window,
                                                   const gfx::Rect& bounds) {
  switch (window->type()) {
    case aura::client::WINDOW_TYPE_NORMAL:
    case aura::client::WINDOW_TYPE_POPUP:
      if (IsSystemModal(window))
        return GetSystemModalContainer(window, bounds);
      else if (IsWindowModal(window))
        return GetContainerForWindow(window->transient_parent());
      return GetAlwaysOnTopController(bounds)->GetContainer(window);
    case aura::client::WINDOW_TYPE_PANEL:
      return GetContainerById(bounds, internal::kShellWindowId_PanelContainer);
    case aura::client::WINDOW_TYPE_MENU:
      return GetContainerById(bounds, internal::kShellWindowId_MenuContainer);
    case aura::client::WINDOW_TYPE_TOOLTIP:
      return GetContainerById(
          bounds, internal::kShellWindowId_DragImageAndTooltipContainer);
    case aura::client::WINDOW_TYPE_CONTROL:
      return GetContainerById(
          bounds, internal::kShellWindowId_UnparentedControlContainer);
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
    aura::Window* window,
    const gfx::Rect& bounds) const {
  DCHECK(IsSystemModal(window));

  // If screen lock is not active, all modal windows are placed into the
  // normal modal container.
  // TODO(oshima): support multiple root windows.
  aura::Window* lock_container =
      GetContainerById(bounds, internal::kShellWindowId_LockScreenContainer);
  if (!lock_container->children().size()) {
    return GetContainerById(bounds,
                            internal::kShellWindowId_SystemModalContainer);
  }

  // Otherwise those that originate from LockScreen container and above are
  // placed in the screen lock modal container.
  int lock_container_id = lock_container->id();
  int window_container_id = window->transient_parent()->parent()->id();

  aura::Window* container = NULL;
  if (window_container_id < lock_container_id) {
    container = GetContainerById(
        bounds, internal::kShellWindowId_SystemModalContainer);
  } else {
    container = GetContainerById(
        bounds, internal::kShellWindowId_LockSystemModalContainer);
  }

  return container;
}

internal::AlwaysOnTopController*
StackingController::GetAlwaysOnTopController(const gfx::Rect& bounds) {
  aura::RootWindow* root_window = FindContainerRoot(bounds);
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
