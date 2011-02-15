// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/webui/language_chewing_options_handler.h"

#include <limits>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/webui/language_options_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kI18nPrefix[] = "Chewing_";
}  // namespace

namespace chromeos {

LanguageChewingOptionsHandler::LanguageChewingOptionsHandler() {
}

LanguageChewingOptionsHandler::~LanguageChewingOptionsHandler() {
}

void LanguageChewingOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "languageChewingPage",
                IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTINGS_TITLE);

  for (size_t i = 0; i < language_prefs::kNumChewingBooleanPrefs; ++i) {
    localized_strings->SetString(
        GetI18nContentValue(language_prefs::kChewingBooleanPrefs[i],
                            kI18nPrefix),
        l10n_util::GetStringUTF16(
            language_prefs::kChewingBooleanPrefs[i].message_id));
  }

  // For maximum Chinese characters in pre-edit buffer, we use slider UI.
  {
    const language_prefs::LanguageIntegerRangePreference& preference =
        language_prefs::kChewingIntegerPrefs[
            language_prefs::kChewingMaxChiSymbolLenIndex];
    localized_strings->SetString(
        GetI18nContentValue(preference, kI18nPrefix),
        l10n_util::GetStringUTF16(preference.message_id));
    localized_strings->SetString(
        GetTemplateDataMinName(preference, kI18nPrefix),
        base::IntToString(preference.min_pref_value));
    localized_strings->SetString(
        GetTemplateDataMaxName(preference, kI18nPrefix),
        base::IntToString(preference.max_pref_value));
  }

  // For number of candidates per page, we use select-option UI.
  {
    const language_prefs::LanguageIntegerRangePreference& preference =
        language_prefs::kChewingIntegerPrefs[
            language_prefs::kChewingCandPerPageIndex];
    localized_strings->SetString(
        GetI18nContentValue(preference, kI18nPrefix),
        l10n_util::GetStringUTF16(preference.message_id));
    ListValue* list_value = new ListValue();
    for (int i = preference.min_pref_value; i <= preference.max_pref_value;
         ++i) {
      ListValue* option = new ListValue();
      option->Append(CreateValue(i));
      option->Append(CreateValue(i));
      list_value->Append(option);
    }
    localized_strings->Set(GetTemplateDataPropertyName(preference, kI18nPrefix),
                           list_value);
  }

  for (size_t i = 0; i < language_prefs::kNumChewingMultipleChoicePrefs;
       ++i) {
    const language_prefs::LanguageMultipleChoicePreference<const char*>&
        preference = language_prefs::kChewingMultipleChoicePrefs[i];
    localized_strings->SetString(
        GetI18nContentValue(preference, kI18nPrefix),
        l10n_util::GetStringUTF16(preference.label_message_id));
    localized_strings->Set(
        GetTemplateDataPropertyName(preference, kI18nPrefix),
        CreateMultipleChoiceList(preference));
  }

  localized_strings->SetString(
      GetI18nContentValue(language_prefs::kChewingHsuSelKeyType, kI18nPrefix),
      l10n_util::GetStringUTF16(
          language_prefs::kChewingHsuSelKeyType.label_message_id));
  localized_strings->Set(
      GetTemplateDataPropertyName(language_prefs::kChewingHsuSelKeyType,
                                  kI18nPrefix),
      CreateMultipleChoiceList(language_prefs::kChewingHsuSelKeyType));
}

}  // namespace chromeos
