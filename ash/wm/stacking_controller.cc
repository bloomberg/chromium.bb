// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/stacking_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/always_on_top_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ash {
namespace internal {
namespace {

aura::Window* GetContainer(int id) {
  return Shell::GetInstance()->GetContainer(id);
}

bool IsSystemModal(aura::Window* window) {
  return window->transient_parent() &&
      window->GetIntProperty(aura::client::kModalKey) == ui::MODAL_TYPE_SYSTEM;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// StackingController, public:

StackingController::StackingController() {
  aura::client::SetStackingClient(this);
  always_on_top_controller_.reset(new internal::AlwaysOnTopController);
  always_on_top_controller_->SetContainers(
      GetContainer(internal::kShellWindowId_DefaultContainer),
      GetContainer(internal::kShellWindowId_AlwaysOnTopContainer));
}

StackingController::~StackingController() {
}

////////////////////////////////////////////////////////////////////////////////
// StackingController, aura::StackingClient implementation:

aura::Window* StackingController::GetDefaultParent(aura::Window* window) {
  switch (window->type()) {
    case aura::client::WINDOW_TYPE_NORMAL:
    case aura::client::WINDOW_TYPE_POPUP:
    // TODO(beng): control windows with NULL parents should be parented to a
    //             unique, probably hidden, container. Adding here now for
    //             compatibility, since these windows were WINDOW_TYPE_POPUP
    //             until now.
    case aura::client::WINDOW_TYPE_CONTROL:
      if (IsSystemModal(window))
        return GetSystemModalContainer(window);
      return always_on_top_controller_->GetContainer(window);
    case aura::client::WINDOW_TYPE_PANEL:
      return GetContainer(internal::kShellWindowId_PanelContainer);
    case aura::client::WINDOW_TYPE_MENU:
    case aura::client::WINDOW_TYPE_TOOLTIP:
      return GetContainer(internal::kShellWindowId_MenuAndTooltipContainer);
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
      GetContainer(internal::kShellWindowId_LockScreenContainer);
  if (!lock_container->children().size())
    return GetContainer(internal::kShellWindowId_SystemModalContainer);

  // Otherwise those that originate from LockScreen container and above are
  // placed in the screen lock modal container.
  int lock_container_id = lock_container->id();
  int window_container_id = window->transient_parent()->parent()->id();

  aura::Window* container = NULL;
  if (window_container_id < lock_container_id)
    container = GetContainer(internal::kShellWindowId_SystemModalContainer);
  else
    container = GetContainer(internal::kShellWindowId_LockSystemModalContainer);

  return container;
}

}  // namespace internal
}  // namespace ash
