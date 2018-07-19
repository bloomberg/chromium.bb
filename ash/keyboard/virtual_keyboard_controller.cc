// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/virtual_keyboard_controller.h"

#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/keyboard/keyboard_ui.h"
#include "ash/public/cpp/config.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "ui/base/emoji/emoji_panel_helper.h"
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

// Checks if virtual keyboard is force-enabled by enable-virtual-keyboard flag.
bool IsVirtualKeyboardEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      keyboard::switches::kEnableVirtualKeyboard);
}

void DisableVirtualKeyboard() {
  // Reset the accessibility keyboard settings.
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  if (!accessibility_controller)
    return;

  DCHECK(accessibility_controller->IsVirtualKeyboardEnabled());
  accessibility_controller->SetVirtualKeyboardEnabled(false);
}

void MoveKeyboardToDisplayInternal(const display::Display& display) {
  // Remove the keyboard from curent root window controller
  TRACE_EVENT0("vk", "MoveKeyboardToDisplayInternal");
  Shell::Get()->keyboard_ui()->Hide();
  RootWindowController::ForWindow(
      keyboard::KeyboardController::Get()->GetRootWindow())
      ->DeactivateKeyboard(keyboard::KeyboardController::Get());

  for (RootWindowController* controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    if (display::Screen::GetScreen()
            ->GetDisplayNearestWindow(controller->GetRootWindow())
            .id() == display.id()) {
      controller->ActivateKeyboard(keyboard::KeyboardController::Get());
      break;
    }
  }
}

bool HasTouchableDisplay() {
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (display.touch_support() == display::Display::TouchSupport::AVAILABLE)
      return true;
  }
  return false;
}

void MoveKeyboardToFirstTouchableDisplay() {
  // Move the keyboard to the first display with touch capability.
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (display.touch_support() == display::Display::TouchSupport::AVAILABLE) {
      MoveKeyboardToDisplayInternal(display);
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
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
  UpdateDevices();

  // Set callback to show the emoji panel
  ui::SetShowEmojiKeyboardCallback(base::BindRepeating(
      &VirtualKeyboardController::ForceShowKeyboardWithKeyset,
      base::Unretained(this),
      chromeos::input_method::mojom::ImeKeyset::kEmoji));

  keyboard::KeyboardController::Get()->AddObserver(this);
}

VirtualKeyboardController::~VirtualKeyboardController() {
  keyboard::KeyboardController::Get()->RemoveObserver(this);

  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  if (Shell::Get()->session_controller())
    Shell::Get()->session_controller()->RemoveObserver(this);
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);

  // Reset the emoji panel callback
  ui::SetShowEmojiKeyboardCallback(base::DoNothing());
}

void VirtualKeyboardController::ForceShowKeyboardWithKeyset(
    chromeos::input_method::mojom::ImeKeyset keyset) {
  Shell::Get()->ime_controller()->OverrideKeyboardKeyset(
      keyset, base::BindOnce(&VirtualKeyboardController::ForceShowKeyboard,
                             base::Unretained(this)));
}

void VirtualKeyboardController::OnTabletModeStarted() {
  if (IsVirtualKeyboardEnabled()) {
    SetKeyboardEnabled(true);
  } else {
    UpdateKeyboardEnabled();
  }
}

void VirtualKeyboardController::OnTabletModeEnded() {
  if (IsVirtualKeyboardEnabled()) {
    SetKeyboardEnabled(false);
  } else {
    UpdateKeyboardEnabled();
  }
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

void VirtualKeyboardController::MoveKeyboardToDisplay(
    const display::Display& display) {
  DCHECK(keyboard::KeyboardController::Get()->enabled());
  DCHECK(display.is_valid());

  TRACE_EVENT0("vk", "MoveKeyboardToDisplay");

  aura::Window* keyboard_window =
      keyboard::KeyboardController::Get()->GetKeyboardWindow();
  DCHECK(keyboard_window);

  const display::Screen* screen = display::Screen::GetScreen();
  const display::Display current_display =
      screen->GetDisplayNearestWindow(keyboard_window);

  if (display.id() != current_display.id())
    MoveKeyboardToDisplayInternal(display);
}

void VirtualKeyboardController::MoveKeyboardToTouchableDisplay() {
  DCHECK(keyboard::KeyboardController::Get()->enabled());

  TRACE_EVENT0("vk", "MoveKeyboardToTouchableDisplay");

  aura::Window* keyboard_window =
      keyboard::KeyboardController::Get()->GetKeyboardWindow();
  DCHECK(keyboard_window);

  const display::Screen* screen = display::Screen::GetScreen();
  const display::Display current_display =
      screen->GetDisplayNearestWindow(keyboard_window);

  if (wm::GetFocusedWindow()) {
    // Move the virtual keyboard to the focused display if that display has
    // touch capability or no other display has touch capability.
    const display::Display focused_display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            wm::GetFocusedWindow());
    if (current_display.id() != focused_display.id() &&
        focused_display.is_valid() &&
        (focused_display.touch_support() ==
             display::Display::TouchSupport::AVAILABLE ||
         !HasTouchableDisplay())) {
      MoveKeyboardToDisplayInternal(focused_display);
      return;
    }
  }

  if (current_display.touch_support() !=
      display::Display::TouchSupport::AVAILABLE) {
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
  if (IsVirtualKeyboardEnabled()) {
    SetKeyboardEnabled(Shell::Get()
                           ->tablet_mode_controller()
                           ->IsTabletModeWindowManagerEnabled());
    return;
  }
  bool ignore_internal_keyboard = Shell::Get()
                                      ->tablet_mode_controller()
                                      ->IsTabletModeWindowManagerEnabled();
  bool is_internal_keyboard_active =
      has_internal_keyboard_ && !ignore_internal_keyboard;
  SetKeyboardEnabled(!is_internal_keyboard_active && has_touchscreen_ &&
                     (!has_external_keyboard_ || ignore_external_keyboard_));
  Shell::Get()->system_tray_notifier()->NotifyVirtualKeyboardSuppressionChanged(
      !is_internal_keyboard_active && has_touchscreen_ &&
      has_external_keyboard_);
}

void VirtualKeyboardController::SetKeyboardEnabled(bool enabled) {
  bool was_enabled = keyboard::IsKeyboardEnabled();
  keyboard::SetTouchKeyboardEnabled(enabled);
  bool is_enabled = keyboard::IsKeyboardEnabled();
  if (is_enabled == was_enabled)
    return;
  if (is_enabled) {
    Shell::Get()->EnableKeyboard();
  } else {
    Shell::Get()->DisableKeyboard();
  }
}

void VirtualKeyboardController::ForceShowKeyboard() {
  // If the virtual keyboard is enabled, show the keyboard directly.
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  if (keyboard_controller->enabled()) {
    keyboard_controller->ShowKeyboard(false /* locked */);
    return;
  }

  // Otherwise, force enable the virtual keyboard by turning on the
  // accessibility keyboard.
  // TODO(https://crbug.com/818567): This is risky as enabling accessibility
  // keyboard is a persistent setting, so we have to ensure that we disable it
  // again when the keyboard is closed.
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  DCHECK(!accessibility_controller->IsVirtualKeyboardEnabled());

  // TODO(mash): Turning on accessibility keyboard does not create a valid
  // KeyboardController under MASH. See https://crbug.com/646565.
  if (Shell::GetAshConfig() == Config::MASH_DEPRECATED)
    return;

  // Onscreen keyboard has not been enabled yet, forces to bring out the
  // keyboard for one time.
  accessibility_controller->SetVirtualKeyboardEnabled(true);
  keyboard_enabled_using_accessibility_prefs_ = true;
  keyboard_controller = keyboard::KeyboardController::Get();
  DCHECK(keyboard_controller->enabled());

  keyboard_controller->ShowKeyboard(false);
}

void VirtualKeyboardController::OnKeyboardDisabled() {
  Shell::Get()->ime_controller()->OverrideKeyboardKeyset(
      chromeos::input_method::mojom::ImeKeyset::kNone);
}

void VirtualKeyboardController::OnKeyboardHidden(bool is_temporary_hide) {
  // The keyboard may temporarily hide (e.g. to change container behaviors).
  // The keyset should not be reset in this case.
  if (is_temporary_hide)
    return;

  Shell::Get()->ime_controller()->OverrideKeyboardKeyset(
      chromeos::input_method::mojom::ImeKeyset::kNone);

  if (keyboard_enabled_using_accessibility_prefs_) {
    keyboard_enabled_using_accessibility_prefs_ = false;

    // Posts a task to disable the virtual keyboard.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(DisableVirtualKeyboard));
  }
}

void VirtualKeyboardController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  // Force on-screen keyboard to reset.
  if (keyboard::IsKeyboardEnabled())
    Shell::Get()->EnableKeyboard();
}

}  // namespace ash
