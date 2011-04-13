// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_bidi_checker_browsertest.h"

#include "base/path_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"

static const FilePath::CharType* kWebUIBidiCheckerLibraryJS =
    FILE_PATH_LITERAL(
        "../../../../third_party/bidichecker/bidichecker_packaged.js");

WebUIBidiCheckerBrowserTest::~WebUIBidiCheckerBrowserTest() {}

WebUIBidiCheckerBrowserTest::WebUIBidiCheckerBrowserTest() {}

void WebUIBidiCheckerBrowserTest::SetUpInProcessBrowserTestFixture() {
  WebUIBrowserTest::SetUpInProcessBrowserTestFixture();
  WebUIBrowserTest::AddLibrary(kWebUIBidiCheckerLibraryJS);
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest, TestMainHistoryPageLTR) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIHistoryURL));
  AddLibrary(FILE_PATH_LITERAL("bidichecker_tests.js"));
  ASSERT_TRUE(RunJavascriptTest("runBidiCheckerLTR"));
}
