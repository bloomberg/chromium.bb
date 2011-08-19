// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNLOAD_UITEST_H_
#define CHROME_BROWSER_UNLOAD_UITEST_H_
#pragma once

#include "base/file_util.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "content/browser/net/url_request_mock_http_job.h"
#include "net/url_request/url_request_test_util.h"
#include "ui/base/events.h"
#include "ui/base/message_box_flags.h"

class UnloadTest : public UITest {
 public:
  virtual void SetUp() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(),
        "BrowserCloseTabWhenOtherTabHasListener") == 0) {
      launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
    }

    UITest::SetUp();
  }

  void CheckTitle(const std::wstring& expected_title) {
    const int kCheckDelayMs = 100;
    for (int max_wait_time = TestTimeouts::action_max_timeout_ms();
         max_wait_time > 0; max_wait_time -= kCheckDelayMs) {
      if (expected_title == GetActiveTabTitle())
        break;
      base::PlatformThread::Sleep(kCheckDelayMs);
    }

    EXPECT_EQ(expected_title, GetActiveTabTitle());
  }

  void NavigateToDataURL(const std::string& html_content,
                         const std::wstring& expected_title) {
    NavigateToURL(GURL("data:text/html," + html_content));
    CheckTitle(expected_title);
  }

  void NavigateToNolistenersFileTwice() {
    NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                      FilePath(FILE_PATH_LITERAL("title2.html"))));
    CheckTitle(L"Title Of Awesomeness");
    NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
                      FilePath(FILE_PATH_LITERAL("title2.html"))));
    CheckTitle(L"Title Of Awesomeness");
  }

  // Navigates to a URL asynchronously, then again synchronously. The first
  // load is purposely async to test the case where the user loads another
  // page without waiting for the first load to complete.
  void NavigateToNolistenersFileTwiceAsync() {
    NavigateToURLAsync(
        URLRequestMockHTTPJob::GetMockUrl(
            FilePath(FILE_PATH_LITERAL("title2.html"))));
    NavigateToURL(
        URLRequestMockHTTPJob::GetMockUrl(
            FilePath(FILE_PATH_LITERAL("title2.html"))));

    CheckTitle(L"Title Of Awesomeness");
  }

  void LoadUrlAndQuitBrowser(const std::string& html_content,
                             const std::wstring& expected_title = L"") {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser.get());
    NavigateToDataURL(html_content, expected_title);
    bool application_closed = false;
    EXPECT_TRUE(CloseBrowser(browser.get(), &application_closed));
  }

  void ClickModalDialogButton(ui::MessageBoxFlags::DialogButton button) {
    bool modal_dialog_showing = false;
    ui::MessageBoxFlags::DialogButton available_buttons;
    EXPECT_TRUE(automation()->WaitForAppModalDialog());
    EXPECT_TRUE(automation()->GetShowingAppModalDialog(&modal_dialog_showing,
        &available_buttons));
    ASSERT_TRUE(modal_dialog_showing);
    EXPECT_TRUE((button & available_buttons) != 0);
    EXPECT_TRUE(automation()->ClickAppModalDialogButton(button));
  }
};

#endif  // CHROME_BROWSER_UNLOAD_UITEST_H_
