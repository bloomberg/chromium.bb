// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MULTILANGUAGE_OPTIONS_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MULTILANGUAGE_OPTIONS_BROWSERTEST_H_

#include "base/macros.h"
#include "chrome/test/base/web_ui_browser_test.h"

// This is a helper class used by multilanguage_options_webui_browsertest.js
// to flip the enable-multilingual-spellchecker command line switch and set
// the AcceptLanguages and SpellcheckDictionaries preferences.
class MultilanguageOptionsBrowserTest : public WebUIBrowserTest {
 public:
  MultilanguageOptionsBrowserTest();
  ~MultilanguageOptionsBrowserTest() override;
  // Set the kSpellCheckDictionaries preference to an empty list value.
  void ClearSpellcheckDictionaries();

 private:
  // WebUIBrowserTest implementation.
  void SetUpOnMainThread() override;
  void SetDictionariesPref(const base::ListValue& value);

  DISALLOW_COPY_AND_ASSIGN(MultilanguageOptionsBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MULTILANGUAGE_OPTIONS_BROWSERTEST_H_
