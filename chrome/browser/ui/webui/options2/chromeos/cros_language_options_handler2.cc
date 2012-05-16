// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/cros_language_options_handler2.h"

#include <map>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::UserMetricsAction;

namespace chromeos {
namespace options2 {

CrosLanguageOptionsHandler::CrosLanguageOptionsHandler() {
}

CrosLanguageOptionsHandler::~CrosLanguageOptionsHandler() {
}

void CrosLanguageOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  ::options2::LanguageOptionsHandlerCommon::GetLocalizedValues(
      localized_strings);

  RegisterTitle(localized_strings, "languagePage",
                IDS_OPTIONS_SETTINGS_LANGUAGES_AND_INPUT_DIALOG_TITLE);
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
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SWITCH_INPUT_METHODS_HINT));
  localized_strings->SetString("select_previous_input_method_hint",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SELECT_PREVIOUS_INPUT_METHOD_HINT));
  localized_strings->SetString("restart_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_SIGN_OUT_BUTTON));
  localized_strings->SetString("virtual_keyboard_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_VIRTUAL_KEYBOARD_BUTTON));

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::GetInstance();
  // GetSupportedInputMethods() never return NULL.
  scoped_ptr<input_method::InputMethodDescriptors> descriptors(
      manager->GetSupportedInputMethods());
  localized_strings->Set("languageList", GetLanguageList(*descriptors));
  localized_strings->Set("inputMethodList", GetInputMethodList(*descriptors));
}

void CrosLanguageOptionsHandler::RegisterMessages() {
  ::options2::LanguageOptionsHandlerCommon::RegisterMessages();

  web_ui()->RegisterMessageCallback("inputMethodDisable",
      base::Bind(&CrosLanguageOptionsHandler::InputMethodDisableCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("inputMethodEnable",
      base::Bind(&CrosLanguageOptionsHandler::InputMethodEnableCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("inputMethodOptionsOpen",
      base::Bind(&CrosLanguageOptionsHandler::InputMethodOptionsOpenCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("uiLanguageRestart",
      base::Bind(&CrosLanguageOptionsHandler::RestartCallback,
                 base::Unretained(this)));
}

ListValue* CrosLanguageOptionsHandler::GetInputMethodList(
    const input_method::InputMethodDescriptors& descriptors) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::GetInstance();

  ListValue* input_method_list = new ListValue();

  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor =
        descriptors[i];
    const std::string language_code = descriptor.language_code();
    const std::string display_name =
        manager->GetInputMethodUtil()->GetInputMethodDisplayNameFromId(
            descriptor.id());

    DictionaryValue* dictionary = new DictionaryValue();
    dictionary->SetString("id", descriptor.id());
    dictionary->SetString("displayName", display_name);

    // One input method can be associated with multiple languages, hence
    // we use a dictionary here.
    DictionaryValue* language_codes = new DictionaryValue();
    language_codes->SetBoolean(language_code, true);
    // Check kExtraLanguages to see if there are languages associated with
    // this input method. If these are present, add these.
    for (size_t j = 0; j < input_method::kExtraLanguagesLength; ++j) {
      const std::string extra_input_method_id =
          input_method::kExtraLanguages[j].input_method_id;
      const std::string extra_language_code =
          input_method::kExtraLanguages[j].language_code;
      if (extra_input_method_id == descriptor.id()) {
        language_codes->SetBoolean(extra_language_code, true);
      }
    }
    dictionary->Set("languageCodeSet", language_codes);

    input_method_list->Append(dictionary);
  }

  return input_method_list;
}

ListValue* CrosLanguageOptionsHandler::GetLanguageList(
    const input_method::InputMethodDescriptors& descriptors) {
  std::set<std::string> language_codes;
  // Collect the language codes from the supported input methods.
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor = descriptors[i];
    const std::string language_code = descriptor.language_code();
    language_codes.insert(language_code);
  }
  // Collect the language codes from kExtraLanguages.
  for (size_t i = 0; i < input_method::kExtraLanguagesLength; ++i) {
    const char* language_code =
        input_method::kExtraLanguages[i].language_code;
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
        input_method::InputMethodUtil::GetLanguageDisplayNameFromCode(*iter);
    const string16 native_display_name =
        input_method::InputMethodUtil::GetLanguageNativeDisplayNameFromCode(
            *iter);
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
    // Sets the directionality of the display language name.
    string16 display_name(display_names[i]);
    bool markup_removal =
        base::i18n::UnadjustStringForLocaleDirection(&display_name);
    DCHECK(markup_removal);
    bool has_rtl_chars = base::i18n::StringContainsStrongRTLChars(display_name);
    std::string directionality = has_rtl_chars ? "rtl" : "ltr";

    const LanguagePair& pair = language_map[display_names[i]];
    DictionaryValue* dictionary = new DictionaryValue();
    dictionary->SetString("code",  pair.first);
    dictionary->SetString("displayName", display_names[i]);
    dictionary->SetString("textDirection", directionality);
    dictionary->SetString("nativeDisplayName", pair.second);
    language_list->Append(dictionary);
  }

  return language_list;
}

string16 CrosLanguageOptionsHandler::GetProductName() {
  return l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME);
}

void CrosLanguageOptionsHandler::SetApplicationLocale(
    const std::string& language_code) {
  Profile::FromWebUI(web_ui())->ChangeAppLocale(
      language_code, Profile::APP_LOCALE_CHANGED_VIA_SETTINGS);
}

void CrosLanguageOptionsHandler::RestartCallback(const ListValue* args) {
  content::RecordAction(UserMetricsAction("LanguageOptions_SignOut"));

  Browser* browser = browser::FindBrowserForController(
      &web_ui()->GetWebContents()->GetController(), NULL);
  if (browser)
    browser->ExecuteCommand(IDC_EXIT);
}

void CrosLanguageOptionsHandler::InputMethodDisableCallback(
    const ListValue* args) {
  const std::string input_method_id = UTF16ToASCII(ExtractStringValue(args));
  const std::string action = base::StringPrintf(
      "LanguageOptions_DisableInputMethod_%s", input_method_id.c_str());
  content::RecordComputedAction(action);
}

void CrosLanguageOptionsHandler::InputMethodEnableCallback(
    const ListValue* args) {
  const std::string input_method_id = UTF16ToASCII(ExtractStringValue(args));
  const std::string action = base::StringPrintf(
      "LanguageOptions_EnableInputMethod_%s", input_method_id.c_str());
  content::RecordComputedAction(action);
}

void CrosLanguageOptionsHandler::InputMethodOptionsOpenCallback(
    const ListValue* args) {
  const std::string input_method_id = UTF16ToASCII(ExtractStringValue(args));
  const std::string action = base::StringPrintf(
      "InputMethodOptions_Open_%s", input_method_id.c_str());
  content::RecordComputedAction(action);
}

}  // namespace options2
}  // namespace chromeos
