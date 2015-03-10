// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/virtual_keyboard_private/chrome_virtual_keyboard_delegate.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "ui/aura/window_tree_host.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace SetMode = extensions::core_api::virtual_keyboard_private::SetMode;

namespace {

aura::Window* GetKeyboardContainer() {
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  return controller ? controller->GetContainerWindow() : nullptr;
}

std::string GenerateFeatureFlag(std::string feature, bool enabled) {
  return feature + (enabled ? "-enabled" : "-disabled");
}

keyboard::KeyboardMode getKeyboardModeEnum(SetMode::Params::Mode mode) {
  switch (mode) {
    case SetMode::Params::MODE_NONE:
      return keyboard::NONE;
    case SetMode::Params::MODE_FULL_WIDTH:
      return keyboard::FULL_WIDTH;
    case SetMode::Params::MODE_FLOATING:
      return keyboard::FLOATING;
  }
  return keyboard::NONE;
}

}  // namespace

namespace extensions {

bool ChromeVirtualKeyboardDelegate::GetKeyboardConfig(
    base::DictionaryValue* results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  results->SetString("layout", keyboard::GetKeyboardLayout());
  results->SetBoolean("a11ymode", keyboard::GetAccessibilityKeyboardEnabled());
  // TODO(rsadam): Deprecate this, and rely on features.
  results->SetBoolean("experimental",
                      keyboard::IsExperimentalInputViewEnabled());
  scoped_ptr<base::ListValue> features(new base::ListValue());
  features->AppendString(
      GenerateFeatureFlag("gesturetyping", keyboard::IsGestureTypingEnabled()));
  features->AppendString(GenerateFeatureFlag("experimental",
      keyboard::IsExperimentalInputViewEnabled()));
  // TODO(rsadam): Populate features with more inputview features.
  results->Set("features", features.Pass());
  return true;
}

bool ChromeVirtualKeyboardDelegate::HideKeyboard() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  if (!controller)
    return false;

  UMA_HISTOGRAM_ENUMERATION("VirtualKeyboard.KeyboardControlEvent",
                            keyboard::KEYBOARD_CONTROL_HIDE_USER,
                            keyboard::KEYBOARD_CONTROL_MAX);

  // Pass HIDE_REASON_MANUAL since calls to HideKeyboard as part of this API
  // would be user generated.
  controller->HideKeyboard(keyboard::KeyboardController::HIDE_REASON_MANUAL);
  return true;
}

bool ChromeVirtualKeyboardDelegate::InsertText(const base::string16& text) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return keyboard::InsertText(text);
}

bool ChromeVirtualKeyboardDelegate::OnKeyboardLoaded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  keyboard::MarkKeyboardLoadFinished();
  base::UserMetricsAction("VirtualKeyboardLoaded");
  return true;
}

bool ChromeVirtualKeyboardDelegate::LockKeyboard(bool state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  if (!controller)
    return false;

  keyboard::KeyboardController::GetInstance()->set_lock_keyboard(state);
  return true;
}

bool ChromeVirtualKeyboardDelegate::MoveCursor(int swipe_direction,
                                               int modifier_flags) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kEnableSwipeSelection)) {
    return false;
  }
  aura::Window* window = GetKeyboardContainer();
  return window && keyboard::MoveCursor(
                       swipe_direction, modifier_flags, window->GetHost());
}

bool ChromeVirtualKeyboardDelegate::SendKeyEvent(const std::string& type,
                                                 int char_value,
                                                 int key_code,
                                                 const std::string& key_name,
                                                 int modifiers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aura::Window* window = GetKeyboardContainer();
  return window && keyboard::SendKeyEvent(type,
                                          char_value,
                                          key_code,
                                          key_name,
                                          modifiers,
                                          window->GetHost());
}

bool ChromeVirtualKeyboardDelegate::ShowLanguageSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RecordAction(base::UserMetricsAction("OpenLanguageOptionsDialog"));
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        chrome::kLanguageOptionsSubPage);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetVirtualKeyboardMode(int mode_enum) {
  keyboard::KeyboardMode keyboard_mode =
      getKeyboardModeEnum(static_cast<SetMode::Params::Mode>(mode_enum));
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::GetInstance();
  if (!controller)
    return false;

  controller->SetKeyboardMode(keyboard_mode);
  return true;
}

bool ChromeVirtualKeyboardDelegate::IsLanguageSettingsEnabled() {
  return (user_manager::UserManager::Get()->IsUserLoggedIn() &&
          !chromeos::UserAddingScreen::Get()->IsRunning() &&
          !(chromeos::ScreenLocker::default_screen_locker() &&
            chromeos::ScreenLocker::default_screen_locker()->locked()));
}

}  // namespace extensions
