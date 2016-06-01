// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/container_ids.h"

#include "ash/public/interfaces/container.mojom.h"
#include "ash/wm/common/wm_shell_window_ids.h"

using ash::mojom::Container;

namespace mash {
namespace wm {

const Container kActivationContainers[] = {
    // TODO(sky): figure out right set of containers. I suspect this should be
    // all non containers.
    Container::USER_PRIVATE_WINDOWS,
    Container::USER_PRIVATE_ALWAYS_ON_TOP_WINDOWS,
    Container::USER_PRIVATE_DOCKED_WINDOWS,
    Container::USER_PRIVATE_PRESENTATION_WINDOWS,
    Container::USER_PRIVATE_PANELS, Container::USER_PRIVATE_APP_LIST,
    Container::USER_PRIVATE_SYSTEM_MODAL, Container::LOGIN,
    // Bubble windows must be allowed to activate because some of them rely on
    // deactivation to close.
    Container::BUBBLES,
};

const size_t kNumActivationContainers = arraysize(kActivationContainers);

Container AshContainerToMashContainer(int ash_id) {
  switch (ash_id) {
    case ash::kShellWindowId_UnparentedControlContainer:
    case ash::kShellWindowId_LockScreenContainer:
    case ash::kShellWindowId_LockSystemModalContainer:
      // We should never be asked to parent windows of these types.
      NOTREACHED();
      return Container::USER_PRIVATE;

    case ash::kShellWindowId_DefaultContainer:
      return Container::USER_PRIVATE_WINDOWS;

    case ash::kShellWindowId_AlwaysOnTopContainer:
      return Container::USER_PRIVATE_ALWAYS_ON_TOP_WINDOWS;

    case ash::kShellWindowId_DockedContainer:
      return Container::USER_PRIVATE_DOCKED_WINDOWS;

    case ash::kShellWindowId_ShelfContainer:
      return Container::USER_PRIVATE_SHELF;

    case ash::kShellWindowId_PanelContainer:
      return Container::USER_PRIVATE_PANELS;

    case ash::kShellWindowId_AppListContainer:
      return Container::USER_PRIVATE_APP_LIST;

    case ash::kShellWindowId_SystemModalContainer:
      return Container::USER_PRIVATE_SYSTEM_MODAL;

    case ash::kShellWindowId_MenuContainer:
      return Container::MENUS;

    case ash::kShellWindowId_DragImageAndTooltipContainer:
      return Container::DRAG_AND_TOOLTIPS;

    default:
      NOTREACHED();
  }
  return Container::USER_PRIVATE_WINDOWS;
}

int MashContainerToAshContainer(Container container) {
  switch (container) {
    case Container::ROOT:
      return kUnknownAshId;

    case Container::ALL_USER_BACKGROUND:
      return kUnknownAshId;

    case Container::USER:
      return kUnknownAshId;

    case Container::USER_BACKGROUND:
      return kUnknownAshId;

    case Container::USER_PRIVATE:
      return kUnknownAshId;

    case Container::USER_PRIVATE_WINDOWS:
      return ash::kShellWindowId_DefaultContainer;

    case Container::USER_PRIVATE_ALWAYS_ON_TOP_WINDOWS:
      return ash::kShellWindowId_AlwaysOnTopContainer;

    case Container::USER_PRIVATE_DOCKED_WINDOWS:
      return ash::kShellWindowId_DockedContainer;

    case Container::USER_PRIVATE_PRESENTATION_WINDOWS:
      return kUnknownAshId;

    case Container::USER_PRIVATE_SHELF:
      return ash::kShellWindowId_ShelfContainer;

    case Container::USER_PRIVATE_PANELS:
      return ash::kShellWindowId_PanelContainer;

    case Container::USER_PRIVATE_APP_LIST:
      return ash::kShellWindowId_AppListContainer;

    case Container::USER_PRIVATE_SYSTEM_MODAL:
      return ash::kShellWindowId_SystemModalContainer;

    case Container::LOGIN:
      return kUnknownAshId;

    case Container::LOGIN_WINDOWS:
      return kUnknownAshId;

    case Container::LOGIN_APP:
      return kUnknownAshId;

    case Container::LOGIN_SHELF:
      return kUnknownAshId;

    case Container::STATUS:
      return kUnknownAshId;

    case Container::BUBBLES:
      return kUnknownAshId;

    case Container::SYSTEM_MODAL_WINDOWS:
      return kUnknownAshId;

    case Container::KEYBOARD:
      return kUnknownAshId;

    case Container::MENUS:
      return ash::kShellWindowId_MenuContainer;

    case Container::DRAG_AND_TOOLTIPS:
      return ash::kShellWindowId_DragImageAndTooltipContainer;

    case Container::COUNT:
      return kUnknownAshId;
  }
  return kUnknownAshId;
}

}  // namespace wm
}  // namespace mash
