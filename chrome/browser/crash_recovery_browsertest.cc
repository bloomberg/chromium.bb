// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationController;
using content::OpenURLParams;
using content::Referrer;

namespace {

void SimulateRendererCrash(Browser* browser) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::NotificationService::AllSources());
  browser->OpenURL(OpenURLParams(
      GURL(chrome::kChromeUICrashURL), Referrer(), CURRENT_TAB,
      content::PAGE_TRANSITION_TYPED, false));
  observer.Wait();
}

}  // namespace

class CrashRecoveryBrowserTest : public InProcessBrowserTest {
};

// Test that reload works after a crash.
// Disabled, http://crbug.com/29331 , http://crbug.com/69637 .
IN_PROC_BROWSER_TEST_F(CrashRecoveryBrowserTest, Reload) {
  // The title of the active tab should change each time this URL is loaded.
  GURL url(
      "data:text/html,<script>document.title=new Date().valueOf()</script>");
  ui_test_utils::NavigateToURL(browser(), url);

  string16 title_before_crash;
  string16 title_after_crash;

  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(),
                                                &title_before_crash));
  SimulateRendererCrash(browser());
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  chrome::Reload(browser(), CURRENT_TAB);
  observer.Wait();
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(),
                                                &title_after_crash));
  EXPECT_NE(title_before_crash, title_after_crash);
}

// Tests that loading a crashed page in a new tab correctly updates the title.
// There was an earlier bug (1270510) in process-per-site in which the max page
// ID of the RenderProcessHost was stale, so the NavigationEntry in the new tab
// was not committed.  This prevents regression of that bug.
IN_PROC_BROWSER_TEST_F(CrashRecoveryBrowserTest, LoadInNewTab) {
  const base::FilePath::CharType* kTitle2File =
      FILE_PATH_LITERAL("title2.html");

  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(kTitle2File)));

  string16 title_before_crash;
  string16 title_after_crash;

  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(),
                                                &title_before_crash));
  SimulateRendererCrash(browser());
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  chrome::Reload(browser(), CURRENT_TAB);
  observer.Wait();
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(),
                                                &title_after_crash));
  EXPECT_EQ(title_before_crash, title_after_crash);
}
