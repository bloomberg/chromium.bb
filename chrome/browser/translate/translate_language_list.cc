// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_language_list.h"

#include <set>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/translate/translate_browser_metrics.h"
#include "chrome/browser/translate/translate_event_details.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/translate/translate_url_fetcher.h"
#include "chrome/browser/translate/translate_url_util.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// The default list of languages the Google translation server supports.
// We use this list until we receive the list that the server exposes.
// For information, here is the list of languages that Chrome can be run in
// but that the translation server does not support:
// am Amharic
// bn Bengali
// gu Gujarati
// kn Kannada
// ml Malayalam
// mr Marathi
// ta Tamil
// te Telugu
const char* const kDefaultSupportedLanguages[] = {
  "af",     // Afrikaans
  "sq",     // Albanian
  "ar",     // Arabic
  "be",     // Belarusian
  "bg",     // Bulgarian
  "ca",     // Catalan
  "zh-CN",  // Chinese (Simplified)
  "zh-TW",  // Chinese (Traditional)
  "hr",     // Croatian
  "cs",     // Czech
  "da",     // Danish
  "nl",     // Dutch
  "en",     // English
  "eo",     // Esperanto
  "et",     // Estonian
  "tl",     // Filipino
  "fi",     // Finnish
  "fr",     // French
  "gl",     // Galician
  "de",     // German
  "el",     // Greek
  "ht",     // Haitian Creole
  "iw",     // Hebrew
  "hi",     // Hindi
  "hu",     // Hungarian
  "is",     // Icelandic
  "id",     // Indonesian
  "ga",     // Irish
  "it",     // Italian
  "ja",     // Japanese
  "ko",     // Korean
  "lv",     // Latvian
  "lt",     // Lithuanian
  "mk",     // Macedonian
  "ms",     // Malay
  "mt",     // Maltese
  "no",     // Norwegian
  "fa",     // Persian
  "pl",     // Polish
  "pt",     // Portuguese
  "ro",     // Romanian
  "ru",     // Russian
  "sr",     // Serbian
  "sk",     // Slovak
  "sl",     // Slovenian
  "es",     // Spanish
  "sw",     // Swahili
  "sv",     // Swedish
  "th",     // Thai
  "tr",     // Turkish
  "uk",     // Ukrainian
  "vi",     // Vietnamese
  "cy",     // Welsh
  "yi",     // Yiddish
};

// Constant URL string to fetch server supporting language list.
const char kLanguageListFetchURL[] =
    "https://translate.googleapis.com/translate_a/l?client=chrome&cb=sl";

// Used in kTranslateScriptURL to request supporting languages list including
// "alpha languages".
const char kAlphaLanguageQueryName[] = "alpha";
const char kAlphaLanguageQueryValue[] = "1";

// Represent if the language list updater is disabled.
bool update_is_disabled = false;

// Retry parameter for fetching.
const int kMaxRetryOn5xx = 5;

// Show a message in chrome:://translate-internals Event Logs.
void NotifyEvent(int line, const std::string& message) {
  TranslateManager* manager = TranslateManager::GetInstance();
  DCHECK(manager);

  TranslateEventDetails details(__FILE__, line, message);
  manager->NotifyTranslateEvent(details);
}

// Parses |language_list| containing the list of languages that the translate
// server can translate to and from, and fills |set| with them.
void SetSupportedLanguages(const std::string& language_list,
                           std::set<std::string>* target_language_set,
                           std::set<std::string>* alpha_language_set) {
  DCHECK(target_language_set);
  DCHECK(alpha_language_set);

  // The format is:
  // sl({
  //   "sl": {"XX": "LanguageName", ...},
  //   "tl": {"XX": "LanguageName", ...},
  //   "al": {"XX": 1, ...}
  // })
  // Where "sl(" is set in kLanguageListCallbackName, "tl" is
  // kTargetLanguagesKey and "al" kAlphaLanguagesKey.
  if (!StartsWithASCII(language_list,
                       TranslateLanguageList::kLanguageListCallbackName,
                       false) ||
      !EndsWith(language_list, ")", false)) {
    // We don't have a NOTREACHED here since this can happen in ui_tests, even
    // though the the BrowserMain function won't call us with parameters.ui_task
    // is NULL some tests don't set it, so we must bail here.
    return;
  }
  static const size_t kLanguageListCallbackNameLength =
      strlen(TranslateLanguageList::kLanguageListCallbackName);
  std::string languages_json = language_list.substr(
      kLanguageListCallbackNameLength,
      language_list.size() - kLanguageListCallbackNameLength - 1);
  scoped_ptr<Value> json_value(
      base::JSONReader::Read(languages_json, base::JSON_ALLOW_TRAILING_COMMAS));
  if (json_value == NULL || !json_value->IsType(Value::TYPE_DICTIONARY)) {
    NOTREACHED();
    return;
  }
  // The first level dictionary contains three sub-dict, first for source
  // languages and second for target languages, we want to use the target
  // languages. The last is for alpha languages.
  DictionaryValue* language_dict =
      static_cast<DictionaryValue*>(json_value.get());
  DictionaryValue* target_languages = NULL;
  if (!language_dict->GetDictionary(TranslateLanguageList::kTargetLanguagesKey,
                                    &target_languages) ||
      target_languages == NULL) {
    NOTREACHED();
    return;
  }

  const std::string& locale = g_browser_process->GetApplicationLocale();

  // Now we can clear language list.
  target_language_set->clear();
  std::string message;
  // ... and replace it with the values we just fetched from the server.
  for (DictionaryValue::Iterator iter(*target_languages);
       !iter.IsAtEnd();
       iter.Advance()) {
    const std::string& lang = iter.key();
    if (!l10n_util::IsLocaleNameTranslated(lang.c_str(), locale)) {
      TranslateBrowserMetrics::ReportUndisplayableLanguage(lang);
      continue;
    }
    target_language_set->insert(lang);
    if (message.empty())
      message += lang;
    else
      message += ", " + lang;
  }
  NotifyEvent(__LINE__, message);

  // Get the alpha languages. The "al" parameter could be abandoned.
  DictionaryValue* alpha_languages = NULL;
  if (!language_dict->GetDictionary(TranslateLanguageList::kAlphaLanguagesKey,
                                    &alpha_languages) ||
      alpha_languages == NULL) {
    return;
  }

  // We assume that the alpha languages are included in the above target
  // languages, and don't use UMA or NotifyEvent.
  alpha_language_set->clear();
  for (DictionaryValue::Iterator iter(*alpha_languages);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& lang = iter.key();
    if (!l10n_util::IsLocaleNameTranslated(lang.c_str(), locale))
      continue;
    alpha_language_set->insert(lang);
  }
}

}  // namespace

// This must be kept in sync with the &cb= value in the kLanguageListFetchURL.
const char TranslateLanguageList::kLanguageListCallbackName[] = "sl(";
const char TranslateLanguageList::kTargetLanguagesKey[] = "tl";
const char TranslateLanguageList::kAlphaLanguagesKey[] = "al";

TranslateLanguageList::TranslateLanguageList() {
  // We default to our hard coded list of languages in
  // |kDefaultSupportedLanguages|. This list will be overriden by a server
  // providing supported langauges list.
  for (size_t i = 0; i < arraysize(kDefaultSupportedLanguages); ++i)
    all_supported_languages_.insert(kDefaultSupportedLanguages[i]);

  if (update_is_disabled)
    return;

  language_list_fetcher_.reset(new TranslateURLFetcher(kFetcherId));
  language_list_fetcher_->set_max_retry_on_5xx(kMaxRetryOn5xx);
}

TranslateLanguageList::~TranslateLanguageList() {
}

void TranslateLanguageList::GetSupportedLanguages(
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  std::set<std::string>::const_iterator iter = all_supported_languages_.begin();
  for (; iter != all_supported_languages_.end(); ++iter)
    languages->push_back(*iter);

  // Update language lists if they are not updated after Chrome was launched
  // for later requests.
  if (!update_is_disabled && language_list_fetcher_.get())
    RequestLanguageList();
}

std::string TranslateLanguageList::GetLanguageCode(
    const std::string& chrome_locale) {
  // Only remove the country code for country specific languages we don't
  // support specifically yet.
  if (IsSupportedLanguage(chrome_locale))
    return chrome_locale;

  size_t hypen_index = chrome_locale.find('-');
  if (hypen_index == std::string::npos)
    return chrome_locale;
  return chrome_locale.substr(0, hypen_index);
}

bool TranslateLanguageList::IsSupportedLanguage(const std::string& language) {
  return all_supported_languages_.count(language) != 0;
}

bool TranslateLanguageList::IsAlphaLanguage(const std::string& language) {
  return alpha_languages_.count(language) != 0;
}

void TranslateLanguageList::RequestLanguageList() {
  // If resource requests are not allowed, we'll get a callback when they are.
  if (resource_request_allowed_notifier_.ResourceRequestsAllowed())
    OnResourceRequestsAllowed();
}

void TranslateLanguageList::OnResourceRequestsAllowed() {
  if (language_list_fetcher_.get() &&
      (language_list_fetcher_->state() == TranslateURLFetcher::IDLE ||
       language_list_fetcher_->state() == TranslateURLFetcher::FAILED)) {
    GURL url = GURL(kLanguageListFetchURL);
    url = TranslateURLUtil::AddHostLocaleToUrl(url);
    url = TranslateURLUtil::AddApiKeyToUrl(url);
    url = net::AppendQueryParameter(url,
                                    kAlphaLanguageQueryName,
                                    kAlphaLanguageQueryValue);

    std::string message = base::StringPrintf(
        "Language list includeing alpha languages fetch starts (URL: %s)",
        url.spec().c_str());
    NotifyEvent(__LINE__, message);

    bool result = language_list_fetcher_->Request(
        url,
        base::Bind(&TranslateLanguageList::OnLanguageListFetchComplete,
                   base::Unretained(this)));
    if (!result)
      NotifyEvent(__LINE__, "Request is omitted due to retry limit");
  }
}

// static
void TranslateLanguageList::DisableUpdate() {
  update_is_disabled = true;
}

void TranslateLanguageList::OnLanguageListFetchComplete(
    int id,
    bool success,
    const std::string& data) {
  if (!success) {
    // Since it fails just now, omit to schedule resource requests if
    // ResourceRequestAllowedNotifier think it's ready. Otherwise, a callback
    // will be invoked later to request resources again.
    // The TranslateURLFetcher has a limit for retried requests and aborts
    // re-try not to invoke OnLanguageListFetchComplete anymore if it's asked to
    // re-try too many times.
    NotifyEvent(__LINE__, "Failed to fetch languages");
    return;
  }

  NotifyEvent(__LINE__, "Language list is updated");

  DCHECK_EQ(kFetcherId, id);

  SetSupportedLanguages(data, &all_supported_languages_, &alpha_languages_);
  language_list_fetcher_.reset();

  last_updated_ = base::Time::Now();
}
