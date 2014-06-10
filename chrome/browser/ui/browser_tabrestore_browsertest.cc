// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"

typedef InProcessBrowserTest BrowserTabRestoreTest;

void AwaitTabsReady(content::DOMMessageQueue* message_queue, int tabs) {
  for (int i = 0; i < tabs; ++i) {
    std::string message;
    EXPECT_TRUE(message_queue->WaitForMessage(&message));
    EXPECT_EQ("\"READY\"", message);
  }
}

void CheckVisbility(TabStripModel* tab_strip_model, int visible_index) {
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    std::string document_visibility_state;
    const char kGetStateJS[] = "window.domAutomationController.send("
        "window.document.visibilityState);";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetStateJS, &document_visibility_state));
    if (i == visible_index) {
      EXPECT_EQ("visible", document_visibility_state);
    } else {
      EXPECT_EQ("hidden", document_visibility_state);
    }
  }
}

void CreateTestTabs(Browser* browser) {
  GURL test_page(ui_test_utils::GetTestUrl(base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("tab-restore-visibilty.html"))));
  ui_test_utils::NavigateToURLWithDisposition(
      browser, test_page, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser, test_page, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
}

void CloseBrowser(Browser* browser) {
  content::WindowedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser));
  chrome::CloseWindow(browser);
  close_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(BrowserTabRestoreTest, RecentTabsMenuTabDisposition) {
  // Create tabs.
  CreateTestTabs(browser());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  BrowserList* active_browser_list =
      BrowserList::GetInstance(browser()->host_desktop_type());
  EXPECT_EQ(2u, active_browser_list->size());

  // Close the first browser.
  CloseBrowser(browser());
  EXPECT_EQ(1u, active_browser_list->size());

  // Restore tabs using the browser's recent tabs menu.
  content::DOMMessageQueue queue;
  Browser* browser = active_browser_list->get(0);
  RecentTabsSubMenuModel menu(NULL, browser, NULL);
  menu.ExecuteCommand(
      RecentTabsSubMenuModel::GetFirstRecentTabsCommandId(), 0);
  AwaitTabsReady(&queue, 2);

  // There should be 3 restored tabs in the new browser.
  EXPECT_EQ(2u, active_browser_list->size());
  browser = active_browser_list->get(1);
  EXPECT_EQ(3, browser->tab_strip_model()->count());

  // The middle tab only should have visible disposition.
  CheckVisbility(browser->tab_strip_model(), 1);
}

IN_PROC_BROWSER_TEST_F(BrowserTabRestoreTest, DelegateRestoreTabDisposition) {
  // Create tabs.
  CreateTestTabs(browser());
  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  BrowserList* active_browser_list =
      BrowserList::GetInstance(browser()->host_desktop_type());
  EXPECT_EQ(2u, active_browser_list->size());

  // Close the first browser.
  CloseBrowser(browser());
  EXPECT_EQ(1u, active_browser_list->size());

  // Check the browser has a delegated restore service.
  Browser* browser = active_browser_list->get(0);
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser->profile());
  bool has_tab_restore_service = !!service;
  ASSERT_TRUE(has_tab_restore_service);
  TabRestoreServiceDelegate* delegate =
      TabRestoreServiceDelegate::FindDelegateForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  bool has_tab_restore_delegate = !!delegate;
  ASSERT_TRUE(has_tab_restore_delegate);

  // Restore tabs using that delegated restore service.
  content::DOMMessageQueue queue;
  service->RestoreMostRecentEntry(
      delegate, browser->host_desktop_type());
  AwaitTabsReady(&queue, 2);

  // There should be 3 restored tabs in the new browser.
  EXPECT_EQ(2u, active_browser_list->size());
  browser = active_browser_list->get(1);
  EXPECT_EQ(3, browser->tab_strip_model()->count());

  // The middle tab only should have visible disposition.
  CheckVisbility(browser->tab_strip_model(), 1);
}
