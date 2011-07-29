// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/test/test_server.h"

class ResourceDispatcherHostBrowserTest : public InProcessBrowserTest {
 public:
  ResourceDispatcherHostBrowserTest() {
    EnableDOMAutomation();
  }

 protected:
  RenderViewHost* render_view_host() {
    return browser()->GetSelectedTabContents()->render_view_host();
  }

  bool GetPopupTitle(const GURL& url, string16* title);
};

bool ResourceDispatcherHostBrowserTest::GetPopupTitle(const GURL& url,
                                                      string16* title) {
  ui_test_utils::NavigateToURL(browser(), url);

  ui_test_utils::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      NotificationService::AllSources());

  // Create dynamic popup.
  if (!ui_test_utils::ExecuteJavaScript(
          render_view_host(), L"", L"OpenPopup();"))
    return false;

  observer.Wait();

  std::set<Browser*> excluded;
  excluded.insert(browser());
  Browser* popup = ui_test_utils::GetBrowserNotInSet(excluded);
  if (!popup)
    return false;

  *title = popup->GetWindowTitleForCurrentTab();
  return true;
}

// Test title for content created by javascript window.open().
// See http://crbug.com/5988
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest, DynamicTitle1) {
  ASSERT_TRUE(test_server()->Start());

  GURL url(test_server()->GetURL("files/dynamic1.html"));
  string16 title;
  ASSERT_TRUE(GetPopupTitle(url, &title));
  EXPECT_TRUE(StartsWith(title, ASCIIToUTF16("My Popup Title"), true))
      << "Actual title: " << title;
}

// Test title for content created by javascript window.open().
// See http://crbug.com/5988
IN_PROC_BROWSER_TEST_F(ResourceDispatcherHostBrowserTest, DynamicTitle2) {
  ASSERT_TRUE(test_server()->Start());

  GURL url(test_server()->GetURL("files/dynamic2.html"));
  string16 title;
  ASSERT_TRUE(GetPopupTitle(url, &title));
  EXPECT_TRUE(StartsWith(title, ASCIIToUTF16("My Dynamic Title"), true))
      << "Actual title: " << title;
}
