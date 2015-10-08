// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_SINGLE_LANGUAGE_OPTIONS_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_SINGLE_LANGUAGE_OPTIONS_BROWSERTEST_H_

#include "base/macros.h"
#include "chrome/test/base/web_ui_browser_test.h"

// This is a helper class for language_options_webui_browsertest.js to disable
// multilingual-spellchecker.
class SingleLanguageOptionsBrowserTest : public WebUIBrowserTest {
 public:
  SingleLanguageOptionsBrowserTest();
  ~SingleLanguageOptionsBrowserTest() override;

 private:
  // WebUIBrowserTest implementation.
  void SetUpCommandLine(base::CommandLine* command_line) override;

  DISALLOW_COPY_AND_ASSIGN(SingleLanguageOptionsBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_SINGLE_LANGUAGE_OPTIONS_BROWSERTEST_H_
