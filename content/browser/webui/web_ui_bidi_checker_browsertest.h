// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_BIDI_CHECKER_BROWSERTEST_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_BIDI_CHECKER_BROWSERTEST_H_
#pragma once

#include "content/browser/webui/web_ui_browsertest.h"
#include "chrome/test/in_process_browser_test.h"

// Base class for BidiChecker-based tests. Preloads the BidiChecker JS library
// for each test.
class WebUIBidiCheckerBrowserTest : public WebUIBrowserTest {
 public:
  virtual ~WebUIBidiCheckerBrowserTest();

 protected:
  WebUIBidiCheckerBrowserTest();

  // Setup test path.
  virtual void SetUpInProcessBrowserTestFixture();
};

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_BIDI_CHECKER_BROWSERTEST_H_
