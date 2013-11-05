// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

class FileManagerJsTest : public InProcessBrowserTest {
 public:
  // Runs all test functions in |file|, waiting for them to complete.
  void RunTest(const base::FilePath& file) {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("file_manager/unit_tests")), file);
    ui_test_utils::NavigateToURL(browser(), url);

    content::RenderViewHost* rvh = browser()->tab_strip_model()
        ->GetActiveWebContents()->GetRenderViewHost();
    ASSERT_TRUE(rvh);

    const std::vector<int> empty_libraries;
    EXPECT_TRUE(ExecuteWebUIResourceTest(rvh, empty_libraries));
  }
};

IN_PROC_BROWSER_TEST_F(
    FileManagerJsTest, NavigationListModelTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("navigation_list_model_unittest.html")));
}
