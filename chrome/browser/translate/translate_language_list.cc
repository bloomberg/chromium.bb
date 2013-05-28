// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_language_list.h"

#include <set>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/translate/translate_url_util.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

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

}  // namespace

// This must be kept in sync with the &cb= value in the kLanguageListFetchURL.
const char TranslateLanguageList::kLanguageListCallbackName[] = "sl(";
const char TranslateLanguageList::kTargetLanguagesKey[] = "tl";

TranslateLanguageList::TranslateLanguageList() {
  // We default to our hard coded list of languages in
  // |kDefaultSupportedLanguages|. This list will be overriden by a server
  // providing supported langauges list.
  for (size_t i = 0; i < arraysize(kDefaultSupportedLanguages); ++i)
    supported_languages_.insert(kDefaultSupportedLanguages[i]);
}

TranslateLanguageList::~TranslateLanguageList() {}

void TranslateLanguageList::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(url_fetcher_.get() == source);
  scoped_ptr<const net::URLFetcher> delete_ptr(url_fetcher_.release());

  if (source->GetStatus().status() == net::URLRequestStatus::SUCCESS &&
      source->GetResponseCode() == net::HTTP_OK) {
    std::string data;
    source->GetResponseAsString(&data);
    SetSupportedLanguages(data);
  } else {
    // TODO(toyoshim): Try again. http://crbug.com/244202 .
    VLOG(9) << "Failed to Fetch languages from: " << kLanguageListFetchURL;
  }
}

void TranslateLanguageList::GetSupportedLanguages(
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  std::set<std::string>::const_iterator iter = supported_languages_.begin();
  for (; iter != supported_languages_.end(); ++iter)
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

bool TranslateLanguageList::IsSupportedLanguage(
    const std::string& page_language) {
  return supported_languages_.count(page_language) != 0;
}

void TranslateLanguageList::RequestLanguageList() {
  if (url_fetcher_.get())
    return;

  GURL language_list_fetch_url = GURL(kLanguageListFetchURL);
  language_list_fetch_url =
      TranslateURLUtil::AddHostLocaleToUrl(language_list_fetch_url);
  language_list_fetch_url =
      TranslateURLUtil::AddApiKeyToUrl(language_list_fetch_url);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableTranslateAlphaLanguages)) {
    language_list_fetch_url = net::AppendQueryParameter(
        language_list_fetch_url,
        kAlphaLanguageQueryName,
        kAlphaLanguageQueryValue);
  }

  VLOG(9) << "Fetch supporting language list from: "
          << language_list_fetch_url.spec().c_str();

  url_fetcher_.reset(net::URLFetcher::Create(1,
                                             language_list_fetch_url,
                                             net::URLFetcher::GET,
                                             this));
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);
  url_fetcher_->SetRequestContext(g_browser_process->system_request_context());
  url_fetcher_->SetMaxRetriesOn5xx(kMaxRetryLanguageListFetch);
  url_fetcher_->Start();
}

void TranslateLanguageList::SetSupportedLanguages(
    const std::string& language_list) {
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
  // Now we can clear our current state...
  supported_languages_.clear();
  // ... and replace it with the values we just fetched from the server.
  for (DictionaryValue::Iterator iter(*target_languages);
       !iter.IsAtEnd();
       iter.Advance()) {
    supported_languages_.insert(iter.key());
  }
}
