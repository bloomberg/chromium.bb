// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BIDI_CHECKER_WEB_UI_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_BIDI_CHECKER_WEB_UI_TEST_H_
#pragma once

#include <string>

#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace base {
class WaitableEvent;
}

// Base class for BidiChecker-based tests. Preloads the BidiChecker JS library
// for each test.
class WebUIBidiCheckerBrowserTest : public WebUIBrowserTest {
 public:
  virtual ~WebUIBidiCheckerBrowserTest();

 protected:
  WebUIBidiCheckerBrowserTest();

  // Runs the Bidi Checker on the given page URL. |is_rtl| should be true when
  // the active page locale is RTL.
  void RunBidiCheckerOnPage(const std::string& page_url, bool is_rtl);

  // Setup test path.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
};

// Base class for BidiChecker-based tests that run with an LTR UI.
class WebUIBidiCheckerBrowserTestLTR : public WebUIBidiCheckerBrowserTest {
 public:
  void RunBidiCheckerOnPage(const std::string& page_url);
};

// Base class for BidiChecker-based tests that run with an RTL UI.
class WebUIBidiCheckerBrowserTestRTL : public WebUIBidiCheckerBrowserTest {
 public:
  void RunBidiCheckerOnPage(const std::string& page_url);

 protected:
  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void CleanUpOnMainThread() OVERRIDE;

  static void SetUpOnIOThread(base::WaitableEvent* event);

  // The app locale before we change it
  std::string app_locale_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BIDI_CHECKER_WEB_UI_TEST_H_
