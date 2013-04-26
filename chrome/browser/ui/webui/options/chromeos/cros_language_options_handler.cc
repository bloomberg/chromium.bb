// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"

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
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/input_method_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::UserMetricsAction;

namespace chromeos {
namespace options {

CrosLanguageOptionsHandler::CrosLanguageOptionsHandler()
    : composition_extension_appended_(false),
      is_page_initialized_(false) {
  input_method::GetInputMethodManager()->GetComponentExtensionIMEManager()->
      AddObserver(this);
}

CrosLanguageOptionsHandler::~CrosLanguageOptionsHandler() {
  input_method::GetInputMethodManager()->GetComponentExtensionIMEManager()->
      RemoveObserver(this);
}

void CrosLanguageOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  ::options::LanguageOptionsHandlerCommon::GetLocalizedValues(
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
  localized_strings->SetString("extension_ime_label",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD_EXTENSION_IME));
  localized_strings->SetString("extension_ime_description",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD_EXTENSION_DESCRIPTION));
  localized_strings->SetString("noInputMethods",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_NO_INPUT_METHODS));

  input_method::InputMethodManager* manager =
      input_method::GetInputMethodManager();
  // GetSupportedInputMethods() never return NULL.
  scoped_ptr<input_method::InputMethodDescriptors> descriptors(
      manager->GetSupportedInputMethods());
  localized_strings->Set("languageList", GetAcceptLanguageList(*descriptors));
  localized_strings->Set("inputMethodList", GetInputMethodList(*descriptors));
  localized_strings->Set("extensionImeList", GetExtensionImeList());

  ComponentExtensionIMEManager* component_extension_manager =
      input_method::GetInputMethodManager()->GetComponentExtensionIMEManager();
  if (component_extension_manager->IsInitialized()) {
    localized_strings->Set("componentExtensionImeList",
                           GetComponentExtensionImeList());
    composition_extension_appended_ = true;
  } else {
    // If component extension IME manager is not ready for use, it will be
    // added in |InitializePage()|.
    localized_strings->Set("componentExtensionImeList",
                           new ListValue());
  }
}

void CrosLanguageOptionsHandler::RegisterMessages() {
  ::options::LanguageOptionsHandlerCommon::RegisterMessages();

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
      input_method::GetInputMethodManager();

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
    // Check extra languages to see if there are languages associated with
    // this input method. If these are present, add these.
    const std::vector<std::string> extra_language_codes =
        manager->GetInputMethodUtil()->GetExtraLanguageCodesFromId(
            descriptor.id());
    for (size_t j = 0; j < extra_language_codes.size(); ++j)
      language_codes->SetBoolean(extra_language_codes[j], true);
    dictionary->Set("languageCodeSet", language_codes);

    input_method_list->Append(dictionary);
  }

  return input_method_list;
}

// static
ListValue* CrosLanguageOptionsHandler::GetLanguageListInternal(
    const input_method::InputMethodDescriptors& descriptors,
    const std::vector<std::string>& base_language_codes) {
  const std::string app_locale = g_browser_process->GetApplicationLocale();

  std::set<std::string> language_codes;
  // Collect the language codes from the supported input methods.
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor = descriptors[i];
    const std::string language_code = descriptor.language_code();
    language_codes.insert(language_code);
  }
  // Collect the language codes from extra languages.
  const std::vector<std::string> extra_language_codes =
      input_method::GetInputMethodManager()->GetInputMethodUtil()
          ->GetExtraLanguageCodeList();
  for (size_t i = 0; i < extra_language_codes.size(); ++i)
    language_codes.insert(extra_language_codes[i]);

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
     // Exclude the language which is not in |base_langauge_codes| even it has
     // input methods.
    if (std::find(base_language_codes.begin(),
                  base_language_codes.end(),
                  *iter) == base_language_codes.end()) {
      continue;
    }
    const string16 display_name =
        l10n_util::GetDisplayNameForLocale(*iter, app_locale, true);
    const string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(*iter, *iter, true);

    display_names.push_back(display_name);
    language_map[display_name] =
        std::make_pair(*iter, native_display_name);
  }
  DCHECK_EQ(display_names.size(), language_map.size());

  // Build the list of display names, and build the language map.
  for (size_t i = 0; i < base_language_codes.size(); ++i) {
    // Skip this language if it was already added.
    if (language_codes.find(base_language_codes[i]) != language_codes.end())
      continue;
    string16 display_name =
        l10n_util::GetDisplayNameForLocale(
            base_language_codes[i], app_locale, false);
    string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(
            base_language_codes[i], base_language_codes[i], false);
    display_names.push_back(display_name);
    language_map[display_name] =
        std::make_pair(base_language_codes[i], native_display_name);
  }

  // Sort display names using locale specific sorter.
  l10n_util::SortStrings16(app_locale, &display_names);

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
    dictionary->SetString("code", pair.first);
    dictionary->SetString("displayName", display_names[i]);
    dictionary->SetString("textDirection", directionality);
    dictionary->SetString("nativeDisplayName", pair.second);
    language_list->Append(dictionary);
  }

  return language_list;
}

// static
base::ListValue* CrosLanguageOptionsHandler::GetAcceptLanguageList(
    const input_method::InputMethodDescriptors& descriptors) {
  // Collect the language codes from the supported accept-languages.
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> accept_language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &accept_language_codes);
  return GetLanguageListInternal(descriptors, accept_language_codes);
}

// static
base::ListValue* CrosLanguageOptionsHandler::GetUILanguageList(
    const input_method::InputMethodDescriptors& descriptors) {
  // Collect the language codes from the available locales.
  return GetLanguageListInternal(descriptors, l10n_util::GetAvailableLocales());
}

base::ListValue* CrosLanguageOptionsHandler::GetExtensionImeList() {
  input_method::InputMethodManager* manager =
      input_method::GetInputMethodManager();

  input_method::InputMethodDescriptors descriptors;
  manager->GetInputMethodExtensions(&descriptors);

  ListValue* extension_ime_ids_list = new ListValue();

  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor = descriptors[i];
    DictionaryValue* dictionary = new DictionaryValue();
    dictionary->SetString("id", descriptor.id());
    dictionary->SetString("displayName", descriptor.name());
    extension_ime_ids_list->Append(dictionary);
  }

  return extension_ime_ids_list;
}

base::ListValue* CrosLanguageOptionsHandler::GetComponentExtensionImeList() {
  ComponentExtensionIMEManager* component_extension_manager =
      input_method::GetInputMethodManager()->GetComponentExtensionIMEManager();
  DCHECK(component_extension_manager->IsInitialized());

  scoped_ptr<ListValue> extension_ime_ids_list(new ListValue());
  input_method::InputMethodDescriptors descriptors =
      component_extension_manager->GetAllIMEAsInputMethodDescriptor();
  for (size_t i = 0; i < descriptors.size(); ++i) {
    const input_method::InputMethodDescriptor& descriptor = descriptors[i];
    scoped_ptr<DictionaryValue> dictionary(new DictionaryValue());
    dictionary->SetString("id", descriptor.id());
    dictionary->SetString("displayName", descriptor.name());
    dictionary->SetString("optionsPage", descriptor.options_page_url());

    scoped_ptr<DictionaryValue> language_codes(new DictionaryValue());
    language_codes->SetBoolean(descriptor.language_code(), true);
    dictionary->Set("languageCodeSet", language_codes.release());
    extension_ime_ids_list->Append(dictionary.release());
  }
  return extension_ime_ids_list.release();
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
  chrome::AttemptUserExit();
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

void CrosLanguageOptionsHandler::OnInitialized() {
  if (composition_extension_appended_ || !is_page_initialized_) {
    // If an option page is not ready to call JavaScript, appending component
    // extension IMEs will be done in InitializePage function later.
    return;
  }

  DCHECK(input_method::GetInputMethodManager()->
         GetComponentExtensionIMEManager()->IsInitialized());
  scoped_ptr<ListValue> ime_list(GetComponentExtensionImeList());
  web_ui()->CallJavascriptFunction(
      "options.LanguageOptions.onComponentManagerInitialized",
      *ime_list);
  composition_extension_appended_ = true;
}

void CrosLanguageOptionsHandler::InitializePage() {
  is_page_initialized_ = true;
  if (composition_extension_appended_)
    return;

  ComponentExtensionIMEManager* component_extension_manager =
      input_method::GetInputMethodManager()->GetComponentExtensionIMEManager();
  if (!component_extension_manager->IsInitialized()) {
    // If the component extension IME manager is not available yet, append the
    // component extension list in |OnInitialized()|.
    return;
  }

  scoped_ptr<ListValue> ime_list(GetComponentExtensionImeList());
  web_ui()->CallJavascriptFunction(
      "options.LanguageOptions.onComponentManagerInitialized",
      *ime_list);
  composition_extension_appended_ = true;
}

}  // namespace options
}  // namespace chromeos
