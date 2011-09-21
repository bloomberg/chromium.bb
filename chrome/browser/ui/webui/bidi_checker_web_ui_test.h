// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BIDI_CHECKER_WEB_UI_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_BIDI_CHECKER_WEB_UI_TEST_H_
#pragma once

#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "chrome/test/base/in_process_browser_test.h"

// Base class for BidiChecker-based tests. Preloads the BidiChecker JS library
// for each test.
class WebUIBidiCheckerBrowserTest : public WebUIBrowserTest {
 public:
  virtual ~WebUIBidiCheckerBrowserTest();

  // Runs the Bidi Checker on the given page URL. |isRTL| should be true when
  // the active page locale is RTL.
  void RunBidiCheckerOnPage(const char pageURL[], bool isRTL);

 protected:
  WebUIBidiCheckerBrowserTest();

  // Setup test path.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
};

// Base class for BidiChecker-based tests that run with an RTL UI.
// Preloads the BidiChecker JS library for each test, as well as modifies the
// active language.
class WebUIBidiCheckerBrowserTestFakeBidi : public WebUIBidiCheckerBrowserTest {
 public:
  virtual ~WebUIBidiCheckerBrowserTestFakeBidi();

 protected:
  WebUIBidiCheckerBrowserTestFakeBidi();

  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void CleanUpOnMainThread() OVERRIDE;

  // The app locale before we change it
  std::string app_locale_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BIDI_CHECKER_WEB_UI_TEST_H_
