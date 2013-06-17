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
#include "chrome/browser/translate/translate_url_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/l10n/l10n_util.h"

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

// Retry parameter for fetching supporting language list.
const int kMaxRetryLanguageListFetch = 5;

// Assign following IDs to URLFetchers so that tests can distinguish each
// request in order to simiulate respectively.
const int kFetcherIdForLanguageList = 1;
const int kFetcherIdForAlphaLanguageList = 2;

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
                           std::set<std::string>* set) {
  DCHECK(set);
  // The format is:
  // sl({"sl": {"XX": "LanguageName", ...}, "tl": {"XX": "LanguageName", ...}})
  // Where "sl(" is set in kLanguageListCallbackName
  // and "tl" is kTargetLanguagesKey
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
  // The first level dictionary contains two sub-dict, one for source languages
  // and the other for target languages, we want to use the target languages.
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
  set->clear();
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
    set->insert(lang);
    if (message.empty())
      message += lang;
    else
      message += ", " + lang;
  }
  NotifyEvent(__LINE__, message);
}

}  // namespace

TranslateLanguageList::LanguageListFetcher::LanguageListFetcher(
    bool include_alpha_languages)
  : include_alpha_languages_(include_alpha_languages),
    state_(IDLE) {
}

TranslateLanguageList::LanguageListFetcher::~LanguageListFetcher() {
}

bool TranslateLanguageList::LanguageListFetcher::Request(
    const TranslateLanguageList::LanguageListFetcher::Callback& callback) {
  // This function is not supporsed to be called before previous operaion is not
  // finished.
  if (state_ == REQUESTING) {
    NOTREACHED();
    return false;
  }

  state_ = REQUESTING;
  callback_ = callback;

  GURL url = GURL(kLanguageListFetchURL);
  url = TranslateURLUtil::AddHostLocaleToUrl(url);
  url = TranslateURLUtil::AddApiKeyToUrl(url);
  if (include_alpha_languages_) {
    url = net::AppendQueryParameter(url,
                                    kAlphaLanguageQueryName,
                                    kAlphaLanguageQueryValue);
  }

  std::string message = base::StringPrintf(
      "%s list fetch starts (URL: %s)",
      include_alpha_languages_ ? "Language" : "Alpha language",
      url.spec().c_str());
  NotifyEvent(__LINE__, message);

  fetcher_.reset(net::URLFetcher::Create(
      include_alpha_languages_ ? kFetcherIdForAlphaLanguageList :
                                 kFetcherIdForLanguageList,
      url,
      net::URLFetcher::GET,
      this));
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                         net::LOAD_DO_NOT_SAVE_COOKIES);
  fetcher_->SetRequestContext(g_browser_process->system_request_context());
  fetcher_->SetMaxRetriesOn5xx(kMaxRetryLanguageListFetch);
  fetcher_->Start();

  return true;
}

void TranslateLanguageList::LanguageListFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(fetcher_.get() == source);

  std::string data;
  if (source->GetStatus().status() == net::URLRequestStatus::SUCCESS &&
      source->GetResponseCode() == net::HTTP_OK) {
    state_ = COMPLETED;
    source->GetResponseAsString(&data);
    std::string message = base::StringPrintf(
        "%s list is updated",
        include_alpha_languages_ ? "Alpha language" : "Language");
    NotifyEvent(__LINE__, message);
  } else {
    state_ = FAILED;
    std::string message = base::StringPrintf(
        "Failed to Fetch languages from: %s",
        source->GetURL().spec().c_str());
    NotifyEvent(__LINE__, message);
  }

  scoped_ptr<const net::URLFetcher> delete_ptr(fetcher_.release());
  callback_.Run(include_alpha_languages_, state_ == COMPLETED, data);
}

// This must be kept in sync with the &cb= value in the kLanguageListFetchURL.
const char TranslateLanguageList::kLanguageListCallbackName[] = "sl(";
const char TranslateLanguageList::kTargetLanguagesKey[] = "tl";

TranslateLanguageList::TranslateLanguageList() {
  // We default to our hard coded list of languages in
  // |kDefaultSupportedLanguages|. This list will be overriden by a server
  // providing supported langauges list.
  for (size_t i = 0; i < arraysize(kDefaultSupportedLanguages); ++i)
    supported_languages_.insert(kDefaultSupportedLanguages[i]);
  UpdateSupportedLanguages();

  language_list_fetcher_.reset(new LanguageListFetcher(false));
  alpha_language_list_fetcher_.reset(new LanguageListFetcher(true));
}

TranslateLanguageList::~TranslateLanguageList() {
}

void TranslateLanguageList::GetSupportedLanguages(
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  std::set<std::string>::const_iterator iter = all_supported_languages_.begin();
  for (; iter != all_supported_languages_.end(); ++iter)
    languages->push_back(*iter);
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
  // |language| should exist only in the alpha language list.
  return supported_alpha_languages_.count(language) != 0 &&
         supported_languages_.count(language) == 0;
}

void TranslateLanguageList::RequestLanguageList() {
  if (language_list_fetcher_.get() &&
      (language_list_fetcher_->state() == LanguageListFetcher::IDLE ||
       language_list_fetcher_->state() == LanguageListFetcher::FAILED)) {
    language_list_fetcher_->Request(
        base::Bind(&TranslateLanguageList::OnLanguageListFetchComplete,
                   base::Unretained(this)));
  }

  if (alpha_language_list_fetcher_.get() &&
      (alpha_language_list_fetcher_->state() == LanguageListFetcher::IDLE ||
       alpha_language_list_fetcher_->state() == LanguageListFetcher::FAILED)) {
    alpha_language_list_fetcher_->Request(
        base::Bind(&TranslateLanguageList::OnLanguageListFetchComplete,
                   base::Unretained(this)));
  }
}

void TranslateLanguageList::OnLanguageListFetchComplete(
    bool include_alpha_languages,
    bool success,
    const std::string& data) {
  if (!success)
    return;

  if (!include_alpha_languages) {
    SetSupportedLanguages(data, &supported_languages_);
    language_list_fetcher_.reset();
  } else {
    SetSupportedLanguages(data, &supported_alpha_languages_);
    alpha_language_list_fetcher_.reset();
  }
  UpdateSupportedLanguages();

  last_updated_ = base::Time::Now();
}

void TranslateLanguageList::UpdateSupportedLanguages() {
  all_supported_languages_.clear();
  std::set<std::string>::const_iterator iter;
  for (iter = supported_languages_.begin();
       iter != supported_languages_.end();
       ++iter) {
    all_supported_languages_.insert(*iter);
  }
  for (iter = supported_alpha_languages_.begin();
       iter != supported_alpha_languages_.end();
       ++iter) {
    all_supported_languages_.insert(*iter);
  }
}
