// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/language_options_handler_common.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace options {

LanguageOptionsHandlerCommon::LanguageOptionsHandlerCommon() {
}

LanguageOptionsHandlerCommon::~LanguageOptionsHandlerCommon() {
}

void LanguageOptionsHandlerCommon::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

#if defined(OS_CHROMEOS)
  const int product_id = IDS_PRODUCT_OS_NAME;
#else
  const int product_id = IDS_PRODUCT_NAME;
#endif

  static OptionsStringResource resources[] = {
    { "addButton", IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_BUTTON },
    { "languages", IDS_OPTIONS_SETTINGS_LANGUAGES_LANGUAGES },
    { "addLanguageInstructions",
      IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_LANGUAGE_INSTRUCTIONS },
    { "cannotBeDisplayedInThisLanguage",
      IDS_OPTIONS_SETTINGS_LANGUAGES_CANNOT_BE_DISPLAYED_IN_THIS_LANGUAGE,
      product_id },
    { "isDisplayedInThisLanguage",
      IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
      product_id },
    { "displayInThisLanguage",
      IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
      product_id },
    { "restartRequired", IDS_OPTIONS_RELAUNCH_REQUIRED },
  // OS X uses the OS native spellchecker so no need for these strings.
#if !defined(OS_MACOSX)
    { "useThisForSpellChecking",
      IDS_OPTIONS_SETTINGS_USE_THIS_FOR_SPELL_CHECKING },
    { "cannotBeUsedForSpellChecking",
      IDS_OPTIONS_SETTINGS_CANNOT_BE_USED_FOR_SPELL_CHECKING },
    { "isUsedForSpellChecking",
      IDS_OPTIONS_SETTINGS_IS_USED_FOR_SPELL_CHECKING },
    { "enableSpellCheck", IDS_OPTIONS_ENABLE_SPELLCHECK },
    { "downloadingDictionary", IDS_OPTIONS_DICTIONARY_DOWNLOADING },
    { "downloadFailed", IDS_OPTIONS_DICTIONARY_DOWNLOAD_FAILED },
    { "retryButton", IDS_OPTIONS_DICTIONARY_DOWNLOAD_RETRY },
    { "downloadFailHelp", IDS_OPTIONS_DICTIONARY_DOWNLOAD_FAIL_HELP },
#endif  // !OS_MACOSX
    { "addLanguageTitle", IDS_OPTIONS_LANGUAGES_ADD_TITLE },
    { "addLanguageSelectLabel", IDS_OPTIONS_LANGUAGES_ADD_SELECT_LABEL },
    { "restartButton", IDS_OPTIONS_SETTINGS_LANGUAGES_RELAUNCH_BUTTON },
    { "offerToTranslateInThisLanguage",
      IDS_OPTIONS_LANGUAGES_OFFER_TO_TRANSLATE_IN_THIS_LANGUAGE },
    { "cannotTranslateInThisLanguage",
      IDS_OPTIONS_LANGUAGES_CANNOT_TRANSLATE_IN_THIS_LANGUAGE },
  };

#if defined(ENABLE_SETTINGS_APP)
  static OptionsStringResource app_resources[] = {
    { "cannotBeDisplayedInThisLanguage",
      IDS_OPTIONS_SETTINGS_LANGUAGES_CANNOT_BE_DISPLAYED_IN_THIS_LANGUAGE,
      IDS_SETTINGS_APP_LAUNCHER_PRODUCT_NAME },
    { "isDisplayedInThisLanguage",
      IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
      IDS_SETTINGS_APP_LAUNCHER_PRODUCT_NAME },
    { "displayInThisLanguage",
      IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
      IDS_SETTINGS_APP_LAUNCHER_PRODUCT_NAME },
  };
  base::DictionaryValue* app_values = NULL;
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

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  std::string default_target_language =
      TranslateService::GetTargetLanguage(prefs);
  localized_strings->SetString("defaultTargetLanguage",
                               default_target_language);

  std::vector<std::string> languages;
  translate::TranslateDownloadManager::GetSupportedLanguages(&languages);

  base::ListValue* languages_list = new base::ListValue();
  for (std::vector<std::string>::iterator it = languages.begin();
       it != languages.end(); ++it) {
    languages_list->Append(new base::StringValue(*it));
  }

  localized_strings->Set("translateSupportedLanguages", languages_list);
}

void LanguageOptionsHandlerCommon::Uninitialize() {
  SpellcheckService* service = GetSpellcheckService();
  if (!service)
    return;

  for (auto dict : service->GetHunspellDictionaries())
    dict->RemoveObserver(this);
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
  web_ui()->RegisterMessageCallback("updateLanguageList",
      base::Bind(
          &LanguageOptionsHandlerCommon::UpdateLanguageListCallback,
          base::Unretained(this)));
}

void LanguageOptionsHandlerCommon::OnHunspellDictionaryInitialized(
    const std::string& language) {
}

void LanguageOptionsHandlerCommon::OnHunspellDictionaryDownloadBegin(
    const std::string& language) {
  web_ui()->CallJavascriptFunction(
      "options.LanguageOptions.onDictionaryDownloadBegin",
      base::StringValue(language));
}

void LanguageOptionsHandlerCommon::OnHunspellDictionaryDownloadSuccess(
    const std::string& language) {
  web_ui()->CallJavascriptFunction(
      "options.LanguageOptions.onDictionaryDownloadSuccess",
      base::StringValue(language));
}

void LanguageOptionsHandlerCommon::OnHunspellDictionaryDownloadFailure(
    const std::string& language) {
  web_ui()->CallJavascriptFunction(
      "options.LanguageOptions.onDictionaryDownloadFailure",
      base::StringValue(language));
}

base::DictionaryValue* LanguageOptionsHandlerCommon::GetUILanguageCodeSet() {
  base::DictionaryValue* dictionary = new base::DictionaryValue();
  const std::vector<std::string>& available_locales =
      l10n_util::GetAvailableLocales();
  for (size_t i = 0; i < available_locales.size(); ++i)
    dictionary->SetBoolean(available_locales[i], true);
  return dictionary;
}

base::DictionaryValue*
LanguageOptionsHandlerCommon::GetSpellCheckLanguageCodeSet() {
  base::DictionaryValue* dictionary = new base::DictionaryValue();
  std::vector<std::string> spell_check_languages;
  chrome::spellcheck_common::SpellCheckLanguages(&spell_check_languages);
  for (size_t i = 0; i < spell_check_languages.size(); ++i) {
    dictionary->SetBoolean(spell_check_languages[i], true);
  }
  return dictionary;
}

void LanguageOptionsHandlerCommon::LanguageOptionsOpenCallback(
    const base::ListValue* args) {
  content::RecordAction(UserMetricsAction("LanguageOptions_Open"));
  SpellcheckService* service = GetSpellcheckService();
  if (!service)
    return;

  for (auto dictionary : service->GetHunspellDictionaries()) {
    dictionary->RemoveObserver(this);
    dictionary->AddObserver(this);

    if (dictionary->IsDownloadInProgress())
      OnHunspellDictionaryDownloadBegin(dictionary->GetLanguage());
    else if (dictionary->IsDownloadFailure())
      OnHunspellDictionaryDownloadFailure(dictionary->GetLanguage());
    else
      OnHunspellDictionaryDownloadSuccess(dictionary->GetLanguage());
  }
}

void LanguageOptionsHandlerCommon::UiLanguageChangeCallback(
    const base::ListValue* args) {
  const std::string language_code =
      base::UTF16ToASCII(ExtractStringValue(args));
  CHECK(!language_code.empty());
  const std::string action = base::StringPrintf(
      "LanguageOptions_UiLanguageChange_%s", language_code.c_str());
  content::RecordComputedAction(action);
  SetApplicationLocale(language_code);
  base::StringValue language_value(language_code);
  web_ui()->CallJavascriptFunction("options.LanguageOptions.uiLanguageSaved",
                                   language_value);
}

void LanguageOptionsHandlerCommon::SpellCheckLanguageChangeCallback(
    const base::ListValue* args) {
  const std::string language_code =
      base::UTF16ToASCII(ExtractStringValue(args));
  const std::string action = base::StringPrintf(
      "LanguageOptions_SpellCheckLanguageChange_%s", language_code.c_str());
  content::RecordComputedAction(action);

  SpellcheckService* service = GetSpellcheckService();
  if (!service)
    return;

  for (auto dictionary : service->GetHunspellDictionaries()) {
    dictionary->RemoveObserver(this);
    dictionary->AddObserver(this);
  }
}

void LanguageOptionsHandlerCommon::UpdateLanguageListCallback(
    const base::ListValue* args) {
  CHECK_EQ(args->GetSize(), 1u);
  const base::ListValue* language_list;
  args->GetList(0, &language_list);
  DCHECK(language_list);

  std::vector<std::string> languages;
  for (base::ListValue::const_iterator it = language_list->begin();
       it != language_list->end(); ++it) {
    std::string lang;
    (*it)->GetAsString(&lang);
    languages.push_back(lang);
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  scoped_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(profile->GetPrefs());
  translate_prefs->UpdateLanguageList(languages);
}

void LanguageOptionsHandlerCommon::RetrySpellcheckDictionaryDownload(
    const base::ListValue* args) {
  std::string language = base::UTF16ToUTF8(ExtractStringValue(args));
  SpellcheckService* service = GetSpellcheckService();
  if (!service)
    return;

  for (auto dictionary : service->GetHunspellDictionaries()) {
    if (dictionary->GetLanguage() == language) {
      dictionary->RetryDownloadDictionary(
          Profile::FromWebUI(web_ui())->GetRequestContext());
      return;
    }
  }
}

SpellcheckService* LanguageOptionsHandlerCommon::GetSpellcheckService() {
  return SpellcheckServiceFactory::GetForContext(Profile::FromWebUI(web_ui()));
}

}  // namespace options
