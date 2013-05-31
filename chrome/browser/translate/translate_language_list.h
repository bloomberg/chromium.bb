// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_LANGUAGE_LIST_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_LANGUAGE_LIST_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLFetcher;
}

// The TranslateLanguageList class is responsible for maintaining the latest
// supporting language list.
// This class is defined to be owned only by TranslateManager.
class TranslateLanguageList : public net::URLFetcherDelegate {
 public:
  TranslateLanguageList();
  virtual ~TranslateLanguageList();

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Fills |languages| with the list of languages that the translate server can
  // translate to and from. If alpha language support is enabled, it fills
  // |languages| with the list of all supporting languages including alpha
  // languages.
  void GetSupportedLanguages(std::vector<std::string>* languages);

  // Returns the language code that can be used with the Translate method for a
  // specified |chrome_locale|.
  std::string GetLanguageCode(const std::string& chrome_locale);

  // Returns true if |language| is supported by the translation server. If alpha
  // language support is enabled, also returns true if |language| is in alpha
  // language list.
  bool IsSupportedLanguage(const std::string& language);

  // Returns true if |language| is supported by the translation server as a
  // alpha language.
  bool IsAlphaLanguage(const std::string& language);

  // Fetches the language list from the translate server. It will not retry
  // more than kMaxRetryLanguageListFetch times.
  void RequestLanguageList();

  // static const values shared with our browser tests.
  static const char kLanguageListCallbackName[];
  static const char kTargetLanguagesKey[];

 private:
  // Updates |all_supported_languages_| to contain the union of
  // |supported_languages_| and |supported_alpha_languages_|.
  void UpdateSupportedLanguages();

  // The languages supported by the translation server.
  std::set<std::string> supported_languages_;

  // The alpha languages supported by the translation server.
  std::set<std::string> supported_alpha_languages_;

  // All the languages supported by the translation server. It contains the
  // union of |supported_languages_| and |supported_alpha_languages_|.
  std::set<std::string> all_supported_languages_;

  // An URLFetcher instance to fetch a server providing supported language list.
  scoped_ptr<net::URLFetcher> language_list_fetcher_;

  // An URLFetcher instance to fetch a server providing supported alpha language
  // list.
  scoped_ptr<net::URLFetcher> alpha_language_list_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(TranslateLanguageList);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_LANGUAGE_LIST_H_
