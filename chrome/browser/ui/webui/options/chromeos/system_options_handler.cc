// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/system_options_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/json_value_serializer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
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

  localized_strings->SetString("bluetooth",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_BLUETOOTH));
  localized_strings->SetString("enableBluetooth",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_BLUETOOTH_ENABLE));
  localized_strings->SetString("findBluetoothDevices",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_FIND_BLUETOOTH_DEVICES));

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

  // Bluetooth support is a work in progress.  Supress the feature unless
  // explicitly enabled via a command line flag.
  // TODO (kevers) - Test for presence of bluetooth hardware.
  if (CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kEnableBluetooth)) {
    web_ui_->CallJavascriptFunction(
        "options.SystemOptions.ShowBluetoothSettings");
  }
}

void SystemOptionsHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("accessibilityChange",
      base::Bind(&SystemOptionsHandler::AccessibilityChangeCallback,
                 base::Unretained(this)));
}

void SystemOptionsHandler::AccessibilityChangeCallback(const ListValue* args) {
  std::string checked_str;
  args->GetString(0, &checked_str);
  bool accessibility_enabled = (checked_str == "true");

  chromeos::accessibility::EnableAccessibility(accessibility_enabled, NULL);
}
