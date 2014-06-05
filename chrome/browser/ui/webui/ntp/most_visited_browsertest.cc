// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "url/gurl.h"

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

// TODO(samarth): delete along with the rest of the NTP4 code.
WEB_UI_UNITTEST_F(MostVisitedWebUITest, DISABLED_refreshDataBasic);
WEB_UI_UNITTEST_F(MostVisitedWebUITest, DISABLED_refreshDataOrdering);
WEB_UI_UNITTEST_F(MostVisitedWebUITest, DISABLED_refreshDataPinning);
