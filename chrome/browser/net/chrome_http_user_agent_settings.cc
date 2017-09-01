// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_http_user_agent_settings.h"

#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_util.h"

namespace {

// Helper class that builds the list of languages for the Accept-Language
// headers.
// The output is a comma-separated list of languages as string.
// Duplicates are removed.
class AcceptLanguageBuilder {
 public:
  // Adds a language to the string.
  // Duplicates are ignored.
  void AddLanguageCode(const std::string& language) {
    if (seen_.find(language) == seen_.end()) {
      if (str_.empty()) {
        base::StringAppendF(&str_, "%s", language.c_str());
      } else {
        base::StringAppendF(&str_, ",%s", language.c_str());
      }
      seen_.insert(language);
    }
  }

  // Returns the string constructed up to this point.
  std::string GetString() const { return str_; }

 private:
  // The string that contains the list of languages, comma-separated.
  std::string str_;
  // Set the remove duplicates.
  std::unordered_set<std::string> seen_;
};

// Extract the base language code from a language code.
// If there is no '-' in the code, the original code is returned.
std::string GetBaseLanguageCode(const std::string& language_code) {
  const std::vector<std::string> tokens = base::SplitString(
      language_code, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return tokens.empty() ? "" : tokens[0];
}

}  // namespace

ChromeHttpUserAgentSettings::ChromeHttpUserAgentSettings(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pref_accept_language_.Init(prefs::kAcceptLanguages, prefs);
  last_pref_accept_language_ = *pref_accept_language_;

  const std::string accept_languages_str =
      base::FeatureList::IsEnabled(features::kUseNewAcceptLanguageHeader)
          ? ExpandLanguageList(last_pref_accept_language_)
          : last_pref_accept_language_;
  last_http_accept_language_ =
      net::HttpUtil::GenerateAcceptLanguageHeader(accept_languages_str);

  pref_accept_language_.MoveToThread(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO));
}

ChromeHttpUserAgentSettings::~ChromeHttpUserAgentSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

std::string ChromeHttpUserAgentSettings::ExpandLanguageList(
    const std::string& language_prefs) {
  const std::vector<std::string> languages = base::SplitString(
      language_prefs, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (languages.empty())
    return "";

  AcceptLanguageBuilder builder;

  const int size = languages.size();
  for (int i = 0; i < size; ++i) {
    const std::string& language = languages[i];
    builder.AddLanguageCode(language);

    // Extract the base language
    const std::string& base_language = GetBaseLanguageCode(language);

    // Look ahead and add the base language if the next language is not part
    // of the same family.
    const int j = i + 1;
    if (j >= size || GetBaseLanguageCode(languages[j]) != base_language) {
      builder.AddLanguageCode(base_language);
    }
  }

  return builder.GetString();
}

void ChromeHttpUserAgentSettings::CleanupOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pref_accept_language_.Destroy();
}

std::string ChromeHttpUserAgentSettings::GetAcceptLanguage() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::string new_pref_accept_language = *pref_accept_language_;

  if (new_pref_accept_language != last_pref_accept_language_) {
    const std::string accept_languages_str =
        base::FeatureList::IsEnabled(features::kUseNewAcceptLanguageHeader)
            ? ExpandLanguageList(new_pref_accept_language)
            : new_pref_accept_language;
    last_http_accept_language_ =
        net::HttpUtil::GenerateAcceptLanguageHeader(accept_languages_str);
    last_pref_accept_language_ = new_pref_accept_language;
  }

  return last_http_accept_language_;
}

std::string ChromeHttpUserAgentSettings::GetUserAgent() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return ::GetUserAgent();
}

