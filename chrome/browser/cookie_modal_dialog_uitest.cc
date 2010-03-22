// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"

class CookieModalDialogTest : public UITest {
 public:
  void RunBasicTest(MessageBoxFlags::DialogButton button_to_press,
                    const std::wstring& expected_title) {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser);

    ASSERT_TRUE(browser->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                                  CONTENT_SETTING_ASK));

    GURL url(URLRequestMockHTTPJob::GetMockUrl(
        FilePath(FILE_PATH_LITERAL("cookie2.html"))));

    scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy);

    int64 last_navigation_time;
    ASSERT_TRUE(tab_proxy->GetLastNavigationTime(&last_navigation_time));
    ASSERT_TRUE(tab_proxy->NavigateToURLAsync(url));

    // cookie2.html sets a cookie and then reads back the cookie within onload,
    // which means that the navigation will not complete until the cookie
    // prompt is dismissed.

    bool modal_dialog_showing = false;
    MessageBoxFlags::DialogButton available_buttons;
    ASSERT_TRUE(automation()->WaitForAppModalDialog());
    ASSERT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
        &available_buttons));
    ASSERT_TRUE(modal_dialog_showing);
    ASSERT_NE((button_to_press & available_buttons), 0);
    ASSERT_TRUE(automation()->ClickAppModalDialogButton(button_to_press));

    // Now, the cookie prompt is dismissed, and we can wait for the navigation
    // to complete.  Before returning from onload, the test updates the title.
    // We can therefore be sure that upon return from WaitForNavigation that
    // the title has been updated with the final test result.

    ASSERT_TRUE(tab_proxy->WaitForNavigation(last_navigation_time));

    std::wstring title;
    ASSERT_TRUE(tab_proxy->GetTabTitle(&title));

    EXPECT_EQ(expected_title, title);
  }
};

// TODO(port): Enable these once the cookie dialogs are fully implemented.
#if defined(OS_WIN)
TEST_F(CookieModalDialogTest, AllowCookies) {
  RunBasicTest(MessageBoxFlags::DIALOGBUTTON_OK, L"cookie allowed");
}

TEST_F(CookieModalDialogTest, BlockCookies) {
  RunBasicTest(MessageBoxFlags::DIALOGBUTTON_CANCEL, L"cookie blocked");
}
#endif
