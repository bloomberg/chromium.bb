// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_LANGUAGE_LIST_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_LANGUAGE_LIST_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/web_resource/resource_request_allowed_notifier.h"

class TranslateURLFetcher;

// The TranslateLanguageList class is responsible for maintaining the latest
// supporting language list.
// This class is defined to be owned only by TranslateManager.
class TranslateLanguageList : public ResourceRequestAllowedNotifier::Observer {
 public:
  static const int kFetcherId = 1;

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

  // Fetches the language list from the translate server. It will retry
  // automatically when a server return 5xx errors and retry count doesn't
  // reach to limits.
  void RequestLanguageList();

  // ResourceRequestAllowedNotifier::Observer implementation:
  virtual void OnResourceRequestsAllowed() OVERRIDE;

  // Disables the language list updater. This is used only for testing now.
  static void DisableUpdate();

  // static const values shared with our browser tests.
  static const char kLanguageListCallbackName[];
  static const char kTargetLanguagesKey[];
  static const char kAlphaLanguagesKey[];

 private:
  // Callback function called when TranslateURLFetcher::Request() is finished.
  void OnLanguageListFetchComplete(int id,
                                   bool success,
                                   const std::string& data);

  // All the languages supported by the translation server.
  std::set<std::string> all_supported_languages_;

  // Alpha languages supported by the translation server.
  std::set<std::string> alpha_languages_;

  // A LanguageListFetcher instance to fetch a server providing supported
  // language list including alpha languages.
  scoped_ptr<TranslateURLFetcher> language_list_fetcher_;

  // The last-updated time when the language list is sent.
  base::Time last_updated_;

  // Helper class to know if it's allowed to make network resource requests.
  ResourceRequestAllowedNotifier resource_request_allowed_notifier_;

  DISALLOW_COPY_AND_ASSIGN(TranslateLanguageList);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_LANGUAGE_LIST_H_
