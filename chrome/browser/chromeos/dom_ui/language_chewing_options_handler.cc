// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_chewing_options_handler.h"

#include <limits>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "grit/generated_resources.h"

namespace {

// Returns an i18n-content value corresponding to |preference|.
template <typename T>
std::wstring GetI18nContentValue(const T& preference) {
  return ASCIIToWide(preference.ibus_config_name) + L"Content";
}

// Returns a property name of templateData corresponding to |preference|.
template <typename T>
std::wstring GetTemplateDataPropertyName(const T& preference) {
  return ASCIIToWide(preference.ibus_config_name) + L"Value";
}

// Returns an property name of templateData corresponding the value of the min
// attribute.
template <typename T>
std::wstring GetTemplateDataMinName(const T& preference) {
  return ASCIIToWide(preference.ibus_config_name) + L"Min";
}

// Returns an property name of templateData corresponding the value of the max
// attribute.
template <typename T>
std::wstring GetTemplateDataMaxName(const T& preference) {
  return ASCIIToWide(preference.ibus_config_name) + L"Max";
}

// Returns a list of options for LanguageMultipleChoicePreference.
ListValue* CreateMultipleChoiceList(
    const chromeos::LanguageMultipleChoicePreference<const char*>& preference) {
  int list_length = 0;
  for (size_t i = 0;
       i < chromeos::LanguageMultipleChoicePreference<const char*>::kMaxItems;
       ++i) {
    if (preference.values_and_ids[i].item_message_id == 0)
      break;
    ++list_length;
  }
  DCHECK_GT(list_length, 0);

  ListValue* list_value = new ListValue();
  for (int i = 0; i < list_length; ++i) {
    ListValue* option = new ListValue();
    option->Append(Value::CreateStringValue(
        preference.values_and_ids[i].ibus_config_value));
    option->Append(Value::CreateStringValue(l10n_util::GetString(
        preference.values_and_ids[i].item_message_id)));
    list_value->Append(option);
  }
  return list_value;
}

}  // namespace

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
        IntToWString(preference.min_pref_value));
    localized_strings->SetString(
        GetTemplateDataMaxName(preference),
        IntToWString(preference.max_pref_value));
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
        CreateMultipleChoiceList(preference));
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
      IntToWString(hsu_sel_key_type_min));
  localized_strings->SetString(
      GetTemplateDataMaxName(chromeos::kChewingHsuSelKeyType),
      IntToWString(hsu_sel_key_type_max));
}
