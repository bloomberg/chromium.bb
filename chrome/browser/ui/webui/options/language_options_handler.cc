// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/language_options_handler.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

LanguageOptionsHandler::LanguageOptionsHandler() {
}

LanguageOptionsHandler::~LanguageOptionsHandler() {
}

void LanguageOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  LanguageOptionsHandlerCommon::GetLocalizedValues(localized_strings);

  localized_strings->SetString("restart_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_RELAUNCH_BUTTON));
  localized_strings->Set("languageList", GetLanguageList());
}

void LanguageOptionsHandler::RegisterMessages() {
  LanguageOptionsHandlerCommon::RegisterMessages();

  web_ui_->RegisterMessageCallback("uiLanguageRestart",
      NewCallback(this, &LanguageOptionsHandler::RestartCallback));
}

ListValue* LanguageOptionsHandler::GetLanguageList() {
  // Collect the language codes from the supported accept-languages.
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &language_codes);

  // Map of display name -> {language code, native_display_name}.
  // In theory, we should be able to create a map that is sorted by
  // display names using ICU comparator, but doing it is hard, thus we'll
  // use an auxiliary vector to achieve the same result.
  typedef std::pair<std::string, string16> LanguagePair;
  typedef std::map<string16, LanguagePair> LanguageMap;
  LanguageMap language_map;
  // The auxiliary vector mentioned above.
  std::vector<string16> display_names;

  // Build the list of display names, and build the language map.
  for (size_t i = 0; i < language_codes.size(); ++i) {
    string16 display_name =
        l10n_util::GetDisplayNameForLocale(language_codes[i], app_locale,
                                           false);
    base::i18n::AdjustStringForLocaleDirection(&display_name);
    string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(language_codes[i], language_codes[i],
                                           false);
    base::i18n::AdjustStringForLocaleDirection(&native_display_name);
    display_names.push_back(display_name);
    language_map[display_name] =
        std::make_pair(language_codes[i], native_display_name);
  }
  DCHECK_EQ(display_names.size(), language_map.size());

  // Sort display names using locale specific sorter.
  l10n_util::SortStrings16(app_locale, &display_names);

  // Build the language list from the language map.
  ListValue* language_list = new ListValue();
  for (size_t i = 0; i < display_names.size(); ++i) {
    const LanguagePair& pair = language_map[display_names[i]];
    DictionaryValue* dictionary = new DictionaryValue();
    dictionary->SetString("code",  pair.first);
    dictionary->SetString("displayName", display_names[i]);
    dictionary->SetString("nativeDisplayName", pair.second);
    language_list->Append(dictionary);
  }

  return language_list;
}

string16 LanguageOptionsHandler::GetProductName() {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

void LanguageOptionsHandler::SetApplicationLocale(
    const std::string& language_code) {
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(prefs::kApplicationLocale, language_code);
}

void LanguageOptionsHandler::RestartCallback(const ListValue* args) {
  UserMetrics::RecordAction(UserMetricsAction("LanguageOptions_Restart"));

  // Set the flag to restore state after the restart.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  BrowserList::CloseAllBrowsersAndExit();
}
