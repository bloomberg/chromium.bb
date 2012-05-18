// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "net/test/test_server.h"

class LoadTimingObserverTest : public InProcessBrowserTest {
 public:
  LoadTimingObserverTest() {
    EnableDOMAutomation();
  }

 protected:
};

// http://crbug.com/102030
IN_PROC_BROWSER_TEST_F(LoadTimingObserverTest, FLAKY_CacheHitAfterRedirect) {
  ASSERT_TRUE(test_server()->Start());
  GURL cached_page = test_server()->GetURL("cachetime");
  std::string redirect = "server-redirect?" + cached_page.spec();
  ui_test_utils::NavigateToURL(browser(), cached_page);
  ui_test_utils::NavigateToURL(browser(), test_server()->GetURL(redirect));

  int response_start = 0;
  int response_end = 0;
  content::RenderViewHost* render_view_host =
      browser()->GetSelectedWebContents()->GetRenderViewHost();
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
      render_view_host, L"",
      L"window.domAutomationController.send("
      L"window.performance.timing.responseStart - "
      L"window.performance.timing.navigationStart)", &response_start));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
      render_view_host, L"",
      L"window.domAutomationController.send("
      L"window.performance.timing.responseEnd - "
      L"window.performance.timing.navigationStart)", &response_end));
  EXPECT_LE(response_start, response_end);
}
