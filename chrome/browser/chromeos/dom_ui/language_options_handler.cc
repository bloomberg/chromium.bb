// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_options_handler.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

LanguageOptionsHandler::LanguageOptionsHandler() {
}

LanguageOptionsHandler::~LanguageOptionsHandler() {
}

void LanguageOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"languagePage",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_DIALOG_TITLE));
  localized_strings->SetString(L"add_button",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_BUTTON));
  localized_strings->SetString(L"configure",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_CONFIGURE));
  localized_strings->SetString(L"input_method",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD));
  localized_strings->SetString(L"languages",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_LANGUAGES));
  localized_strings->SetString(L"remove_button",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_REMOVE_BUTTON));
  localized_strings->SetString(L"others",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_OTHERS));
  localized_strings->SetString(L"is_displayed_in_this_language",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  localized_strings->SetString(L"display_in_this_language",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));

  // Build mappings of locale code (language code) to display name
  // (ex. "en-US" => "English (United States)".
  const std::vector<std::string>& locales = l10n_util::GetAvailableLocales();
  for (size_t i = 0; i < locales.size(); ++i) {
    localized_strings->SetString(UTF8ToWide(locales[i]),
        chromeos::input_method::GetLanguageDisplayNameFromCode(locales[i]));
  }

  localized_strings->Set(L"inputMethodList", GetInputMethodList());
}

ListValue* LanguageOptionsHandler::GetInputMethodList() {
  using chromeos::CrosLibrary;

  ListValue* input_method_list = new ListValue();

  // GetSupportedLanguages() never return NULL.
  scoped_ptr<chromeos::InputMethodDescriptors> descriptors(
      CrosLibrary::Get()->GetInputMethodLibrary()->GetSupportedInputMethods());
  for (size_t i = 0; i < descriptors->size(); ++i) {
    const chromeos::InputMethodDescriptor& descriptor = descriptors->at(i);
    const std::string language_code =
        chromeos::input_method::GetLanguageCodeFromDescriptor(descriptor);
    const std::string display_name =
        chromeos::input_method::GetInputMethodDisplayNameFromId(descriptor.id);

    DictionaryValue* dictionary = new DictionaryValue();
    dictionary->SetString(L"id", UTF8ToWide(descriptor.id));
    dictionary->SetString(L"displayName", UTF8ToWide(display_name));
    dictionary->SetString(L"languageCode", UTF8ToWide(language_code));
    input_method_list->Append(dictionary);
  }

  return input_method_list;
}
