// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/accelerators/accelerator_controller_delegate_mus.h"

#include "base/logging.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "services/ui/public/interfaces/display/display_controller.mojom.h"

namespace ash {
namespace mus {

AcceleratorControllerDelegateMus::AcceleratorControllerDelegateMus(
    shell::Connector* connector)
    : connector_(connector) {
#if !defined(OS_CHROMEOS)
  // To avoid trybot complaining that |connector_| is not being
  // used in non-ChromeOS.
  connector_ = nullptr;
#endif
}

AcceleratorControllerDelegateMus::~AcceleratorControllerDelegateMus() {}

bool AcceleratorControllerDelegateMus::HandlesAction(AcceleratorAction action) {
  // This is the list of actions that are not ported from aura. The actions are
  // replicated here to make sure we don't forget any. This list should
  // eventually be empty. If there are any actions that don't make sense for
  // mus, then they should be removed from AcceleratorAction.
  // http://crbug.com/612331.
  switch (action) {
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
    case DEV_TOGGLE_ROOT_WINDOW_FULL_SCREEN:
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
    case MAGNIFY_SCREEN_ZOOM_IN:
    case MAGNIFY_SCREEN_ZOOM_OUT:
    case ROTATE_SCREEN:
    case ROTATE_WINDOW:
    case SCALE_UI_DOWN:
    case SCALE_UI_RESET:
    case SCALE_UI_UP:
    case SHOW_SYSTEM_TRAY_BUBBLE:
    case TAKE_PARTIAL_SCREENSHOT:
    case TAKE_SCREENSHOT:
    case TAKE_WINDOW_SCREENSHOT:
    case UNPIN:
      NOTIMPLEMENTED();
      return false;

#if defined(OS_CHROMEOS)
    case DEV_ADD_REMOVE_DISPLAY: {
      display::mojom::DisplayControllerPtr display_controller;
      connector_->ConnectToInterface("service:ui", &display_controller);
      display_controller->ToggleVirtualDisplay();
      return true;
    }
    case DEV_TOGGLE_UNIFIED_DESKTOP:
    case LOCK_PRESSED:
    case LOCK_RELEASED:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case SWAP_PRIMARY_DISPLAY:
    case TOGGLE_MIRROR_MODE:
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
      NOTIMPLEMENTED();
      return false;
    case TOUCH_HUD_PROJECTION_TOGGLE: {
      mash::mojom::LaunchablePtr launchable;
      connector_->ConnectToInterface("service:touch_hud", &launchable);
      launchable->Launch(mash::mojom::kWindow,
                         mash::mojom::LaunchMode::DEFAULT);
      return true;
    }
#endif

    default:
      break;
  }
  return false;
}

bool AcceleratorControllerDelegateMus::CanPerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator,
    const ui::Accelerator& previous_accelerator) {
  return false;
}

void AcceleratorControllerDelegateMus::PerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  // Should never be hit as HandlesAction() unconditionally returns false.
  NOTREACHED();
}

void AcceleratorControllerDelegateMus::ShowDeprecatedAcceleratorNotification(
    const char* const notification_id,
    int message_id,
    int old_shortcut_id,
    int new_shortcut_id) {
  // TODO: http://crbug.com/630316.
  NOTIMPLEMENTED();
}

}  // namespace mus
}  // namespace ash
