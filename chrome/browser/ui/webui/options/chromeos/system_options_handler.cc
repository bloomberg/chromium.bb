// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/system_options_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/json/json_value_serializer.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/xinput_hierarchy_changed_event_listener.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/chromeos/system_settings_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;

namespace {

void TouchpadExistsFileThread(bool* exists) {
  *exists = chromeos::system::touchpad_settings::TouchpadExists();
}

void MouseExistsFileThread(bool* exists) {
  *exists = chromeos::system::mouse_settings::MouseExists();
}

}  // namespace

SystemOptionsHandler::SystemOptionsHandler() {
}

SystemOptionsHandler::~SystemOptionsHandler() {
  chromeos::XInputHierarchyChangedEventListener::GetInstance()
      ->RemoveObserver(this);
}

void SystemOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "systemPage", IDS_OPTIONS_SYSTEM_TAB_LABEL);
  localized_strings->SetString("datetimeTitle",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME));
  localized_strings->SetString("timezone",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_TIMEZONE_DESCRIPTION));
  localized_strings->SetString("use24HourClock",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_USE_24HOUR_CLOCK_DESCRIPTION));

  localized_strings->SetString("screen",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_SCREEN));
  localized_strings->SetString("brightness",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BRIGHTNESS_DESCRIPTION));
  localized_strings->SetString("brightnessDecrease",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BRIGHTNESS_DECREASE));
  localized_strings->SetString("brightnessIncrease",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BRIGHTNESS_INCREASE));

  localized_strings->SetString("pointer",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_POINTER));
  localized_strings->SetString("touchpad",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_TOUCHPAD));
  localized_strings->SetString("mouse",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_MOUSE));
  localized_strings->SetString("enableTapToClick",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_TAP_TO_CLICK_ENABLED_DESCRIPTION));
  localized_strings->SetString("sensitivity",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SENSITIVITY_DESCRIPTION));
  localized_strings->SetString("sensitivityLess",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SENSITIVITY_LESS_DESCRIPTION));
  localized_strings->SetString("sensitivityMore",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SENSITIVITY_MORE_DESCRIPTION));
  localized_strings->SetString("primaryMouseRight",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_PRIMARY_MOUSE_RIGHT_DESCRIPTION));

  localized_strings->SetString("language",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_LANGUAGE));
  localized_strings->SetString("languageCustomize",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_CUSTOMIZE));
  localized_strings->SetString("modifierKeysCustomize",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_MODIFIER_KEYS_CUSTOMIZE));

  localized_strings->SetString("accessibilityTitle",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY));
  localized_strings->SetString("accessibilitySpokenFeedback",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_ACCESSIBILITY_DESCRIPTION));
  localized_strings->SetString("accessibilityHighContrast",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_ACCESSIBILITY_HIGH_CONTRAST_DESCRIPTION));
  localized_strings->SetString("accessibilityScreenMagnifier",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_ACCESSIBILITY_SCREEN_MAGNIFIER_DESCRIPTION));
  localized_strings->SetString("accessibilityVirtualKeyboard",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_ACCESSIBILITY_VIRTUAL_KEYBOARD_DESCRIPTION));

  // TODO(pastarmovj): replace this with a call to the CrosSettings list
  // handling functionality to come.
  localized_strings->Set("timezoneList",
      static_cast<chromeos::SystemSettingsProvider*>(
          chromeos::CrosSettings::Get()->GetProvider(
              chromeos::kSystemTimezone))->GetTimezoneList());
}

void SystemOptionsHandler::Initialize() {
  PrefService* pref_service = g_browser_process->local_state();
  base::FundamentalValue spoken_feedback_enabled(
      pref_service->GetBoolean(prefs::kSpokenFeedbackEnabled));
  web_ui()->CallJavascriptFunction(
      "options.SystemOptions.setSpokenFeedbackCheckboxState",
      spoken_feedback_enabled);
  base::FundamentalValue high_contrast_enabled(
      pref_service->GetBoolean(prefs::kHighContrastEnabled));
  web_ui()->CallJavascriptFunction(
      "options.SystemOptions.setHighContrastCheckboxState",
      high_contrast_enabled);
  base::FundamentalValue screen_magnifier_enabled(
      pref_service->GetBoolean(prefs::kScreenMagnifierEnabled));
  web_ui()->CallJavascriptFunction(
      "options.SystemOptions.setScreenMagnifierCheckboxState",
      screen_magnifier_enabled);
  base::FundamentalValue virtual_keyboard_enabled(
      pref_service->GetBoolean(prefs::kVirtualKeyboardEnabled));
  web_ui()->CallJavascriptFunction(
      "options.SystemOptions.setVirtualKeyboardCheckboxState",
      virtual_keyboard_enabled);

  chromeos::XInputHierarchyChangedEventListener::GetInstance()
      ->AddObserver(this);
  DeviceHierarchyChanged();
}

void SystemOptionsHandler::CheckTouchpadExists() {
  bool* exists = new bool;
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&TouchpadExistsFileThread, exists),
      base::Bind(&SystemOptionsHandler::TouchpadExists, AsWeakPtr(), exists));
}

void SystemOptionsHandler::CheckMouseExists() {
  bool* exists = new bool;
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&MouseExistsFileThread, exists),
      base::Bind(&SystemOptionsHandler::MouseExists, AsWeakPtr(), exists));
}

void SystemOptionsHandler::TouchpadExists(bool* exists) {
  base::FundamentalValue val(*exists);
  web_ui()->CallJavascriptFunction("options.SystemOptions.showTouchpadControls",
                                   val);
  delete exists;
}

void SystemOptionsHandler::MouseExists(bool* exists) {
  base::FundamentalValue val(*exists);
  web_ui()->CallJavascriptFunction("options.SystemOptions.showMouseControls",
                                   val);
  delete exists;
}

void SystemOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "spokenFeedbackChange",
      base::Bind(&SystemOptionsHandler::SpokenFeedbackChangeCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "highContrastChange",
      base::Bind(&SystemOptionsHandler::HighContrastChangeCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "screenMagnifierChange",
      base::Bind(&SystemOptionsHandler::ScreenMagnifierChangeCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "virtualKeyboardChange",
      base::Bind(&SystemOptionsHandler::VirtualKeyboardChangeCallback,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "decreaseScreenBrightness",
      base::Bind(&SystemOptionsHandler::DecreaseScreenBrightnessCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "increaseScreenBrightness",
      base::Bind(&SystemOptionsHandler::IncreaseScreenBrightnessCallback,
                 base::Unretained(this)));
}

void SystemOptionsHandler::DeviceHierarchyChanged() {
  CheckMouseExists();
  CheckTouchpadExists();
}

void SystemOptionsHandler::SpokenFeedbackChangeCallback(const ListValue* args) {
  bool enabled = false;
  args->GetBoolean(0, &enabled);

  chromeos::accessibility::EnableAccessibility(enabled, NULL);
}

void SystemOptionsHandler::HighContrastChangeCallback(const ListValue* args) {
  bool enabled = false;
  args->GetBoolean(0, &enabled);

  chromeos::accessibility::EnableHighContrast(enabled);
}

void SystemOptionsHandler::ScreenMagnifierChangeCallback(
    const ListValue* args) {
  bool enabled = false;
  args->GetBoolean(0, &enabled);

  chromeos::accessibility::EnableScreenMagnifier(enabled);
}

void SystemOptionsHandler::VirtualKeyboardChangeCallback(
    const ListValue* args) {
  bool enabled = false;
  args->GetBoolean(0, &enabled);

  chromeos::accessibility::EnableVirtualKeyboard(enabled);
}

void SystemOptionsHandler::DecreaseScreenBrightnessCallback(
    const ListValue* args) {
  // Do not allow the options button to turn off the backlight, as that
  // can make it very difficult to see the increase brightness button.
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      DecreaseScreenBrightness(false);
}

void SystemOptionsHandler::IncreaseScreenBrightnessCallback(
    const ListValue* args) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      IncreaseScreenBrightness();
}
