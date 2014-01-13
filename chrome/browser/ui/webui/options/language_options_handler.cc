// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/language_options_handler.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace options {

LanguageOptionsHandler::LanguageOptionsHandler() {
}

LanguageOptionsHandler::~LanguageOptionsHandler() {
}

void LanguageOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  LanguageOptionsHandlerCommon::GetLocalizedValues(localized_strings);

  RegisterTitle(localized_strings, "languagePage",
                IDS_OPTIONS_SETTINGS_LANGUAGES_DIALOG_TITLE);
  localized_strings->SetString("restart_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_RELAUNCH_BUTTON));
  localized_strings->Set("languageList", GetLanguageList());
}

void LanguageOptionsHandler::RegisterMessages() {
  LanguageOptionsHandlerCommon::RegisterMessages();

  web_ui()->RegisterMessageCallback("uiLanguageRestart",
      base::Bind(&LanguageOptionsHandler::RestartCallback,
                 base::Unretained(this)));
}

base::ListValue* LanguageOptionsHandler::GetLanguageList() {
  // Collect the language codes from the supported accept-languages.
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &language_codes);

  // Map of display name -> {language code, native_display_name}.
  // In theory, we should be able to create a map that is sorted by
  // display names using ICU comparator, but doing it is hard, thus we'll
  // use an auxiliary vector to achieve the same result.
  typedef std::pair<std::string, base::string16> LanguagePair;
  typedef std::map<base::string16, LanguagePair> LanguageMap;
  LanguageMap language_map;
  // The auxiliary vector mentioned above.
  std::vector<base::string16> display_names;

  // Build the list of display names, and build the language map.
  for (size_t i = 0; i < language_codes.size(); ++i) {
    base::string16 display_name =
        l10n_util::GetDisplayNameForLocale(language_codes[i], app_locale,
                                           false);
    base::string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(language_codes[i], language_codes[i],
                                           false);
    display_names.push_back(display_name);
    language_map[display_name] =
        std::make_pair(language_codes[i], native_display_name);
  }
  DCHECK_EQ(display_names.size(), language_map.size());

  // Sort display names using locale specific sorter.
  l10n_util::SortStrings16(app_locale, &display_names);

  // Build the language list from the language map.
  base::ListValue* language_list = new base::ListValue();
  for (size_t i = 0; i < display_names.size(); ++i) {
    base::string16& display_name = display_names[i];
    base::string16 adjusted_display_name(display_name);
    base::i18n::AdjustStringForLocaleDirection(&adjusted_display_name);

    const LanguagePair& pair = language_map[display_name];
    base::string16 adjusted_native_display_name(pair.second);
    base::i18n::AdjustStringForLocaleDirection(&adjusted_native_display_name);

    bool has_rtl_chars = base::i18n::StringContainsStrongRTLChars(display_name);
    std::string directionality = has_rtl_chars ? "rtl" : "ltr";

    base::DictionaryValue* dictionary = new base::DictionaryValue();
    dictionary->SetString("code",  pair.first);
    dictionary->SetString("displayName", adjusted_display_name);
    dictionary->SetString("textDirection", directionality);
    dictionary->SetString("nativeDisplayName", adjusted_native_display_name);
    language_list->Append(dictionary);
  }

  return language_list;
}

base::string16 LanguageOptionsHandler::GetProductName() {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

void LanguageOptionsHandler::SetApplicationLocale(
    const std::string& language_code) {
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(prefs::kApplicationLocale, language_code);
}

void LanguageOptionsHandler::RestartCallback(const base::ListValue* args) {
  content::RecordAction(UserMetricsAction("LanguageOptions_Restart"));
  chrome::AttemptRestart();
}

}  // namespace options
