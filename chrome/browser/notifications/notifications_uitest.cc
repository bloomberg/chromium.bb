// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_unittest.h"

class NotificationsPermissionTest : public UITest {
 public:
  NotificationsPermissionTest() {
    launch_arguments_.AppendSwitch(switches::kEnableDesktopNotifications);
    dom_automation_enabled_ = true;
    show_window_ = true;
  }
};

#if defined(OS_WIN)
TEST_F(NotificationsPermissionTest, TestUserGestureInfobar) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(server.get() != NULL);

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  scoped_refptr<TabProxy> tab(browser->GetActiveTab());
  tab->NavigateToURL(server->TestServerPageW(
      L"files/notifications/notifications_request_function.html"));
  WaitUntilTabCount(1);

  // Request permission by calling request() while eval'ing an inline script;
  // That's considered a user gesture to webkit, and should produce an infobar.
  bool result;
  tab->ExecuteAndExtractBool(L"",
      L"window.domAutomationController.send(request());",
      &result);
  EXPECT_TRUE(result);

  int info_bar_count;
  tab->GetInfoBarCount(&info_bar_count);
  EXPECT_EQ(1, info_bar_count);
}

TEST_F(NotificationsPermissionTest, TestNoUserGestureInfobar) {
  const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(server.get() != NULL);

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  scoped_refptr<TabProxy> tab(browser->GetActiveTab());

  // Load a page which just does a request; no user gesture should result
  // in no infobar.
  tab->NavigateToURL(server->TestServerPageW(
      L"files/notifications/notifications_request_inline.html"));
  WaitUntilTabCount(1);

  int info_bar_count;
  tab->GetInfoBarCount(&info_bar_count);
  EXPECT_EQ(0, info_bar_count);
}
#endif  // OS_WIN
