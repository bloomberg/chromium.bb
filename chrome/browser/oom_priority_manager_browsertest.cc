// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/oom_priority_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"

namespace {

class OomPriorityManagerTest : public InProcessBrowserTest {
};

IN_PROC_BROWSER_TEST_F(OomPriorityManagerTest, OomPriorityManagerBasics) {
  using namespace ui_test_utils;

  // Get three tabs open.  Load asynchronously to speed up the test.
  WindowedNotificationObserver load1(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  OpenURLParams open1(GURL("chrome://about"), content::Referrer(),
                      CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false);
  browser()->OpenURL(open1);

  WindowedNotificationObserver load2(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  OpenURLParams open2(GURL("chrome://credits"), content::Referrer(),
                      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED,
                      false);
  browser()->OpenURL(open2);

  WindowedNotificationObserver load3(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  OpenURLParams open3(GURL("chrome://terms"), content::Referrer(),
                      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED,
                      false);
  browser()->OpenURL(open3);

  load1.Wait();
  load2.Wait();
  load3.Wait();

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

  // Select the first tab.  It should reload.
  WindowedNotificationObserver reload1(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  browser()->SelectNumberedTab(0);
  reload1.Wait();
  EXPECT_FALSE(browser()->IsTabDiscarded(0));
  EXPECT_TRUE(browser()->IsTabDiscarded(1));
  EXPECT_TRUE(browser()->IsTabDiscarded(2));
}

}  // namespace
