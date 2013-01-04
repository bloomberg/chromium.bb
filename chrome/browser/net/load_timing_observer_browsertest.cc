// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"

class LoadTimingObserverTest : public InProcessBrowserTest {
 public:
  LoadTimingObserverTest() {}
};

// http://crbug.com/102030
IN_PROC_BROWSER_TEST_F(LoadTimingObserverTest, DISABLED_CacheHitAfterRedirect) {
  ASSERT_TRUE(test_server()->Start());
  GURL cached_page = test_server()->GetURL("cachetime");
  std::string redirect = "server-redirect?" + cached_page.spec();
  ui_test_utils::NavigateToURL(browser(), cached_page);
  ui_test_utils::NavigateToURL(browser(), test_server()->GetURL(redirect));

  int response_start = 0;
  int response_end = 0;
  content::WebContents* contents = chrome::GetActiveWebContents(browser());
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      contents,
      "window.domAutomationController.send("
      "    window.performance.timing.responseStart - "
      "    window.performance.timing.navigationStart)",
      &response_start));
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      contents,
      "window.domAutomationController.send("
      "    window.performance.timing.responseEnd - "
      "    window.performance.timing.navigationStart)",
      &response_end));
  EXPECT_LE(response_start, response_end);
}
