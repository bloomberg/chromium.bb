// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/virtual_keyboard_controller.h"

#include <vector>

#include "ash/common/keyboard/keyboard_ui.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace ash {
namespace {

// Checks whether smart deployment is enabled.
bool IsSmartVirtualKeyboardEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kEnableVirtualKeyboard)) {
    return false;
  }
  return keyboard::IsSmartDeployEnabled();
}

void MoveKeyboardToDisplayInternal(const int64_t display_id) {
  // Remove the keyboard from curent root window controller
  WmShell::Get()->keyboard_ui()->Hide();
  RootWindowController::ForWindow(
      keyboard::KeyboardController::GetInstance()->GetContainerWindow())
      ->DeactivateKeyboard(keyboard::KeyboardController::GetInstance());

  for (RootWindowController* controller :
       Shell::GetInstance()->GetAllRootWindowControllers()) {
    if (display::Screen::GetScreen()
            ->GetDisplayNearestWindow(controller->GetRootWindow())
            .id() == display_id) {
      controller->ActivateKeyboard(keyboard::KeyboardController::GetInstance());
      break;
    }
  }
}

void MoveKeyboardToFirstTouchableDisplay() {
  // Move the keyboard to the first display with touch capability.
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (display.touch_support() ==
        display::Display::TouchSupport::TOUCH_SUPPORT_AVAILABLE) {
      MoveKeyboardToDisplayInternal(display.id());
      return;
    }
  }
}

}  // namespace

VirtualKeyboardController::VirtualKeyboardController()
    : has_external_keyboard_(false),
      has_internal_keyboard_(false),
      has_touchscreen_(false),
      ignore_external_keyboard_(false) {
  WmShell::Get()->AddShellObserver(this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
  UpdateDevices();
}

VirtualKeyboardController::~VirtualKeyboardController() {
  WmShell::Get()->RemoveShellObserver(this);
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
}

void VirtualKeyboardController::OnMaximizeModeStarted() {
  if (!IsSmartVirtualKeyboardEnabled()) {
    SetKeyboardEnabled(true);
  } else {
    UpdateKeyboardEnabled();
  }
  keyboard::SetOverscrollEnabledWithAccessibilityKeyboard(true);
}

void VirtualKeyboardController::OnMaximizeModeEnded() {
  if (!IsSmartVirtualKeyboardEnabled()) {
    SetKeyboardEnabled(false);
  } else {
    UpdateKeyboardEnabled();
  }
  keyboard::SetOverscrollEnabledWithAccessibilityKeyboard(false);
}

void VirtualKeyboardController::OnTouchscreenDeviceConfigurationChanged() {
  UpdateDevices();
}

void VirtualKeyboardController::OnKeyboardDeviceConfigurationChanged() {
  UpdateDevices();
}

void VirtualKeyboardController::ToggleIgnoreExternalKeyboard() {
  ignore_external_keyboard_ = !ignore_external_keyboard_;
  UpdateKeyboardEnabled();
}

void VirtualKeyboardController::MoveKeyboardToDisplay(int64_t display_id) {
  DCHECK(keyboard::KeyboardController::GetInstance() != nullptr);
  DCHECK(display_id != display::kInvalidDisplayId);

  aura::Window* container =
      keyboard::KeyboardController::GetInstance()->GetContainerWindow();
  DCHECK(container != nullptr);
  const display::Screen* screen = display::Screen::GetScreen();
  const display::Display current_display =
      screen->GetDisplayNearestWindow(container);

  if (display_id != current_display.id())
    MoveKeyboardToDisplayInternal(display_id);
}

void VirtualKeyboardController::MoveKeyboardToTouchableDisplay() {
  DCHECK(keyboard::KeyboardController::GetInstance() != nullptr);

  aura::Window* container =
      keyboard::KeyboardController::GetInstance()->GetContainerWindow();
  DCHECK(container != nullptr);

  const display::Screen* screen = display::Screen::GetScreen();
  const display::Display current_display =
      screen->GetDisplayNearestWindow(container);

  if (WmShell::Get()->GetFocusedWindow() != nullptr) {
    // Move the virtual keyboard to the focused display if that display has
    // touch capability or keyboard is locked
    const display::Display focused_display =
        WmShell::Get()->GetFocusedWindow()->GetDisplayNearestWindow();
    if (current_display.id() != focused_display.id() &&
        focused_display.id() != display::kInvalidDisplayId &&
        focused_display.touch_support() ==
            display::Display::TouchSupport::TOUCH_SUPPORT_AVAILABLE) {
      MoveKeyboardToDisplayInternal(focused_display.id());
      return;
    }
  }

  if (current_display.touch_support() !=
      display::Display::TouchSupport::TOUCH_SUPPORT_AVAILABLE) {
    // The keyboard is currently on the display without touch capability.
    MoveKeyboardToFirstTouchableDisplay();
  }
}

void VirtualKeyboardController::UpdateDevices() {
  ui::InputDeviceManager* device_data_manager =
      ui::InputDeviceManager::GetInstance();

  // Checks for touchscreens.
  has_touchscreen_ = device_data_manager->GetTouchscreenDevices().size() > 0;

  // Checks for keyboards.
  has_external_keyboard_ = false;
  has_internal_keyboard_ = false;
  for (const ui::InputDevice& device :
       device_data_manager->GetKeyboardDevices()) {
    if (has_internal_keyboard_ && has_external_keyboard_)
      break;
    ui::InputDeviceType type = device.type;
    if (type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL)
      has_internal_keyboard_ = true;
    if (type == ui::InputDeviceType::INPUT_DEVICE_EXTERNAL)
      has_external_keyboard_ = true;
  }
  // Update keyboard state.
  UpdateKeyboardEnabled();
}

void VirtualKeyboardController::UpdateKeyboardEnabled() {
  if (!IsSmartVirtualKeyboardEnabled()) {
    SetKeyboardEnabled(WmShell::Get()
                           ->maximize_mode_controller()
                           ->IsMaximizeModeWindowManagerEnabled());
    return;
  }
  bool ignore_internal_keyboard = WmShell::Get()
                                      ->maximize_mode_controller()
                                      ->IsMaximizeModeWindowManagerEnabled();
  bool is_internal_keyboard_active =
      has_internal_keyboard_ && !ignore_internal_keyboard;
  SetKeyboardEnabled(!is_internal_keyboard_active && has_touchscreen_ &&
                     (!has_external_keyboard_ || ignore_external_keyboard_));
  WmShell::Get()
      ->system_tray_notifier()
      ->NotifyVirtualKeyboardSuppressionChanged(!is_internal_keyboard_active &&
                                                has_touchscreen_ &&
                                                has_external_keyboard_);
}

void VirtualKeyboardController::SetKeyboardEnabled(bool enabled) {
  bool was_enabled = keyboard::IsKeyboardEnabled();
  keyboard::SetTouchKeyboardEnabled(enabled);
  bool is_enabled = keyboard::IsKeyboardEnabled();
  if (is_enabled == was_enabled)
    return;
  if (is_enabled) {
    Shell::GetInstance()->CreateKeyboard();
  } else {
    Shell::GetInstance()->DeactivateKeyboard();
  }
}

}  // namespace ash
