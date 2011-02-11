// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_mozc_options_handler.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/dom_ui/language_options_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kI18nPrefix[] = "mozc_";
}  // namespace

namespace chromeos {

LanguageMozcOptionsHandler::LanguageMozcOptionsHandler() {
}

LanguageMozcOptionsHandler::~LanguageMozcOptionsHandler() {
}

void LanguageMozcOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "languageMozcPage",
                IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SETTINGS_TITLE);

  for (size_t i = 0; i < language_prefs::kNumMozcBooleanPrefs; ++i) {
    localized_strings->SetString(
        GetI18nContentValue(language_prefs::kMozcBooleanPrefs[i], kI18nPrefix),
        l10n_util::GetStringUTF16(
            language_prefs::kMozcBooleanPrefs[i].message_id));
  }

  for (size_t i = 0; i < language_prefs::kNumMozcMultipleChoicePrefs; ++i) {
    const language_prefs::LanguageMultipleChoicePreference<const char*>&
        preference = language_prefs::kMozcMultipleChoicePrefs[i];
    localized_strings->SetString(
        GetI18nContentValue(preference, kI18nPrefix),
        l10n_util::GetStringUTF16(preference.label_message_id));
    localized_strings->Set(GetTemplateDataPropertyName(preference, kI18nPrefix),
                           CreateMultipleChoiceList(preference));
  }

  for (size_t i = 0; i < language_prefs::kNumMozcIntegerPrefs; ++i) {
    const language_prefs::LanguageIntegerRangePreference& preference =
        language_prefs::kMozcIntegerPrefs[i];
    localized_strings->SetString(
        GetI18nContentValue(preference, kI18nPrefix),
        l10n_util::GetStringUTF16(preference.message_id));
    ListValue* list_value = new ListValue();
    for (int j = preference.min_pref_value; j <= preference.max_pref_value;
         ++j) {
      ListValue* option = new ListValue();
      option->Append(CreateValue(j));
      option->Append(CreateValue(j));
      list_value->Append(option);
    }
    localized_strings->Set(GetTemplateDataPropertyName(preference, kI18nPrefix),
                           list_value);
  }
}

}  // namespace chromeos
