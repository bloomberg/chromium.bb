// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/container_ids.h"

#include "ash/wm/common/wm_shell_window_ids.h"
#include "mash/wm/public/interfaces/container.mojom.h"

namespace mash {
namespace wm {

const mojom::Container kActivationContainers[] = {
    // TODO(sky): figure out right set of containers. I suspect this should be
    // all non containers.
    mojom::Container::USER_PRIVATE_WINDOWS,
    mojom::Container::USER_PRIVATE_ALWAYS_ON_TOP_WINDOWS,
    mojom::Container::USER_PRIVATE_DOCKED_WINDOWS,
    mojom::Container::USER_PRIVATE_PRESENTATION_WINDOWS,
    mojom::Container::USER_PRIVATE_PANELS,
    mojom::Container::USER_PRIVATE_APP_LIST,
    mojom::Container::USER_PRIVATE_SYSTEM_MODAL,
    // Bubble windows must be allowed to activate because some of them rely on
    // deactivation to close.
    mojom::Container::BUBBLES,
};

const size_t kNumActivationContainers = arraysize(kActivationContainers);

mojom::Container AshContainerToMashContainer(int ash_id) {
  switch (ash_id) {
    case ash::kShellWindowId_UnparentedControlContainer:
    case ash::kShellWindowId_LockScreenContainer:
    case ash::kShellWindowId_LockSystemModalContainer:
      // We should never be asked to parent windows of these types.
      NOTREACHED();
      return mojom::Container::USER_PRIVATE;

    case ash::kShellWindowId_DefaultContainer:
      return mojom::Container::USER_PRIVATE_WINDOWS;

    case ash::kShellWindowId_AlwaysOnTopContainer:
      return mojom::Container::USER_PRIVATE_ALWAYS_ON_TOP_WINDOWS;

    case ash::kShellWindowId_DockedContainer:
      return mojom::Container::USER_PRIVATE_DOCKED_WINDOWS;

    case ash::kShellWindowId_ShelfContainer:
      return mojom::Container::USER_PRIVATE_SHELF;

    case ash::kShellWindowId_PanelContainer:
      return mojom::Container::USER_PRIVATE_PANELS;

    case ash::kShellWindowId_AppListContainer:
      return mojom::Container::USER_PRIVATE_APP_LIST;

    case ash::kShellWindowId_SystemModalContainer:
      return mojom::Container::USER_PRIVATE_SYSTEM_MODAL;

    case ash::kShellWindowId_MenuContainer:
      return mojom::Container::MENUS;

    case ash::kShellWindowId_DragImageAndTooltipContainer:
      return mojom::Container::DRAG_AND_TOOLTIPS;

    default:
      NOTREACHED();
  }
  return mojom::Container::USER_PRIVATE_WINDOWS;
}

int MashContainerToAshContainer(mojom::Container container) {
  switch (container) {
    case mojom::Container::ROOT:
      return kUnknownAshId;

    case mojom::Container::ALL_USER_BACKGROUND:
      return kUnknownAshId;

    case mojom::Container::USER:
      return kUnknownAshId;

    case mojom::Container::USER_BACKGROUND:
      return kUnknownAshId;

    case mojom::Container::USER_PRIVATE:
      return kUnknownAshId;

    case mojom::Container::USER_PRIVATE_WINDOWS:
      return ash::kShellWindowId_DefaultContainer;

    case mojom::Container::USER_PRIVATE_ALWAYS_ON_TOP_WINDOWS:
      return ash::kShellWindowId_AlwaysOnTopContainer;

    case mojom::Container::USER_PRIVATE_DOCKED_WINDOWS:
      return ash::kShellWindowId_DockedContainer;

    case mojom::Container::USER_PRIVATE_PRESENTATION_WINDOWS:
      return kUnknownAshId;

    case mojom::Container::USER_PRIVATE_SHELF:
      return ash::kShellWindowId_ShelfContainer;

    case mojom::Container::USER_PRIVATE_PANELS:
      return ash::kShellWindowId_PanelContainer;

    case mojom::Container::USER_PRIVATE_APP_LIST:
      return ash::kShellWindowId_AppListContainer;

    case mojom::Container::USER_PRIVATE_SYSTEM_MODAL:
      return ash::kShellWindowId_SystemModalContainer;

    case mojom::Container::LOGIN:
      return kUnknownAshId;

    case mojom::Container::LOGIN_WINDOWS:
      return kUnknownAshId;

    case mojom::Container::LOGIN_APP:
      return kUnknownAshId;

    case mojom::Container::LOGIN_SHELF:
      return kUnknownAshId;

    case mojom::Container::STATUS:
      return kUnknownAshId;

    case mojom::Container::BUBBLES:
      return kUnknownAshId;

    case mojom::Container::SYSTEM_MODAL_WINDOWS:
      return kUnknownAshId;

    case mojom::Container::KEYBOARD:
      return kUnknownAshId;

    case mojom::Container::MENUS:
      return ash::kShellWindowId_MenuContainer;

    case mojom::Container::DRAG_AND_TOOLTIPS:
      return ash::kShellWindowId_DragImageAndTooltipContainer;

    case mojom::Container::COUNT:
      return kUnknownAshId;
  }
  return kUnknownAshId;
}

}  // namespace wm
}  // namespace mash
