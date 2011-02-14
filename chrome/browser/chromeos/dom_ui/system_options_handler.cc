// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/system_options_handler.h"

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/dom_ui/system_settings_provider.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

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
  localized_strings->SetString("datetime_title",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME));
  localized_strings->SetString("timezone",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_TIMEZONE_DESCRIPTION));

  localized_strings->SetString("touchpad",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_TOUCHPAD));
  localized_strings->SetString("enable_tap_to_click",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_TAP_TO_CLICK_ENABLED_DESCRIPTION));
  localized_strings->SetString("sensitivity",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SENSITIVITY_DESCRIPTION));
  localized_strings->SetString("sensitivity_less",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SENSITIVITY_LESS_DESCRIPTION));
  localized_strings->SetString("sensitivity_more",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SENSITIVITY_MORE_DESCRIPTION));

  localized_strings->SetString("language",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_LANGUAGE));
  localized_strings->SetString("language_customize",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_CUSTOMIZE));
  localized_strings->SetString("modifier_keys_customize",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_MODIFIER_KEYS_CUSTOMIZE));

  localized_strings->SetString("accessibility_title",
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
  FundamentalValue checked(acc_enabled);
  web_ui_->CallJavascriptFunction(
      L"options.SystemOptions.SetAccessibilityCheckboxState", checked);
}

void SystemOptionsHandler::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("accessibilityChange",
      NewCallback(this, &SystemOptionsHandler::AccessibilityChangeCallback));
}

void SystemOptionsHandler::AccessibilityChangeCallback(const ListValue* args) {
  std::string checked_str;
  args->GetString(0, &checked_str);
  bool accessibility_enabled = (checked_str == "true");
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kAccessibilityEnabled, accessibility_enabled);
}
