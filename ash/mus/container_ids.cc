// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/container_ids.h"

#include "ash/common/shell_window_ids.h"
#include "ash/public/interfaces/container.mojom.h"

using ash::mojom::Container;

namespace ash {
namespace mus {

int MashContainerToAshShellWindowId(Container container) {
  switch (container) {
    case Container::USER_BACKGROUND:
      return kShellWindowId_DesktopBackgroundContainer;

    case Container::USER_PRIVATE_SHELF:
      return kShellWindowId_ShelfContainer;

    case Container::LOGIN_WINDOWS:
      return kShellWindowId_LockScreenContainer;

    case Container::STATUS:
      return kShellWindowId_StatusContainer;

    case Container::BUBBLES:
      // TODO(sky): this mapping isn't right, but BUBBLES should go away once
      // http://crbug.com/616859 lands.
      return kShellWindowId_SettingBubbleContainer;

    case Container::MENUS:
      return kShellWindowId_MenuContainer;

    case Container::DRAG_AND_TOOLTIPS:
      return kShellWindowId_DragImageAndTooltipContainer;

    case Container::OVERLAY:
      return kShellWindowId_OverlayContainer;
  }
  return kShellWindowId_Invalid;
}

}  // namespace mus
}  // namespace ash
