// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/cros_language_options_handler.h"

#include <map>
#include <set>
#include <vector>

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"

namespace chromeos {

CrosLanguageOptionsHandler::CrosLanguageOptionsHandler() {
}

CrosLanguageOptionsHandler::~CrosLanguageOptionsHandler() {
}

void CrosLanguageOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  LanguageOptionsHandlerCommon::GetLocalizedValues(localized_strings);

  localized_strings->SetString("ok_button", l10n_util::GetStringUTF16(IDS_OK));
  localized_strings->SetString("configure",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_CONFIGURE));
  localized_strings->SetString("input_method",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD));
  localized_strings->SetString("please_add_another_input_method",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_PLEASE_ADD_ANOTHER_INPUT_METHOD));
  localized_strings->SetString("input_method_instructions",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD_INSTRUCTIONS));
  localized_strings->SetString("switch_input_methods_hint",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SWITCH_INPUT_METHODS_HINT,
          ASCIIToUTF16("alt+shift")));
  localized_strings->SetString("select_previous_input_method_hint",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SELECT_PREVIOUS_INPUT_METHOD_HINT,
          ASCIIToUTF16("ctrl+space")));
  localized_strings->SetString("restart_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SIGN_OUT_BUTTON));

  // GetSupportedInputMethods() never return NULL.
  InputMethodLibrary *im_library =
      CrosLibrary::Get()->GetInputMethodLibrary();
  scoped_ptr<chromeos::InputMethodDescriptors> descriptors(
      im_library->GetSupportedInputMethods());
  localized_strings->Set("languageList", GetLanguageList(*descriptors));
  localized_strings->Set("inputMethodList", GetInputMethodList(*descriptors));
}

void CrosLanguageOptionsHandler::RegisterMessages() {
  LanguageOptionsHandlerCommon::RegisterMessages();

  web_ui_->RegisterMessageCallback("inputMethodDisable",
      NewCallback(this,
                  &CrosLanguageOptionsHandler::InputMethodDisableCallback));
  web_ui_->RegisterMessageCallback("inputMethodEnable",
      NewCallback(this,
                  &CrosLanguageOptionsHandler::InputMethodEnableCallback));
  web_ui_->RegisterMessageCallback("inputMethodOptionsOpen",
      NewCallback(this,
                  &CrosLanguageOptionsHandler::InputMethodOptionsOpenCallback));
  web_ui_->RegisterMessageCallback("uiLanguageRestart",
      NewCallback(this, &CrosLanguageOptionsHandler::RestartCallback));
}

ListValue* CrosLanguageOptionsHandler::GetInputMethodList(
    const chromeos::InputMethodDescriptors& descriptors) {
  ListValue* input_method_list = new ListValue();

  for (size_t i = 0; i < descriptors.size(); ++i) {
    const chromeos::InputMethodDescriptor& descriptor = descriptors[i];
    const std::string language_code =
        chromeos::input_method::GetLanguageCodeFromDescriptor(descriptor);
    const std::string display_name =
        chromeos::input_method::GetInputMethodDisplayNameFromId(descriptor.id);

    DictionaryValue* dictionary = new DictionaryValue();
    dictionary->SetString("id", descriptor.id);
    dictionary->SetString("displayName", display_name);

    // One input method can be associated with multiple languages, hence
    // we use a dictionary here.
    DictionaryValue* language_codes = new DictionaryValue();
    language_codes->SetBoolean(language_code, true);
    // Check kExtraLanguages to see if there are languages associated with
    // this input method. If these are present, add these.
    for (size_t j = 0; j < arraysize(chromeos::input_method::kExtraLanguages);
         ++j) {
      const std::string extra_input_method_id =
          chromeos::input_method::kExtraLanguages[j].input_method_id;
      const std::string extra_language_code =
          chromeos::input_method::kExtraLanguages[j].language_code;
      if (extra_input_method_id == descriptor.id) {
        language_codes->SetBoolean(extra_language_code, true);
      }
    }
    dictionary->Set("languageCodeSet", language_codes);

    input_method_list->Append(dictionary);
  }

  return input_method_list;
}

ListValue* CrosLanguageOptionsHandler::GetLanguageList(
    const chromeos::InputMethodDescriptors& descriptors) {
  std::set<std::string> language_codes;
  // Collect the language codes from the supported input methods.
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const chromeos::InputMethodDescriptor& descriptor = descriptors[i];
    const std::string language_code =
        chromeos::input_method::GetLanguageCodeFromDescriptor(descriptor);
    language_codes.insert(language_code);
  }
  // Collect the language codes from kExtraLanguages.
  for (size_t i = 0; i < arraysize(chromeos::input_method::kExtraLanguages);
       ++i) {
    const char* language_code =
        chromeos::input_method::kExtraLanguages[i].language_code;
    language_codes.insert(language_code);
  }

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
  for (std::set<std::string>::const_iterator iter = language_codes.begin();
       iter != language_codes.end(); ++iter) {
    const string16 display_name =
        chromeos::input_method::GetLanguageDisplayNameFromCode(*iter);
    const string16 native_display_name =
        chromeos::input_method::GetLanguageNativeDisplayNameFromCode(*iter);
    display_names.push_back(display_name);
    language_map[display_name] =
        std::make_pair(*iter, native_display_name);
  }
  DCHECK_EQ(display_names.size(), language_map.size());

  // Sort display names using locale specific sorter.
  l10n_util::SortStrings16(g_browser_process->GetApplicationLocale(),
                           &display_names);

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

string16 CrosLanguageOptionsHandler::GetProductName() {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_OS_NAME);
}

void CrosLanguageOptionsHandler::SetApplicationLocale(
    std::string language_code) {
  web_ui_->GetProfile()->ChangeAppLocale(
      language_code, Profile::APP_LOCALE_CHANGED_VIA_SETTINGS);
}

void CrosLanguageOptionsHandler::RestartCallback(const ListValue* args) {
  UserMetrics::RecordAction(UserMetricsAction("LanguageOptions_SignOut"));

  Browser* browser = Browser::GetBrowserForController(
      &web_ui_->tab_contents()->controller(), NULL);
  if (browser)
    browser->ExecuteCommand(IDC_EXIT);
}

void CrosLanguageOptionsHandler::InputMethodDisableCallback(
    const ListValue* args) {
  const std::string input_method_id = WideToASCII(ExtractStringValue(args));
  const std::string action = StringPrintf(
      "LanguageOptions_DisableInputMethod_%s", input_method_id.c_str());
  UserMetrics::RecordComputedAction(action);
}

void CrosLanguageOptionsHandler::InputMethodEnableCallback(
    const ListValue* args) {
  const std::string input_method_id = WideToASCII(ExtractStringValue(args));
  const std::string action = StringPrintf(
      "LanguageOptions_EnableInputMethod_%s", input_method_id.c_str());
  UserMetrics::RecordComputedAction(action);
}

void CrosLanguageOptionsHandler::InputMethodOptionsOpenCallback(
    const ListValue* args) {
  const std::string input_method_id = WideToASCII(ExtractStringValue(args));
  const std::string action = StringPrintf(
      "InputMethodOptions_Open_%s", input_method_id.c_str());
  UserMetrics::RecordComputedAction(action);
}

} // namespace chromeos
