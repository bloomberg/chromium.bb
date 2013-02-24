// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browsertest.h"
#include "googleurl/src/gurl.h"

class MostVisitedWebUITest : public WebUIBrowserTest {
 public:
  virtual ~MostVisitedWebUITest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    WebUIBrowserTest::SetUpInProcessBrowserTestFixture();
    AddLibrary(base::FilePath(FILE_PATH_LITERAL("most_visited_page_test.js")));
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    WebUIBrowserTest::SetUpOnMainThread();
    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  }
};

WEB_UI_UNITTEST_F(MostVisitedWebUITest, refreshDataBasic);
WEB_UI_UNITTEST_F(MostVisitedWebUITest, refreshDataOrdering);
WEB_UI_UNITTEST_F(MostVisitedWebUITest, refreshDataPinning);
