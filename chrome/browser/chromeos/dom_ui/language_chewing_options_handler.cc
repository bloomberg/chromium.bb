// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_chewing_options_handler.h"

#include <limits>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/dom_ui/language_options_util.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "grit/generated_resources.h"

LanguageChewingOptionsHandler::LanguageChewingOptionsHandler() {
}

LanguageChewingOptionsHandler::~LanguageChewingOptionsHandler() {
}

void LanguageChewingOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Language Chewing page - ChromeOS
  for (size_t i = 0; i < arraysize(chromeos::kChewingBooleanPrefs); ++i) {
    localized_strings->SetString(
        GetI18nContentValue(chromeos::kChewingBooleanPrefs[i]),
        l10n_util::GetString(chromeos::kChewingBooleanPrefs[i].message_id));
  }

  for (size_t i = 0; i < arraysize(chromeos::kChewingIntegerPrefs); ++i) {
    const chromeos::LanguageIntegerRangePreference& preference =
        chromeos::kChewingIntegerPrefs[i];
    localized_strings->SetString(
        GetI18nContentValue(preference),
        l10n_util::GetString(preference.message_id));
    localized_strings->SetString(
        GetTemplateDataMinName(preference),
        UTF8ToWide(base::IntToString(preference.min_pref_value)));
    localized_strings->SetString(
        GetTemplateDataMaxName(preference),
        UTF8ToWide(base::IntToString(preference.max_pref_value)));
  }

  for (size_t i = 0; i < arraysize(chromeos::kChewingMultipleChoicePrefs);
       ++i) {
    const chromeos::LanguageMultipleChoicePreference<const char*>& preference =
        chromeos::kChewingMultipleChoicePrefs[i];
    localized_strings->SetString(
        GetI18nContentValue(preference),
        l10n_util::GetString(preference.label_message_id));
    localized_strings->Set(
        GetTemplateDataPropertyName(preference),
        chromeos::CreateMultipleChoiceList(preference));
  }

  localized_strings->SetString(
      GetI18nContentValue(chromeos::kChewingHsuSelKeyType),
      l10n_util::GetString(chromeos::kChewingHsuSelKeyType.label_message_id));

  int hsu_sel_key_type_min = std::numeric_limits<int>::max();
  int hsu_sel_key_type_max = std::numeric_limits<int>::min();
  for (size_t i = 0;
       i < chromeos::LanguageMultipleChoicePreference<int>::kMaxItems;
       ++i) {
    if (chromeos::kChewingHsuSelKeyType.values_and_ids[i].item_message_id == 0)
      break;
    const int value =
        chromeos::kChewingHsuSelKeyType.values_and_ids[i].ibus_config_value;
    if (value <= hsu_sel_key_type_min)
      hsu_sel_key_type_min = value;
    if (value >= hsu_sel_key_type_max)
      hsu_sel_key_type_max = value;
  }
  DCHECK_NE(hsu_sel_key_type_min, std::numeric_limits<int>::max());
  DCHECK_NE(hsu_sel_key_type_max, std::numeric_limits<int>::min());

  localized_strings->SetString(
      GetTemplateDataMinName(chromeos::kChewingHsuSelKeyType),
      UTF8ToWide(base::IntToString(hsu_sel_key_type_min)));
  localized_strings->SetString(
      GetTemplateDataMaxName(chromeos::kChewingHsuSelKeyType),
      UTF8ToWide(base::IntToString(hsu_sel_key_type_max)));
}
