// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_SCRIPT_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_SCRIPT_H_

#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

class TranslateScriptTest;
class TranslateURLFetcher;

class TranslateScript {
 public:
  typedef base::Callback<void(bool, const std::string&)> Callback;

  static const int kFetcherId = 0;

  TranslateScript();
  virtual ~TranslateScript();

  // Returns the feched the translate script.
  const std::string& data() { return data_; }

  // Used by unit-tests to override some defaults:
  // Delay after which the translate script is fetched again from the
  // translation server.
  void set_expiration_delay(int delay_ms) {
    expiration_delay_ = base::TimeDelta::FromMilliseconds(delay_ms);
  }

  // Clears the translate script, so it will be fetched next time we translate.
  void Clear() { data_.clear(); }

  // Fetches the JS translate script (the script that is injected in the page
  // to translate it).
  void Request(const Callback& callback);

  // Returns true if this has a pending request.
  bool HasPendingRequest() const { return fetcher_.get() != NULL; }

 private:
  friend class TranslateScriptTest;
  FRIEND_TEST_ALL_PREFIXES(TranslateScriptTest, CheckScriptParameters);
  FRIEND_TEST_ALL_PREFIXES(TranslateScriptTest, CheckScriptURL);

  static const char kScriptURL[];
  static const char kRequestHeader[];

  // Used in kTranslateScriptURL to specify using always ssl to load resources.
  static const char kAlwaysUseSslQueryName[];
  static const char kAlwaysUseSslQueryValue[];

  // Used in kTranslateScriptURL to specify a callback function name.
  static const char kCallbackQueryName[];
  static const char kCallbackQueryValue[];

  // Used in kTranslateScriptURL to specify a CSS loader callback function name.
  static const char kCssLoaderCallbackQueryName[];
  static const char kCssLoaderCallbackQueryValue[];

  // Used in kTranslateScriptURL to specify a JavaScript loader callback
  // function name.
  static const char kJavascriptLoaderCallbackQueryName[];
  static const char kJavascriptLoaderCallbackQueryValue[];

  // The callback when the script is fetched or a server error occured.
  void OnScriptFetchComplete(int id, bool success, const std::string& data);

  // URL fetcher to fetch the translate script.
  scoped_ptr<TranslateURLFetcher> fetcher_;

  // The JS injected in the page to do the translation.
  std::string data_;

  // Delay after which the translate script is fetched again from the translate
  // server.
  base::TimeDelta expiration_delay_;

  // The callback called when the server sends a response.
  Callback callback_;

  base::WeakPtrFactory<TranslateScript> weak_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TranslateScript);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_SCRIPT_H_
