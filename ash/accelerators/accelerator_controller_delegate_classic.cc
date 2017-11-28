// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_delegate_classic.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_commands_classic.h"
#include "ash/accelerators/debug_commands.h"
#include "ash/debug.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/system_notifier.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/sys_info.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {
namespace {

bool CanHandleMagnifyScreen() {
  return Shell::Get()->magnification_controller()->IsEnabled();
}

// Magnify the screen
void HandleMagnifyScreen(int delta_index) {
  if (Shell::Get()->magnification_controller()->IsEnabled()) {
    // TODO(yoshiki): Move the following logic to MagnificationController.
    float scale = Shell::Get()->magnification_controller()->GetScale();
    // Calculate rounded logarithm (base kMagnificationScaleFactor) of scale.
    int scale_index = std::round(
        std::log(scale) /
        std::log(MagnificationController::kMagnificationScaleFactor));

    int new_scale_index = std::max(0, std::min(8, scale_index + delta_index));

    Shell::Get()->magnification_controller()->SetScale(
        std::pow(MagnificationController::kMagnificationScaleFactor,
                 new_scale_index),
        true);
  }
}

bool CanHandleUnpin() {
  // Returns true only for WindowStateType::PINNED.
  // WindowStateType::TRUSTED_PINNED does not accept user's unpin operation.
  wm::WindowState* window_state = wm::GetActiveWindowState();
  return window_state &&
         window_state->GetStateType() == mojom::WindowStateType::PINNED;
}

bool CanHandleTouchHud() {
  return RootWindowController::ForTargetRootWindow()->touch_hud_debug();
}

void HandleTouchHudClear() {
  RootWindowController::ForTargetRootWindow()->touch_hud_debug()->Clear();
}

void HandleTouchHudModeChange() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  controller->touch_hud_debug()->ChangeToNextMode();
}

}  // namespace

AcceleratorControllerDelegateClassic::AcceleratorControllerDelegateClassic() =
    default;

AcceleratorControllerDelegateClassic::~AcceleratorControllerDelegateClassic() =
    default;

bool AcceleratorControllerDelegateClassic::HandlesAction(
    AcceleratorAction action) {
  // NOTE: When adding a new accelerator that only depends on //ash/common code,
  // add it to accelerator_controller.cc instead. See class comment.
  switch (action) {
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
    case LOCK_PRESSED:
    case LOCK_RELEASED:
    case MAGNIFY_SCREEN_ZOOM_IN:
    case MAGNIFY_SCREEN_ZOOM_OUT:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case TOGGLE_MESSAGE_CENTER_BUBBLE:
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
    case TOUCH_HUD_PROJECTION_TOGGLE:
    case UNPIN:
      return true;

    default:
      break;
  }
  return false;
}

bool AcceleratorControllerDelegateClassic::CanPerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator,
    const ui::Accelerator& previous_accelerator) {
  switch (action) {
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
      return debug::DebugAcceleratorsEnabled();
    case MAGNIFY_SCREEN_ZOOM_IN:
    case MAGNIFY_SCREEN_ZOOM_OUT:
      return CanHandleMagnifyScreen();
    case UNPIN:
      return CanHandleUnpin();

    // Following are always enabled:
    case LOCK_PRESSED:
    case LOCK_RELEASED:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case TOUCH_HUD_PROJECTION_TOGGLE:
      return true;

    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
      return CanHandleTouchHud();

    default:
      NOTREACHED();
      break;
  }
  return false;
}

void AcceleratorControllerDelegateClassic::PerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  switch (action) {
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
      debug::ToggleShowDebugBorders();
      break;
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
      debug::ToggleShowFpsCounter();
      break;
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
      debug::ToggleShowPaintRects();
      break;
    case LOCK_PRESSED:
    case LOCK_RELEASED:
      Shell::Get()->power_button_controller()->OnLockButtonEvent(
          action == LOCK_PRESSED, base::TimeTicks());
      break;
    case MAGNIFY_SCREEN_ZOOM_IN:
      HandleMagnifyScreen(1);
      break;
    case MAGNIFY_SCREEN_ZOOM_OUT:
      HandleMagnifyScreen(-1);
      break;
    case POWER_PRESSED:  // fallthrough
    case POWER_RELEASED:
      if (!base::SysInfo::IsRunningOnChromeOS()) {
        // There is no powerd, the Chrome OS power manager, in linux desktop,
        // so call the PowerButtonController here.
        Shell::Get()->power_button_controller()->OnPowerButtonEvent(
            action == POWER_PRESSED, base::TimeTicks());
      }
      // We don't do anything with these at present on the device,
      // (power button events are reported to us from powerm via
      // D-BUS), but we consume them to prevent them from getting
      // passed to apps -- see http://crbug.com/146609.
      break;
    case TOUCH_HUD_CLEAR:
      HandleTouchHudClear();
      break;
    case TOUCH_HUD_MODE_CHANGE:
      HandleTouchHudModeChange();
      break;
    case TOUCH_HUD_PROJECTION_TOGGLE:
      accelerators::ToggleTouchHudProjection();
      break;
    case UNPIN:
      accelerators::Unpin();
      break;
    default:
      break;
  }
}

}  // namespace ash
