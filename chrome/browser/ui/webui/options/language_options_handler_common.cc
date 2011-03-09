// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/language_options_handler_common.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

LanguageOptionsHandlerCommon::LanguageOptionsHandlerCommon() {
}

LanguageOptionsHandlerCommon::~LanguageOptionsHandlerCommon() {
}

void LanguageOptionsHandlerCommon::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  string16 product_name = GetProductName();
  RegisterTitle(localized_strings, "languagePage",
                IDS_OPTIONS_SETTINGS_LANGUAGES_DIALOG_TITLE);
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
          l10n_util::GetStringUTF16(IDS_OPTIONS_RELAUNCH_REQUIRED));
  localized_strings->SetString("enable_spell_check",
          l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_SPELLCHECK));
  localized_strings->SetString("enable_auto_spell_correction",
          l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_AUTO_SPELL_CORRECTION));
  localized_strings->SetString("add_language_title",
          l10n_util::GetStringUTF16(IDS_OPTIONS_LANGUAGES_ADD_TITLE));
  localized_strings->SetString("add_language_select_label",
          l10n_util::GetStringUTF16(IDS_OPTIONS_LANGUAGES_ADD_SELECT_LABEL));
  localized_strings->SetString("restart_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_RELAUNCH_BUTTON));

  // The following are resources, rather than local strings.
  localized_strings->SetString("currentUiLanguageCode",
                               g_browser_process->GetApplicationLocale());
  localized_strings->Set("spellCheckLanguageCodeSet",
                         GetSpellCheckLanguageCodeSet());
  localized_strings->Set("uiLanguageCodeSet", GetUILanguageCodeSet());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool experimental_spell_check_features =
      command_line.HasSwitch(switches::kExperimentalSpellcheckerFeatures);
  localized_strings->SetBoolean("experimentalSpellCheckFeatures",
                                experimental_spell_check_features);
}

void LanguageOptionsHandlerCommon::RegisterMessages() {
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("languageOptionsOpen",
      NewCallback(
          this,
          &LanguageOptionsHandlerCommon::LanguageOptionsOpenCallback));
  web_ui_->RegisterMessageCallback("spellCheckLanguageChange",
      NewCallback(
          this,
          &LanguageOptionsHandlerCommon::SpellCheckLanguageChangeCallback));
  web_ui_->RegisterMessageCallback("uiLanguageChange",
      NewCallback(
          this,
          &LanguageOptionsHandlerCommon::UiLanguageChangeCallback));
}

DictionaryValue* LanguageOptionsHandlerCommon::GetUILanguageCodeSet() {
  DictionaryValue* dictionary = new DictionaryValue();
  const std::vector<std::string>& available_locales =
      l10n_util::GetAvailableLocales();
  for (size_t i = 0; i < available_locales.size(); ++i) {
    dictionary->SetBoolean(available_locales[i], true);
  }
  return dictionary;
}

DictionaryValue* LanguageOptionsHandlerCommon::GetSpellCheckLanguageCodeSet() {
  DictionaryValue* dictionary = new DictionaryValue();
  std::vector<std::string> spell_check_languages;
  SpellCheckCommon::SpellCheckLanguages(&spell_check_languages);
  for (size_t i = 0; i < spell_check_languages.size(); ++i) {
    dictionary->SetBoolean(spell_check_languages[i], true);
  }
  return dictionary;
}

void LanguageOptionsHandlerCommon::LanguageOptionsOpenCallback(
    const ListValue* args) {
  UserMetrics::RecordAction(UserMetricsAction("LanguageOptions_Open"));
}

void LanguageOptionsHandlerCommon::UiLanguageChangeCallback(
    const ListValue* args) {
  const std::string language_code = UTF16ToASCII(ExtractStringValue(args));
  CHECK(!language_code.empty());
  const std::string action = StringPrintf(
      "LanguageOptions_UiLanguageChange_%s", language_code.c_str());
  UserMetrics::RecordComputedAction(action);
  SetApplicationLocale(language_code);
    web_ui_->CallJavascriptFunction("options.LanguageOptions.uiLanguageSaved");
}

void LanguageOptionsHandlerCommon::SpellCheckLanguageChangeCallback(
    const ListValue* args) {
  const std::string language_code = UTF16ToASCII(ExtractStringValue(args));
  CHECK(!language_code.empty());
  const std::string action = StringPrintf(
      "LanguageOptions_SpellCheckLanguageChange_%s", language_code.c_str());
  UserMetrics::RecordComputedAction(action);
}
