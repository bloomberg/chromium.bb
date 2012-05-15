// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

class NotificationsPermissionTest : public InProcessBrowserTest {
 public:
  NotificationsPermissionTest() {
    EnableDOMAutomation();
  }
};

// If this flakes, use http://crbug.com/62311 and http://crbug.com/74428.
IN_PROC_BROWSER_TEST_F(NotificationsPermissionTest, TestUserGestureInfobar) {
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL(
          "files/notifications/notifications_request_function.html"));

  // Request permission by calling request() while eval'ing an inline script;
  // That's considered a user gesture to webkit, and should produce an infobar.
  bool result;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(),
      L"",
      L"window.domAutomationController.send(request());",
      &result));
  EXPECT_TRUE(result);

  EXPECT_EQ(1U, browser()->GetTabContentsWrapperAt(0)->infobar_tab_helper()->
            infobar_count());
}

// If this flakes, use http://crbug.com/62311.
IN_PROC_BROWSER_TEST_F(NotificationsPermissionTest, TestNoUserGestureInfobar) {
  ASSERT_TRUE(test_server()->Start());

  // Load a page which just does a request; no user gesture should result
  // in no infobar.
  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL(
          "files/notifications/notifications_request_inline.html"));

  EXPECT_EQ(0U, browser()->GetTabContentsWrapperAt(0)->infobar_tab_helper()->
            infobar_count());
}
