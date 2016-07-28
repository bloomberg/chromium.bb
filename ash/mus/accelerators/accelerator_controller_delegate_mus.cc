// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/accelerators/accelerator_controller_delegate_mus.h"

#include "base/logging.h"

namespace ash {
namespace mus {

AcceleratorControllerDelegateMus::AcceleratorControllerDelegateMus() {}

AcceleratorControllerDelegateMus::~AcceleratorControllerDelegateMus() {}

bool AcceleratorControllerDelegateMus::HandlesAction(AcceleratorAction action) {
  // This is the list of actions that are not ported from aura. The actions are
  // replicated here to make sure we don't forget any. This list should
  // eventually be empty. If there are any actions that don't make sense for
  // mus, then they should be removed from AcceleratorAction.
  // http://crbug.com/612331.
  switch (action) {
    case DEBUG_TOGGLE_DESKTOP_BACKGROUND_MODE:
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
    case DEBUG_TOGGLE_ROOT_WINDOW_FULL_SCREEN:
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
    case FOCUS_SHELF:
    case LAUNCH_APP_0:
    case LAUNCH_APP_1:
    case LAUNCH_APP_2:
    case LAUNCH_APP_3:
    case LAUNCH_APP_4:
    case LAUNCH_APP_5:
    case LAUNCH_APP_6:
    case LAUNCH_APP_7:
    case LAUNCH_LAST_APP:
    case MAGNIFY_SCREEN_ZOOM_IN:
    case MAGNIFY_SCREEN_ZOOM_OUT:
    case ROTATE_SCREEN:
    case ROTATE_WINDOW:
    case SCALE_UI_DOWN:
    case SCALE_UI_RESET:
    case SCALE_UI_UP:
    case SHOW_MESSAGE_CENTER_BUBBLE:
    case SHOW_SYSTEM_TRAY_BUBBLE:
    case TAKE_PARTIAL_SCREENSHOT:
    case TAKE_SCREENSHOT:
    case TAKE_WINDOW_SCREENSHOT:
    case UNPIN:
      NOTIMPLEMENTED();
      return false;

#if defined(OS_CHROMEOS)
    case DEBUG_ADD_REMOVE_DISPLAY:
    case DEBUG_TOGGLE_UNIFIED_DESKTOP:
    case DISABLE_GPU_WATCHDOG:
    case LOCK_PRESSED:
    case LOCK_RELEASED:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case SWAP_PRIMARY_DISPLAY:
    case TOGGLE_MIRROR_MODE:
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
    case TOUCH_HUD_PROJECTION_TOGGLE:
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
