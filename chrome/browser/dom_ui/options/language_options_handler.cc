// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/language_options_handler.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/input_method_library.h"
#endif  // defined(OS_CHROMEOS)

LanguageOptionsHandler::LanguageOptionsHandler() {
}

LanguageOptionsHandler::~LanguageOptionsHandler() {
}

void LanguageOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  string16 product_name;
#if defined(OS_CHROMEOS)
  product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_OS_NAME);
#else
  product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
#endif  // defined(OS_CHROMEOS)
  localized_strings->SetString("languagePage",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_DIALOG_TITLE));
  localized_strings->SetString("add_button",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_BUTTON));
  localized_strings->SetString("languages",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_LANGUAGES));
  localized_strings->SetString("please_add_another_language",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_PLEASE_ADD_ANOTHER_LANGUAGE));
  localized_strings->SetString("remove_button",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_LANGUAGES_REMOVE_BUTTON));
  localized_strings->SetString("add_language_instructions",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_LANGUAGE_INSTRUCTIONS));
  localized_strings->SetString("cannot_be_displayed_in_this_language",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_CANNOT_BE_DISPLAYED_IN_THIS_LANGUAGE,
          product_name));
  localized_strings->SetString("is_displayed_in_this_language",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
          product_name));
  localized_strings->SetString("display_in_this_language",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
          product_name));
  localized_strings->SetString("this_language_is_currently_in_use",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_THIS_LANGUAGE_IS_CURRENTLY_IN_USE,
          product_name));
  localized_strings->SetString("use_this_for_spell_checking",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_USE_THIS_FOR_SPELL_CHECKING));
  localized_strings->SetString("cannot_be_used_for_spell_checking",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_CANNOT_BE_USED_FOR_SPELL_CHECKING));
  localized_strings->SetString("is_used_for_spell_checking",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_IS_USED_FOR_SPELL_CHECKING));
  localized_strings->SetString("restart_required",
          l10n_util::GetStringUTF16(IDS_OPTIONS_RESTART_REQUIRED));
  localized_strings->SetString("enable_spell_check",
          l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_SPELLCHECK));
  localized_strings->SetString("enable_auto_spell_correction",
          l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_AUTO_SPELL_CORRECTION));
  localized_strings->SetString("add_language_title",
          l10n_util::GetStringUTF16(IDS_OPTIONS_LANGUAGES_ADD_TITLE));
  localized_strings->SetString("add_language_select_label",
          l10n_util::GetStringUTF16(IDS_OPTIONS_LANGUAGES_ADD_SELECT_LABEL));

#if defined(OS_CHROMEOS)
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
#else
  localized_strings->SetString("restart_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_RESTART_BUTTON));
#endif  // defined(OS_CHROMEOS)

  // The followigns are resources, rather than local strings.
  localized_strings->SetString("currentUiLanguageCode",
                               g_browser_process->GetApplicationLocale());
  localized_strings->Set("spellCheckLanguageCodeSet",
                         GetSpellCheckLanguageCodeSet());
  localized_strings->Set("uiLanguageCodeSet", GetUILanguageCodeSet());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool experimental_spell_check_features =
      command_line.HasSwitch(switches::kExperimentalSpellcheckerFeatures);
  localized_strings->SetString("experimentalSpellCheckFeatures",
      experimental_spell_check_features ? "true" : "false");

#if defined(OS_CHROMEOS)
  // GetSupportedInputMethods() never return NULL.
  chromeos::InputMethodLibrary *im_library =
      chromeos::CrosLibrary::Get()->GetInputMethodLibrary();
  scoped_ptr<chromeos::InputMethodDescriptors> descriptors(
      im_library->GetSupportedInputMethods());
  localized_strings->Set("languageList", GetLanguageList(*descriptors));
  localized_strings->Set("inputMethodList", GetInputMethodList(*descriptors));
#else
  localized_strings->Set("languageList", GetLanguageList());
#endif  // defined(OS_CHROMEOS)
}

void LanguageOptionsHandler::RegisterMessages() {
  DCHECK(dom_ui_);
#if defined(OS_CHROMEOS)
  dom_ui_->RegisterMessageCallback("inputMethodDisable",
      NewCallback(this, &LanguageOptionsHandler::InputMethodDisableCallback));
  dom_ui_->RegisterMessageCallback("inputMethodEnable",
      NewCallback(this, &LanguageOptionsHandler::InputMethodEnableCallback));
  dom_ui_->RegisterMessageCallback("inputMethodOptionsOpen",
      NewCallback(this,
                  &LanguageOptionsHandler::InputMethodOptionsOpenCallback));
#endif  // defined(OS_CHROMEOS)
  dom_ui_->RegisterMessageCallback("languageOptionsOpen",
      NewCallback(this, &LanguageOptionsHandler::LanguageOptionsOpenCallback));
  dom_ui_->RegisterMessageCallback("spellCheckLanguageChange",
      NewCallback(this,
                  &LanguageOptionsHandler::SpellCheckLanguageChangeCallback));
  dom_ui_->RegisterMessageCallback("uiLanguageChange",
      NewCallback(this, &LanguageOptionsHandler::UiLanguageChangeCallback));
  dom_ui_->RegisterMessageCallback("uiLanguageRestart",
      NewCallback(this, &LanguageOptionsHandler::RestartCallback));
}

#if defined(OS_CHROMEOS)
ListValue* LanguageOptionsHandler::GetInputMethodList(
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

ListValue* LanguageOptionsHandler::GetLanguageList(
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
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)
ListValue* LanguageOptionsHandler::GetLanguageList() {
  // Collect the language codes from the supported accept-languages.
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  std::vector<std::string> language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &language_codes);

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
  for (size_t i = 0; i < language_codes.size(); ++i) {
    const string16 display_name =
        l10n_util::GetDisplayNameForLocale(language_codes[i], app_locale, true);
    const string16 native_display_name =
        l10n_util::GetDisplayNameForLocale(language_codes[i], language_codes[i],
                                           true);
    display_names.push_back(display_name);
    language_map[display_name] =
        std::make_pair(language_codes[i], native_display_name);
  }
  DCHECK_EQ(display_names.size(), language_map.size());

  // Sort display names using locale specific sorter.
  l10n_util::SortStrings16(app_locale, &display_names);

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
#endif  // !defined(OS_CHROMEOS)

DictionaryValue* LanguageOptionsHandler::GetUILanguageCodeSet() {
  DictionaryValue* dictionary = new DictionaryValue();
  const std::vector<std::string>& available_locales =
      l10n_util::GetAvailableLocales();
  for (size_t i = 0; i < available_locales.size(); ++i) {
    dictionary->SetBoolean(available_locales[i], true);
  }
  return dictionary;
}

DictionaryValue* LanguageOptionsHandler::GetSpellCheckLanguageCodeSet() {
  DictionaryValue* dictionary = new DictionaryValue();
  std::vector<std::string> spell_check_languages;
  SpellCheckCommon::SpellCheckLanguages(&spell_check_languages);
  for (size_t i = 0; i < spell_check_languages.size(); ++i) {
    dictionary->SetBoolean(spell_check_languages[i], true);
  }
  return dictionary;
}

#if defined(OS_CHROMEOS)
void LanguageOptionsHandler::InputMethodDisableCallback(
    const ListValue* args) {
  const std::string input_method_id = WideToASCII(ExtractStringValue(args));
  const std::string action = StringPrintf(
      "LanguageOptions_DisableInputMethod_%s", input_method_id.c_str());
  UserMetrics::RecordComputedAction(action);
}

void LanguageOptionsHandler::InputMethodEnableCallback(
    const ListValue* args) {
  const std::string input_method_id = WideToASCII(ExtractStringValue(args));
  const std::string action = StringPrintf(
      "LanguageOptions_EnableInputMethod_%s", input_method_id.c_str());
  UserMetrics::RecordComputedAction(action);
}

void LanguageOptionsHandler::InputMethodOptionsOpenCallback(
    const ListValue* args) {
  const std::string input_method_id = WideToASCII(ExtractStringValue(args));
  const std::string action = StringPrintf(
      "InputMethodOptions_Open_%s", input_method_id.c_str());
  UserMetrics::RecordComputedAction(action);
}
#endif  // defined(OS_CHROMEOS)

void LanguageOptionsHandler::LanguageOptionsOpenCallback(
    const ListValue* args) {
  UserMetrics::RecordAction(UserMetricsAction("LanguageOptions_Open"));
}

void LanguageOptionsHandler::UiLanguageChangeCallback(
    const ListValue* args) {
  const std::string language_code = WideToASCII(ExtractStringValue(args));
  CHECK(!language_code.empty());
  const std::string action = StringPrintf(
      "LanguageOptions_UiLanguageChange_%s", language_code.c_str());
  UserMetrics::RecordComputedAction(action);
#if defined(OS_CHROMEOS)
  dom_ui_->GetProfile()->ChangeApplicationLocale(language_code, false);
#else
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetString(prefs::kApplicationLocale, language_code);
#endif  // defined(OS_CHROMEOS)
  dom_ui_->CallJavascriptFunction(
      L"options.LanguageOptions.uiLanguageSaved");
}

void LanguageOptionsHandler::SpellCheckLanguageChangeCallback(
    const ListValue* args) {
  const std::string language_code = WideToASCII(ExtractStringValue(args));
  CHECK(!language_code.empty());
  const std::string action = StringPrintf(
      "LanguageOptions_SpellCheckLanguageChange_%s", language_code.c_str());
  UserMetrics::RecordComputedAction(action);
}

void LanguageOptionsHandler::RestartCallback(const ListValue* args) {
#if defined(OS_CHROMEOS)
  UserMetrics::RecordAction(UserMetricsAction("LanguageOptions_SignOut"));

  Browser* browser = Browser::GetBrowserForController(
      &dom_ui_->tab_contents()->controller(), NULL);
  if (browser)
    browser->ExecuteCommand(IDC_EXIT);
#else
  UserMetrics::RecordAction(UserMetricsAction("LanguageOptions_Restart"));

  // Set the flag to restore state after the restart.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  BrowserList::CloseAllBrowsersAndExit();
#endif  // defined(OS_CHROMEOS)
}

