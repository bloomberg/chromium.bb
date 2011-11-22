// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "content/browser/tab_contents/tab_contents.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/system/touchpad_settings.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/chromeos/system_settings_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/extensions/extension.h"
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

}

SystemOptionsHandler::SystemOptionsHandler()
    : chromeos::CrosOptionsPageUIHandler(
        new chromeos::SystemSettingsProvider()) {
}

SystemOptionsHandler::~SystemOptionsHandler() {
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

  localized_strings->SetString("touchpad",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_TOUCHPAD));
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
  localized_strings->SetString("accessibility",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_ACCESSIBILITY_DESCRIPTION));

  localized_strings->Set("timezoneList",
      reinterpret_cast<chromeos::SystemSettingsProvider*>(
          settings_provider_.get())->GetTimezoneList());
}

void SystemOptionsHandler::Initialize() {
  DCHECK(web_ui_);
  PrefService* pref_service = g_browser_process->local_state();
  bool acc_enabled = pref_service->GetBoolean(prefs::kAccessibilityEnabled);
  base::FundamentalValue checked(acc_enabled);
  web_ui_->CallJavascriptFunction(
      "options.SystemOptions.SetAccessibilityCheckboxState", checked);

  bool* exists = new bool;
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&TouchpadExistsFileThread, exists),
      base::Bind(&SystemOptionsHandler::TouchpadExists, AsWeakPtr(), exists));
}

void SystemOptionsHandler::TouchpadExists(bool* exists) {
  if (*exists)
    web_ui_->CallJavascriptFunction(
        "options.SystemOptions.showTouchpadControls");
  delete exists;
}

void SystemOptionsHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("accessibilityChange",
      base::Bind(&SystemOptionsHandler::AccessibilityChangeCallback,
                 base::Unretained(this)));

  web_ui_->RegisterMessageCallback("decreaseScreenBrightness",
      base::Bind(&SystemOptionsHandler::DecreaseScreenBrightnessCallback,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("increaseScreenBrightness",
      base::Bind(&SystemOptionsHandler::IncreaseScreenBrightnessCallback,
                 base::Unretained(this)));
}

void SystemOptionsHandler::AccessibilityChangeCallback(const ListValue* args) {
  std::string checked_str;
  args->GetString(0, &checked_str);
  bool accessibility_enabled = (checked_str == "true");

  chromeos::accessibility::EnableAccessibility(accessibility_enabled, NULL);
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
