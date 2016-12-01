// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/accelerators/accelerator_controller_delegate_mus.h"

#include "ash/mus/window_manager.h"
#include "base/logging.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/display/display_controller.mojom.h"
#include "services/ui/public/interfaces/display/test_display_controller.mojom.h"
#include "ui/display/screen.h"

namespace ash {
namespace mus {

AcceleratorControllerDelegateMus::AcceleratorControllerDelegateMus(
    WindowManager* window_manager)
    : window_manager_(window_manager) {
#if !defined(OS_CHROMEOS)
  // To avoid trybot complaining that |window_manager_| is not being used in
  // non-ChromeOS.
  window_manager_ = nullptr;
#endif
}

AcceleratorControllerDelegateMus::~AcceleratorControllerDelegateMus() {}

bool AcceleratorControllerDelegateMus::HandlesAction(AcceleratorAction action) {
  // Accelerators that return true need to work differently in mash. These
  // should have implementations in CanPerformAction() and PerformAction().
  // Accelerators that return false have not been ported to work with mash yet.
  // If the behavior between cash and mash can be unified then the accelerator
  // should be moved to accelerator_controller.cc/h. See
  // http://crbug.com/612331.
  switch (action) {
    case SCALE_UI_DOWN:
    case SCALE_UI_RESET:
    case SCALE_UI_UP:
    case ROTATE_SCREEN:
      return true;
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
    case DEV_TOGGLE_ROOT_WINDOW_FULL_SCREEN:
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
    case MAGNIFY_SCREEN_ZOOM_IN:
    case MAGNIFY_SCREEN_ZOOM_OUT:
    case ROTATE_WINDOW:
    case SHOW_SYSTEM_TRAY_BUBBLE:
    case TAKE_PARTIAL_SCREENSHOT:
    case TAKE_SCREENSHOT:
    case TAKE_WINDOW_SCREENSHOT:
    case UNPIN:
      NOTIMPLEMENTED();
      return false;

#if defined(OS_CHROMEOS)
    case DEV_ADD_REMOVE_DISPLAY:
    case DEV_TOGGLE_UNIFIED_DESKTOP:
    case SWAP_PRIMARY_DISPLAY:
    case TOGGLE_MIRROR_MODE:
    case TOUCH_HUD_PROJECTION_TOGGLE:
      return true;
    case LOCK_PRESSED:
    case LOCK_RELEASED:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
      NOTIMPLEMENTED();
      return false;
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
  switch (action) {
    case ROTATE_SCREEN:
    case SCALE_UI_DOWN:
    case SCALE_UI_RESET:
    case SCALE_UI_UP:
      return true;
#if defined(OS_CHROMEOS)
    case DEV_ADD_REMOVE_DISPLAY:
    case DEV_TOGGLE_UNIFIED_DESKTOP:
      return true;
    case SWAP_PRIMARY_DISPLAY:
      return display::Screen::GetScreen()->GetNumDisplays() > 1;
    case TOGGLE_MIRROR_MODE:
    case TOUCH_HUD_PROJECTION_TOGGLE:
      return true;
#endif
    default:
      break;
  }
  return false;
}

void AcceleratorControllerDelegateMus::PerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  switch (action) {
    case ROTATE_SCREEN: {
      window_manager_->GetDisplayController()->RotateCurrentDisplayCW();
      break;
    }
    case SCALE_UI_DOWN: {
      window_manager_->GetDisplayController()->DecreaseInternalDisplayZoom();
      break;
    }
    case SCALE_UI_RESET: {
      window_manager_->GetDisplayController()->ResetInternalDisplayZoom();
      break;
    }
    case SCALE_UI_UP: {
      window_manager_->GetDisplayController()->IncreaseInternalDisplayZoom();
      break;
    }
#if defined(OS_CHROMEOS)
    case DEV_ADD_REMOVE_DISPLAY: {
      display::mojom::TestDisplayControllerPtr test_display_controller;
      window_manager_->connector()->ConnectToInterface(
          ui::mojom::kServiceName, &test_display_controller);
      test_display_controller->ToggleAddRemoveDisplay();
      break;
    }
    case DEV_TOGGLE_UNIFIED_DESKTOP: {
      // TODO(crbug.com/657816): This is hack. I'm just stealing the shortcut
      // key to toggle display size in mus. This should be removed by launch.
      display::mojom::TestDisplayControllerPtr test_display_controller;
      window_manager_->connector()->ConnectToInterface(
          ui::mojom::kServiceName, &test_display_controller);
      test_display_controller->ToggleDisplayResolution();
      break;
    }
    case SWAP_PRIMARY_DISPLAY: {
      window_manager_->GetDisplayController()->SwapPrimaryDisplay();
      break;
    }
    case TOGGLE_MIRROR_MODE: {
      window_manager_->GetDisplayController()->ToggleMirrorMode();
      break;
    }
    case TOUCH_HUD_PROJECTION_TOGGLE: {
      mash::mojom::LaunchablePtr launchable;
      window_manager_->connector()->ConnectToInterface("touch_hud",
                                                       &launchable);
      launchable->Launch(mash::mojom::kWindow,
                         mash::mojom::LaunchMode::DEFAULT);
      break;
    }
#endif
    default:
      NOTREACHED();
  }
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
