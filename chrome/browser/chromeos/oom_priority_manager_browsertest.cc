// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/oom_priority_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"

using content::OpenURLParams;

namespace {

typedef InProcessBrowserTest OomPriorityManagerTest;

IN_PROC_BROWSER_TEST_F(OomPriorityManagerTest, OomPriorityManagerBasics) {
  using ui_test_utils::WindowedNotificationObserver;

  // Get three tabs open.
  WindowedNotificationObserver load1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL(chrome::kChromeUIAboutURL), content::Referrer(),
                      CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);
  load1.Wait();

  WindowedNotificationObserver load2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL(chrome::kChromeUICreditsURL), content::Referrer(),
                      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED,
                      false);
  browser()->OpenURL(open2);
  load2.Wait();

  WindowedNotificationObserver load3(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open3(GURL(chrome::kChromeUITermsURL), content::Referrer(),
                      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED,
                      false);
  browser()->OpenURL(open3);
  load3.Wait();

  EXPECT_EQ(3, browser()->tab_count());

  // Navigate the current (third) tab to a different URL, so we can test
  // back/forward later.
  WindowedNotificationObserver load4(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open4(GURL(chrome::kChromeUIVersionURL), content::Referrer(),
                      CURRENT_TAB, content::PAGE_TRANSITION_TYPED,
                      false);
  browser()->OpenURL(open4);
  load4.Wait();

  // Navigate the third tab again, such that we have three navigation entries.
  WindowedNotificationObserver load5(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  OpenURLParams open5(GURL("chrome://dns"), content::Referrer(),
                      CURRENT_TAB, content::PAGE_TRANSITION_TYPED,
                      false);
  browser()->OpenURL(open5);
  load5.Wait();

  EXPECT_EQ(3, browser()->tab_count());

  // Discard a tab.  It should kill the first tab, since it was the oldest
  // and was not selected.
  EXPECT_TRUE(g_browser_process->oom_priority_manager()->DiscardTab());
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_TRUE(browser()->IsTabDiscarded(0));
  EXPECT_FALSE(browser()->IsTabDiscarded(1));
  EXPECT_FALSE(browser()->IsTabDiscarded(2));

  // Run discard again, make sure it kills the second tab.
  g_browser_process->oom_priority_manager()->DiscardTab();
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_TRUE(browser()->IsTabDiscarded(0));
  EXPECT_TRUE(browser()->IsTabDiscarded(1));
  EXPECT_FALSE(browser()->IsTabDiscarded(2));

  // Kill the third tab
  EXPECT_TRUE(g_browser_process->oom_priority_manager()->DiscardTab());
  EXPECT_TRUE(browser()->IsTabDiscarded(0));
  EXPECT_TRUE(browser()->IsTabDiscarded(1));
  EXPECT_TRUE(browser()->IsTabDiscarded(2));

  // Running when all tabs are discarded should do nothing.
  EXPECT_FALSE(g_browser_process->oom_priority_manager()->DiscardTab());

  // Force creation of the FindBarController.
  browser()->GetFindBarController();

  // Select the first tab.  It should reload.
  WindowedNotificationObserver reload1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  browser()->SelectNumberedTab(0);
  reload1.Wait();
  // Make sure the FindBarController gets the right TabContentsWrapper.
  EXPECT_EQ(browser()->GetFindBarController()->tab_contents(),
            browser()->GetSelectedTabContentsWrapper());
  EXPECT_EQ(0, browser()->active_index());
  EXPECT_FALSE(browser()->IsTabDiscarded(0));
  EXPECT_TRUE(browser()->IsTabDiscarded(1));
  EXPECT_TRUE(browser()->IsTabDiscarded(2));

  // Select the third tab. It should reload.
  WindowedNotificationObserver reload2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  browser()->SelectNumberedTab(2);
  reload2.Wait();
  EXPECT_EQ(2, browser()->active_index());
  EXPECT_FALSE(browser()->IsTabDiscarded(0));
  EXPECT_TRUE(browser()->IsTabDiscarded(1));
  EXPECT_FALSE(browser()->IsTabDiscarded(2));

  // Navigate the third tab back twice.  We used to crash here due to
  // crbug.com/121373.
  EXPECT_TRUE(browser()->CanGoBack());
  EXPECT_FALSE(browser()->CanGoForward());
  WindowedNotificationObserver back1(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  browser()->GoBack(CURRENT_TAB);
  back1.Wait();
  EXPECT_TRUE(browser()->CanGoBack());
  EXPECT_TRUE(browser()->CanGoForward());
  WindowedNotificationObserver back2(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  browser()->GoBack(CURRENT_TAB);
  back2.Wait();
  EXPECT_FALSE(browser()->CanGoBack());
  EXPECT_TRUE(browser()->CanGoForward());
}

}  // namespace
