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
  // translate to and from.
  void GetSupportedLanguages(std::vector<std::string>* languages);

  // Returns the language code that can be used with the Translate method for a
  // specified |chrome_locale|.
  std::string GetLanguageCode(const std::string& chrome_locale);

  // Returns true if |language| is supported by the translation server.
  bool IsSupportedLanguage(const std::string& language);

  // TODO(toyoshim): Add IsSupportedAlphaLanguage() here.

  // Fetches the language list from the translate server. It will not retry
  // more than kMaxRetryLanguageListFetch times.
  void RequestLanguageList();

  // static const values shared with our browser tests.
  static const char kLanguageListCallbackName[];
  static const char kTargetLanguagesKey[];

 private:
  // Parses |language_list| and fills |supported_languages_| with the list of
  // languages that the translate server can translate to and from.
  void SetSupportedLanguages(const std::string& language_list);

  // The languages supported by the translation server.
  std::set<std::string> supported_languages_;

  // An URLFetcher instance to fetch a server providing supported language list.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(TranslateLanguageList);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_LANGUAGE_LIST_H_
