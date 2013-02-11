// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/language_options_handler_common.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::UserMetricsAction;

namespace options {

LanguageOptionsHandlerCommon::LanguageOptionsHandlerCommon() {
}

LanguageOptionsHandlerCommon::~LanguageOptionsHandlerCommon() {
}

void LanguageOptionsHandlerCommon::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  static OptionsStringResource resources[] = {
    { "add_button", IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_BUTTON },
    { "languages", IDS_OPTIONS_SETTINGS_LANGUAGES_LANGUAGES },
    { "add_language_instructions",
      IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_LANGUAGE_INSTRUCTIONS },
    { "cannot_be_displayed_in_this_language",
      IDS_OPTIONS_SETTINGS_LANGUAGES_CANNOT_BE_DISPLAYED_IN_THIS_LANGUAGE,
      IDS_PRODUCT_NAME },
    { "is_displayed_in_this_language",
      IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
      IDS_PRODUCT_NAME },
    { "display_in_this_language",
      IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
      IDS_PRODUCT_NAME },
    { "restart_required", IDS_OPTIONS_RELAUNCH_REQUIRED },
  // OS X uses the OS native spellchecker so no need for these strings.
#if !defined(OS_MACOSX)
    { "use_this_for_spell_checking",
      IDS_OPTIONS_SETTINGS_USE_THIS_FOR_SPELL_CHECKING },
    { "cannot_be_used_for_spell_checking",
      IDS_OPTIONS_SETTINGS_CANNOT_BE_USED_FOR_SPELL_CHECKING },
    { "is_used_for_spell_checking",
      IDS_OPTIONS_SETTINGS_IS_USED_FOR_SPELL_CHECKING },
    { "enable_spell_check", IDS_OPTIONS_ENABLE_SPELLCHECK },
    { "enable_auto_spell_correction",
      IDS_OPTIONS_ENABLE_AUTO_SPELL_CORRECTION },
    { "downloading_dictionary", IDS_OPTIONS_DICTIONARY_DOWNLOADING },
    { "download_failed", IDS_OPTIONS_DICTIONARY_DOWNLOAD_FAILED },
    { "retry_button", IDS_OPTIONS_DICTIONARY_DOWNLOAD_RETRY },
    { "download_fail_help", IDS_OPTIONS_DICTIONARY_DOWNLOAD_FAIL_HELP },
#endif  // !OS_MACOSX
    { "add_language_title", IDS_OPTIONS_LANGUAGES_ADD_TITLE },
    { "add_language_select_label", IDS_OPTIONS_LANGUAGES_ADD_SELECT_LABEL },
    { "restart_button", IDS_OPTIONS_SETTINGS_LANGUAGES_RELAUNCH_BUTTON }
  };

#if defined(ENABLE_SETTINGS_APP)
  static OptionsStringResource app_resources[] = {
    { "cannot_be_displayed_in_this_language",
      IDS_OPTIONS_SETTINGS_LANGUAGES_CANNOT_BE_DISPLAYED_IN_THIS_LANGUAGE,
      IDS_SETTINGS_APP_LAUNCHER_PRODUCT_NAME },
    { "is_displayed_in_this_language",
      IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
      IDS_SETTINGS_APP_LAUNCHER_PRODUCT_NAME },
    { "display_in_this_language",
      IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
      IDS_SETTINGS_APP_LAUNCHER_PRODUCT_NAME },
  };
  DictionaryValue* app_values = NULL;
  CHECK(localized_strings->GetDictionary(kSettingsAppKey, &app_values));
  RegisterStrings(app_values, app_resources, arraysize(app_resources));
#endif

  RegisterStrings(localized_strings, resources, arraysize(resources));

  // The following are resources, rather than local strings.
  std::string application_locale = g_browser_process->GetApplicationLocale();
  localized_strings->SetString("currentUiLanguageCode", application_locale);
  std::string prospective_locale =
      g_browser_process->local_state()->GetString(prefs::kApplicationLocale);
  localized_strings->SetString("prospectiveUiLanguageCode",
      !prospective_locale.empty() ? prospective_locale : application_locale);
  localized_strings->Set("spellCheckLanguageCodeSet",
                         GetSpellCheckLanguageCodeSet());
  localized_strings->Set("uiLanguageCodeSet", GetUILanguageCodeSet());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool enable_spelling_auto_correct =
      command_line.HasSwitch(switches::kEnableSpellingAutoCorrect);
  localized_strings->SetBoolean("enableSpellingAutoCorrect",
      enable_spelling_auto_correct);
}

void LanguageOptionsHandlerCommon::Uninitialize() {
  if (hunspell_dictionary_.get())
    hunspell_dictionary_->RemoveObserver(this);
  hunspell_dictionary_.reset();
}

void LanguageOptionsHandlerCommon::RegisterMessages() {
  web_ui()->RegisterMessageCallback("languageOptionsOpen",
      base::Bind(
          &LanguageOptionsHandlerCommon::LanguageOptionsOpenCallback,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback("spellCheckLanguageChange",
      base::Bind(
          &LanguageOptionsHandlerCommon::SpellCheckLanguageChangeCallback,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback("uiLanguageChange",
      base::Bind(
          &LanguageOptionsHandlerCommon::UiLanguageChangeCallback,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback("retryDictionaryDownload",
      base::Bind(
          &LanguageOptionsHandlerCommon::RetrySpellcheckDictionaryDownload,
          base::Unretained(this)));
}

void LanguageOptionsHandlerCommon::OnHunspellDictionaryInitialized() {
}

void LanguageOptionsHandlerCommon::OnHunspellDictionaryDownloadBegin() {
  web_ui()->CallJavascriptFunction(
      "options.LanguageOptions.onDictionaryDownloadBegin",
      StringValue(GetHunspellDictionary()->GetLanguage()));
}

void LanguageOptionsHandlerCommon::OnHunspellDictionaryDownloadSuccess() {
  web_ui()->CallJavascriptFunction(
      "options.LanguageOptions.onDictionaryDownloadSuccess",
      StringValue(GetHunspellDictionary()->GetLanguage()));
}

void LanguageOptionsHandlerCommon::OnHunspellDictionaryDownloadFailure() {
  web_ui()->CallJavascriptFunction(
      "options.LanguageOptions.onDictionaryDownloadFailure",
      StringValue(GetHunspellDictionary()->GetLanguage()));
}

DictionaryValue* LanguageOptionsHandlerCommon::GetUILanguageCodeSet() {
  DictionaryValue* dictionary = new DictionaryValue();
  const std::vector<std::string>& available_locales =
      l10n_util::GetAvailableLocales();
  for (size_t i = 0; i < available_locales.size(); ++i)
    dictionary->SetBoolean(available_locales[i], true);
  return dictionary;
}

DictionaryValue* LanguageOptionsHandlerCommon::GetSpellCheckLanguageCodeSet() {
  DictionaryValue* dictionary = new DictionaryValue();
  std::vector<std::string> spell_check_languages;
  chrome::spellcheck_common::SpellCheckLanguages(&spell_check_languages);
  for (size_t i = 0; i < spell_check_languages.size(); ++i) {
    dictionary->SetBoolean(spell_check_languages[i], true);
  }
  return dictionary;
}

void LanguageOptionsHandlerCommon::LanguageOptionsOpenCallback(
    const ListValue* args) {
  content::RecordAction(UserMetricsAction("LanguageOptions_Open"));
  RefreshHunspellDictionary();
  if (hunspell_dictionary_->IsDownloadInProgress())
    OnHunspellDictionaryDownloadBegin();
  else if (hunspell_dictionary_->IsDownloadFailure())
    OnHunspellDictionaryDownloadFailure();
  else
    OnHunspellDictionaryDownloadSuccess();
}

void LanguageOptionsHandlerCommon::UiLanguageChangeCallback(
    const ListValue* args) {
  const std::string language_code = UTF16ToASCII(ExtractStringValue(args));
  CHECK(!language_code.empty());
  const std::string action = base::StringPrintf(
      "LanguageOptions_UiLanguageChange_%s", language_code.c_str());
  content::RecordComputedAction(action);
  SetApplicationLocale(language_code);
  StringValue language_value(language_code);
  web_ui()->CallJavascriptFunction("options.LanguageOptions.uiLanguageSaved",
                                   language_value);
}

void LanguageOptionsHandlerCommon::SpellCheckLanguageChangeCallback(
    const ListValue* args) {
  const std::string language_code = UTF16ToASCII(ExtractStringValue(args));
  CHECK(!language_code.empty());
  const std::string action = base::StringPrintf(
      "LanguageOptions_SpellCheckLanguageChange_%s", language_code.c_str());
  content::RecordComputedAction(action);
  RefreshHunspellDictionary();
}

void LanguageOptionsHandlerCommon::RetrySpellcheckDictionaryDownload(
    const ListValue* args) {
  GetHunspellDictionary()->RetryDownloadDictionary(
      Profile::FromWebUI(web_ui())->GetRequestContext());
}

void LanguageOptionsHandlerCommon::RefreshHunspellDictionary() {
  if (hunspell_dictionary_.get())
    hunspell_dictionary_->RemoveObserver(this);
  hunspell_dictionary_.reset();
  hunspell_dictionary_ = SpellcheckServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()))->GetHunspellDictionary()->AsWeakPtr();
  hunspell_dictionary_->AddObserver(this);
}

base::WeakPtr<SpellcheckHunspellDictionary>&
    LanguageOptionsHandlerCommon::GetHunspellDictionary() {
  if (!hunspell_dictionary_.get())
    RefreshHunspellDictionary();
  return hunspell_dictionary_;
}

}  // namespace options
