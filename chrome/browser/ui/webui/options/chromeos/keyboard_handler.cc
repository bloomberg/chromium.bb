// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/keyboard_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "ash/common/new_window_controller.h"
#include "ash/common/wm_shell.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/input_device_manager.h"

namespace {
const struct ModifierKeysSelectItem {
  int message_id;
  chromeos::input_method::ModifierKey value;
} kModifierKeysSelectItems[] = {
  { IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_SEARCH,
    chromeos::input_method::kSearchKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_LEFT_CTRL,
    chromeos::input_method::kControlKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_LEFT_ALT,
    chromeos::input_method::kAltKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_VOID,
    chromeos::input_method::kVoidKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_CAPS_LOCK,
    chromeos::input_method::kCapsLockKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_ESCAPE,
    chromeos::input_method::kEscapeKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_BACKSPACE,
    chromeos::input_method::kBackspaceKey },
};

const char* kDataValuesNames[] = {
  "remapSearchKeyToValue",
  "remapControlKeyToValue",
  "remapAltKeyToValue",
  "remapCapsLockKeyToValue",
  "remapDiamondKeyToValue",
  "remapEscapeKeyToValue",
  "remapBackspaceKeyToValue",
};

bool HasExternalKeyboard() {
  for (const ui::InputDevice& keyboard :
       ui::InputDeviceManager::GetInstance()->GetKeyboardDevices()) {
    if (keyboard.type == ui::InputDeviceType::INPUT_DEVICE_EXTERNAL)
      return true;
  }

  return false;
}
}  // namespace

namespace chromeos {
namespace options {

KeyboardHandler::KeyboardHandler() {
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

KeyboardHandler::~KeyboardHandler() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
}

void KeyboardHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  RegisterTitle(localized_strings, "keyboardOverlay",
                IDS_OPTIONS_KEYBOARD_OVERLAY_TITLE);

  localized_strings->SetString("remapSearchKeyToContent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_SEARCH_LABEL));
  localized_strings->SetString("remapControlKeyToContent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_LEFT_CTRL_LABEL));
  localized_strings->SetString("remapAltKeyToContent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_LEFT_ALT_LABEL));
  localized_strings->SetString("remapCapsLockKeyToContent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_CAPS_LOCK_LABEL));
  localized_strings->SetString("remapDiamondKeyToContent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_DIAMOND_KEY_LABEL));
  localized_strings->SetString("remapBackspaceKeyToContent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_BACKSPACE_KEY_LABEL));
  localized_strings->SetString("remapEscapeKeyToContent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_KEY_ESCAPE_KEY_LABEL));
  localized_strings->SetString("sendFunctionKeys",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SEND_FUNCTION_KEYS));
  localized_strings->SetString("sendFunctionKeysDescription",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SEND_FUNCTION_KEYS_DESCRIPTION));
  localized_strings->SetString("enableAutoRepeat",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_AUTO_REPEAT_ENABLE));
  localized_strings->SetString("autoRepeatDelay",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_AUTO_REPEAT_DELAY));
  localized_strings->SetString("autoRepeatDelayLong",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_AUTO_REPEAT_DELAY_LONG));
  localized_strings->SetString("autoRepeatDelayShort",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_AUTO_REPEAT_DELAY_SHORT));
  localized_strings->SetString("autoRepeatRate",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_AUTO_REPEAT_RATE));
  localized_strings->SetString("autoRepeatRateSlow",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_AUTO_REPEAT_RATE_SLOW));
  localized_strings->SetString("autoRepeatRateFast",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_AUTO_REPEAT_RATE_FAST));
  localized_strings->SetString("changeLanguageAndInputSettings",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_CHANGE_LANGUAGE_AND_INPUT_SETTINGS));
  localized_strings->SetString("showKeyboardShortcuts",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SHOW_KEYBOARD_SHORTCUTS));

  for (size_t i = 0; i < arraysize(kDataValuesNames); ++i) {
    base::ListValue* list_value = new base::ListValue();
    for (size_t j = 0; j < arraysize(kModifierKeysSelectItems); ++j) {
      const input_method::ModifierKey value =
          kModifierKeysSelectItems[j].value;
      const int message_id = kModifierKeysSelectItems[j].message_id;
      auto option = base::MakeUnique<base::ListValue>();
      option->AppendInteger(value);
      option->AppendString(l10n_util::GetStringUTF16(message_id));
      list_value->Append(std::move(option));
    }
    localized_strings->Set(kDataValuesNames[i], list_value);
  }
}

void KeyboardHandler::InitializePage() {
  bool has_diamond_key = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kHasChromeOSDiamondKey);
  const base::Value show_diamond_key_options(has_diamond_key);

  web_ui()->CallJavascriptFunctionUnsafe(
      "options.KeyboardOverlay.showDiamondKeyOptions",
      show_diamond_key_options);

  UpdateCapsLockOptions();
}

void KeyboardHandler::RegisterMessages() {
  // Callback to show keyboard overlay.
  web_ui()->RegisterMessageCallback(
      "showKeyboardShortcuts",
      base::Bind(&KeyboardHandler::HandleShowKeyboardShortcuts,
                 base::Unretained(this)));
}

void KeyboardHandler::OnKeyboardDeviceConfigurationChanged() {
  UpdateCapsLockOptions();
}

void KeyboardHandler::HandleShowKeyboardShortcuts(const base::ListValue* args) {
  ash::WmShell::Get()->new_window_controller()->ShowKeyboardOverlay();
}

void KeyboardHandler::UpdateCapsLockOptions() const {
  const base::Value show_caps_lock_options(HasExternalKeyboard());
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.KeyboardOverlay.showCapsLockOptions", show_caps_lock_options);
}

}  // namespace options
}  // namespace chromeos
