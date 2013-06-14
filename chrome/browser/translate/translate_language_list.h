// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_LANGUAGE_LIST_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_LANGUAGE_LIST_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLFetcher;
}

// The TranslateLanguageList class is responsible for maintaining the latest
// supporting language list.
// This class is defined to be owned only by TranslateManager.
class TranslateLanguageList {
 public:
  TranslateLanguageList();
  virtual ~TranslateLanguageList();

  // Returns the last-updated time when the language list is fetched from the
  // Translate server. Returns null time if the list is yet to be fetched.
  base::Time last_updated() { return last_updated_; }

  // Fills |languages| with the list of languages that the translate server can
  // translate to and from. |languages| will include alpha languages.
  void GetSupportedLanguages(std::vector<std::string>* languages);

  // Returns the language code that can be used with the Translate method for a
  // specified |chrome_locale|.
  std::string GetLanguageCode(const std::string& chrome_locale);

  // Returns true if |language| is supported by the translation server. It also
  // returns true against alpha languages.
  bool IsSupportedLanguage(const std::string& language);

  // Returns true if |language| is supported by the translation server as a
  // alpha language.
  bool IsAlphaLanguage(const std::string& language);

  // Fetches the language list from the translate server. It will not retry
  // more than kMaxRetryLanguageListFetch times. Do nothing if the list is
  // already updated.
  void RequestLanguageList();

  // static const values shared with our browser tests.
  static const char kLanguageListCallbackName[];
  static const char kTargetLanguagesKey[];

 private:
  // The LanguageListFetcher class implements a client to fetch a server
  // supported language list. It also maintains the state to represent if the
  // fetch is completed successfully to try again later.
  class LanguageListFetcher : public net::URLFetcherDelegate {
   public:
    // Callback type for Request().
    typedef base::Callback<void(bool, bool, const std::string&)> Callback;

    // Represents internal state if the fetch is completed successfully.
    enum State {
      IDLE,        // No fetch request was issued.
      REQUESTING,  // A fetch request was issued, but not finished yet.
      COMPLETED,   // The last fetch request was finished successfully.
      FAILED,      // The last fetch request was finished with a failure.
    };

    explicit LanguageListFetcher(bool include_alpha_languages);
    virtual ~LanguageListFetcher();

    // Requests to fetch a server supported language list. |callback| will be
    // invoked when the function returns true, and the request is finished
    // asynchronously.
    // Returns false if the previous request is not finished, or the request
    // is omitted due to retry limitation.
    bool Request(const Callback& callback);

    // Gets internal state.
    State state() { return state_; }

    // net::URLFetcherDelegate implementation:
    virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

   private:
    // Represents if this instance should fetch a supported language list
    // including alpha languages.
    bool include_alpha_languages_;

    // Internal state.
    enum State state_;

    // URLFetcher instance.
    scoped_ptr<net::URLFetcher> fetcher_;

    // Callback passed at Request(). It will be invoked when an asynchronous
    // fetch operation is finished.
    Callback callback_;

    DISALLOW_COPY_AND_ASSIGN(LanguageListFetcher);
  };

  // Callback function called when LanguageListFetcher::Request() is finished.
  void OnLanguageListFetchComplete(bool include_alpha_languages,
                                   bool success,
                                   const std::string& data);

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

  // A LanguageListFetcher instance to fetch a server providing supported
  // language list.
  scoped_ptr<LanguageListFetcher> language_list_fetcher_;

  // A LanguageListFetcher instance to fetch a server providing supported alpha
  // language list.
  scoped_ptr<LanguageListFetcher> alpha_language_list_fetcher_;

  // The last-updated time when the language list is sent.
  base::Time last_updated_;

  DISALLOW_COPY_AND_ASSIGN(TranslateLanguageList);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_LANGUAGE_LIST_H_
