// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/BidiCheckerWebUITest.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/ui_test_utils.h"

static const FilePath::CharType* kWebUIBidiCheckerLibraryJS =
    FILE_PATH_LITERAL("third_party/bidichecker/bidichecker_packaged.js");

namespace {
FilePath WebUIBidiCheckerLibraryJSPath() {
  FilePath src_root;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &src_root))
    LOG(ERROR) << "Couldn't find source root";
  return src_root.Append(kWebUIBidiCheckerLibraryJS);
}
}

static const FilePath::CharType* kBidiCheckerTestsJS =
    FILE_PATH_LITERAL("bidichecker_tests.js");

WebUIBidiCheckerBrowserTest::~WebUIBidiCheckerBrowserTest() {}

WebUIBidiCheckerBrowserTest::WebUIBidiCheckerBrowserTest() {}

void WebUIBidiCheckerBrowserTest::SetUpInProcessBrowserTestFixture() {
  WebUIBrowserTest::SetUpInProcessBrowserTestFixture();
  WebUIBrowserTest::AddLibrary(WebUIBidiCheckerLibraryJSPath());
  WebUIBrowserTest::AddLibrary(FilePath(kBidiCheckerTestsJS));
}

void WebUIBidiCheckerBrowserTest::RunBidiCheckerOnPage(const char pageURL[],
                                                       bool isRTL) {
  ui_test_utils::NavigateToURL(browser(), GURL(pageURL));
  ASSERT_TRUE(RunJavascriptTest("runBidiChecker",
                                *Value::CreateStringValue(pageURL),
                                *Value::CreateBooleanValue(isRTL)));
}

IN_PROC_BROWSER_TEST_F(WebUIBidiCheckerBrowserTest, TestMainHistoryPageLTR) {
  HistoryService* history_service =
      browser()->profile()->GetHistoryService(Profile::IMPLICIT_ACCESS);
  const GURL history_url = GURL("http://www.ynet.co.il");
  history_service->AddPage(history_url, history::SOURCE_BROWSED);
  string16 title;
  ASSERT_TRUE(UTF8ToUTF16("\xD7\x91\xD7\x93\xD7\x99\xD7\xA7\xD7\x94\x21",
                          12,
                          &title));
  history_service->SetPageTitle(history_url, title);
  RunBidiCheckerOnPage(chrome::kChromeUIHistoryURL, false);
}
