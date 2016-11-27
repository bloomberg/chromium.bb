// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BIDI_CHECKER_WEB_UI_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_BIDI_CHECKER_WEB_UI_TEST_H_

#include "base/command_line.h"
#include "chrome/test/base/web_ui_browser_test.h"

// Base class for BidiChecker-based tests. Preloads the BidiChecker JS library
// for each test.
class WebUIBidiCheckerBrowserTest : public WebUIBrowserTest {
 public:
  ~WebUIBidiCheckerBrowserTest() override;

  // testing::Test implementation.
  void SetUp() override;
  void TearDown() override;

 protected:
  WebUIBidiCheckerBrowserTest();

  // Runs the Bidi Checker on the given page URL. |is_rtl| should be true when
  // the active page locale is RTL.
  void RunBidiCheckerOnPage(const std::string& page_url, bool is_rtl);

  // Setup test path.
  void SetUpInProcessBrowserTestFixture() override;

 private:
  // The command line args used to run the test before being changed in SetUp().
  base::CommandLine::StringVector argv_;
};

// Base class for BidiChecker-based tests that run with an LTR UI.
// Disabled on all platforms due to http://crbug.com/511439
class DISABLED_WebUIBidiCheckerBrowserTestLTR
    : public WebUIBidiCheckerBrowserTest {
 public:
  void RunBidiCheckerOnPage(const std::string& page_url);
};

// Base class for BidiChecker-based tests that run with an RTL UI.
// Disabled on all platforms due to http://crbug.com/511439
class DISABLED_WebUIBidiCheckerBrowserTestRTL
    : public WebUIBidiCheckerBrowserTest {
 public:
  void RunBidiCheckerOnPage(const std::string& page_url);

 protected:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // The app locale before we change it
  std::string app_locale_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BIDI_CHECKER_WEB_UI_TEST_H_
