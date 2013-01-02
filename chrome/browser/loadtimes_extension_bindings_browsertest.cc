// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/test_server.h"

class LoadtimesExtensionBindingsTest : public InProcessBrowserTest {
 public:
  LoadtimesExtensionBindingsTest() {}

  void CompareBeforeAndAfter() {
    // TODO(simonjam): There's a race on whether or not first paint is populated
    // before we read them. We ought to test that too. Until the race is fixed,
    // zero it out so the test is stable.
    content::RenderViewHost* rvh =
        chrome::GetActiveWebContents(browser())->GetRenderViewHost();
    ASSERT_TRUE(content::ExecuteJavaScript(
        rvh,
        "",
        "window.before.firstPaintAfterLoadTime = 0;"
        "window.before.firstPaintTime = 0;"
        "window.after.firstPaintAfterLoadTime = 0;"
        "window.after.firstPaintTime = 0;"));

    std::string before;
    std::string after;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
        rvh,
        "",
        "window.domAutomationController.send("
        "    JSON.stringify(before))",
        &before));
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
        rvh,
        "",
        "window.domAutomationController.send("
        "    JSON.stringify(after))",
        &after));
    EXPECT_EQ(before, after);
  }
};

IN_PROC_BROWSER_TEST_F(LoadtimesExtensionBindingsTest,
                       LoadTimesSameAfterClientInDocNavigation) {
  ASSERT_TRUE(test_server()->Start());
  GURL plain_url = test_server()->GetURL("blank");
  ui_test_utils::NavigateToURL(browser(), plain_url);
  content::RenderViewHost* rvh =
      chrome::GetActiveWebContents(browser())->GetRenderViewHost();
  ASSERT_TRUE(content::ExecuteJavaScript(
      rvh, "", "window.before = window.chrome.loadTimes()"));
  ASSERT_TRUE(content::ExecuteJavaScript(
      rvh, "", "window.location.href = window.location + \"#\""));
  ASSERT_TRUE(content::ExecuteJavaScript(
      rvh, "", "window.after = window.chrome.loadTimes()"));
  CompareBeforeAndAfter();
}

IN_PROC_BROWSER_TEST_F(LoadtimesExtensionBindingsTest,
                       LoadTimesSameAfterUserInDocNavigation) {
  ASSERT_TRUE(test_server()->Start());
  GURL plain_url = test_server()->GetURL("blank");
  GURL hash_url(plain_url.spec() + "#");
  ui_test_utils::NavigateToURL(browser(), plain_url);
  content::RenderViewHost* rvh =
      chrome::GetActiveWebContents(browser())->GetRenderViewHost();
  ASSERT_TRUE(content::ExecuteJavaScript(
      rvh, "", "window.before = window.chrome.loadTimes()"));
  ui_test_utils::NavigateToURL(browser(), hash_url);
  ASSERT_TRUE(content::ExecuteJavaScript(
      rvh, "", "window.after = window.chrome.loadTimes()"));
  CompareBeforeAndAfter();
}
