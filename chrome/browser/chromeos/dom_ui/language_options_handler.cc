// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_options_handler.h"

#include <map>
#include <set>
#include <string>
#include <utility>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace chromeos {

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
  localized_strings->SetString(L"please_add_another_input_method",
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_PLEASE_ADD_ANOTHER_INPUT_METHOD));
  localized_strings->SetString(L"please_add_another_language",
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_PLEASE_ADD_ANOTHER_LANGUAGE));
  localized_strings->SetString(L"remove_button",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_REMOVE_BUTTON));
  localized_strings->SetString(L"restart_button",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_RESTART_BUTTON));
  localized_strings->SetString(L"add_language_instructions",
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_LANGUAGE_INSTRUCTIONS));
  localized_strings->SetString(L"input_method_instructions",
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD_INSTRUCTIONS));
  localized_strings->SetString(L"switch_input_methods_hint",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SWITCH_INPUT_METHODS_HINT,
          L"alt+shift"));
  localized_strings->SetString(L"select_previous_input_method_hint",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SELECT_PREVIOUS_INPUT_METHOD_HINT,
          L"ctrl+space"));
  localized_strings->SetString(L"cannot_be_displayed_in_this_language",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_CANNOT_BE_DISPLAYED_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  localized_strings->SetString(L"is_displayed_in_this_language",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  localized_strings->SetString(L"display_in_this_language",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  localized_strings->SetString(L"restart_required",
      l10n_util::GetString(IDS_OPTIONS_RESTART_REQUIRED));

  // GetSupportedInputMethods() never return NULL.
  scoped_ptr<InputMethodDescriptors> descriptors(
      CrosLibrary::Get()->GetInputMethodLibrary()->GetSupportedInputMethods());

  // The followigns are resources, rather than local strings.
  localized_strings->SetString(L"currentUiLanguageCode",
      UTF8ToWide(g_browser_process->GetApplicationLocale()));
  localized_strings->Set(L"inputMethodList", GetInputMethodList(*descriptors));
  localized_strings->Set(L"languageList", GetLanguageList(*descriptors));
  localized_strings->Set(L"uiLanguageCodeSet", GetUiLanguageCodeSet());
}

void LanguageOptionsHandler::RegisterMessages() {
  DCHECK(dom_ui_);
  dom_ui_->RegisterMessageCallback("uiLanguageChange",
      NewCallback(this, &LanguageOptionsHandler::UiLanguageChangeCallback));
  dom_ui_->RegisterMessageCallback("uiLanguageRestart",
      NewCallback(this, &LanguageOptionsHandler::RestartCallback));
}

ListValue* LanguageOptionsHandler::GetInputMethodList(
    const InputMethodDescriptors& descriptors) {
  ListValue* input_method_list = new ListValue();

  for (size_t i = 0; i < descriptors.size(); ++i) {
    const InputMethodDescriptor& descriptor = descriptors[i];
    const std::string language_code =
        input_method::GetLanguageCodeFromDescriptor(descriptor);
    const std::string display_name =
        input_method::GetInputMethodDisplayNameFromId(descriptor.id);

    DictionaryValue* dictionary = new DictionaryValue();
    dictionary->SetString("id", descriptor.id);
    dictionary->SetString("displayName", display_name);

    // One input method can be associated with multiple languages, hence
    // we use a dictionary here.
    DictionaryValue* language_codes = new DictionaryValue();
    language_codes->SetBoolean(language_code, true);
    // Check kExtraLanguages to see if there are languages associated with
    // this input method. If these are present, add these.
    for (size_t j = 0; j < arraysize(input_method::kExtraLanguages); ++j) {
      const std::string extra_input_method_id =
          input_method::kExtraLanguages[j].input_method_id;
      const std::string extra_language_code =
          input_method::kExtraLanguages[j].language_code;
      if (extra_input_method_id == descriptor.id) {
        language_codes->SetBoolean(extra_language_code, true);
      }
    }
    dictionary->Set("languageCodeSet", language_codes);

    input_method_list->Append(dictionary);
  }

  return input_method_list;
}

ListValue* LanguageOptionsHandler::GetLanguageList(
    const InputMethodDescriptors& descriptors) {
  std::set<std::string> language_codes;
  // Collect the language codes from the supported input methods.
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const InputMethodDescriptor& descriptor = descriptors[i];
    const std::string language_code =
        input_method::GetLanguageCodeFromDescriptor(descriptor);
    language_codes.insert(language_code);
  }
  // Collect the language codes from kExtraLanguages.
  for (size_t i = 0; i < arraysize(input_method::kExtraLanguages); ++i) {
    const char* language_code =
        input_method::kExtraLanguages[i].language_code;
    language_codes.insert(language_code);
  }

  // Map of display name -> {language code, native_display_name}.
  // In theory, we should be able to create a map that is sorted by
  // display names using ICU comparator, but doing it is hard, thus we'll
  // use an auxiliary vector to achieve the same result.
  typedef std::pair<std::string, std::wstring> LanguagePair;
  typedef std::map<std::wstring, LanguagePair> LanguageMap;
  LanguageMap language_map;
  // The auxiliary vector mentioned above.
  std::vector<std::wstring> display_names;

  // Build the list of display names, and build the language map.
  for (std::set<std::string>::const_iterator iter = language_codes.begin();
       iter != language_codes.end(); ++iter) {
    const std::wstring display_name =
        input_method::GetLanguageDisplayNameFromCode(*iter);
    const std::wstring native_display_name =
        input_method::GetLanguageNativeDisplayNameFromCode(*iter);
    display_names.push_back(display_name);
    language_map[display_name] =
        std::make_pair(*iter, native_display_name);
  }
  DCHECK_EQ(display_names.size(), language_map.size());

  // Sort display names using locale specific sorter.
  l10n_util::SortStrings(g_browser_process->GetApplicationLocale(),
                         &display_names);

  // Build the language list from the language map.
  ListValue* language_list = new ListValue();
  for (size_t i = 0; i < display_names.size(); ++i) {
    const LanguagePair& pair = language_map[display_names[i]];
    DictionaryValue* dictionary = new DictionaryValue();
    dictionary->SetString(L"code",  pair.first);
    dictionary->SetString(L"displayName", display_names[i]);
    dictionary->SetString(L"nativeDisplayName", pair.second);
    language_list->Append(dictionary);
  }

  return language_list;
}

DictionaryValue* LanguageOptionsHandler::GetUiLanguageCodeSet() {
  DictionaryValue* dictionary = new DictionaryValue();
  const std::vector<std::string>& available_locales =
      l10n_util::GetAvailableLocales();
  for (size_t i = 0; i < available_locales.size(); ++i) {
    dictionary->SetBoolean(available_locales[i], true);
  }
  return dictionary;
}

void LanguageOptionsHandler::UiLanguageChangeCallback(
    const Value* value) {
  if (!value || !value->IsType(Value::TYPE_LIST)) {
    NOTREACHED();
    LOG(INFO) << "NOTREACHED";
    return;
  }
  const ListValue* list_value = static_cast<const ListValue*>(value);
  std::string language_code;
  if (list_value->GetSize() != 1 ||
      !list_value->GetString(0, &language_code)) {
    NOTREACHED();
    LOG(INFO) << "NOTREACHED";
    return;
  }
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kApplicationLocale, language_code);
  prefs->SavePersistentPrefs();
  dom_ui_->CallJavascriptFunction(
      L"options.LanguageOptions.uiLanguageSaved");
}

void LanguageOptionsHandler::RestartCallback(const Value* value) {
  Browser* browser = Browser::GetBrowserForController(
      &dom_ui_->tab_contents()->controller(), NULL);
  // TODO(kochi): For ChromiumOS, just exiting means browser restart.
  // Implement browser restart for Chromium Win/Linux/Mac.
  if (browser)
    browser->ExecuteCommand(IDC_EXIT);
}

}  // namespace chromeos
