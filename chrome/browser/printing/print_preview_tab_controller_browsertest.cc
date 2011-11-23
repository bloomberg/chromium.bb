// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"

namespace {

class PrintPreviewTabControllerBrowserTest : public InProcessBrowserTest {
 public:
  PrintPreviewTabControllerBrowserTest() {}
  virtual ~PrintPreviewTabControllerBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnablePrintPreview);
  }
};

class TabDestroyedObserver : public TabContentsObserver {
 public:
  explicit TabDestroyedObserver(TabContents* contents)
      : TabContentsObserver(contents),
        tab_destroyed_(false) {
  }
  virtual ~TabDestroyedObserver() {}

  bool tab_destroyed() { return tab_destroyed_; }

 private:
  virtual void TabContentsDestroyed(TabContents* tab) {
    tab_destroyed_ = true;
  }

  bool tab_destroyed_;
};

// Test to verify that when a initiator tab navigates, we can create a new
// preview tab for the new tab contents.
IN_PROC_BROWSER_TEST_F(PrintPreviewTabControllerBrowserTest,
                       NavigateFromInitiatorTab) {
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  // Lets start with one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());

  // Create a reference to initiator tab contents.
  TabContentsWrapper* initiator_tab =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(initiator_tab);

  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* preview_tab =
    tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created.
  EXPECT_EQ(1, browser()->tab_count());
  ASSERT_TRUE(preview_tab);
  ASSERT_NE(initiator_tab, preview_tab);
  TabDestroyedObserver observer(preview_tab->tab_contents());

  // Navigate in the initiator tab.
  GURL url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_TRUE(observer.tab_destroyed());

  // Get the print preview tab for initiator tab.
  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* new_preview_tab =
     tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New preview tab is created.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_TRUE(new_preview_tab);
}

// Test to verify that after reloading the initiator tab, it creates a new
// print preview tab.
IN_PROC_BROWSER_TEST_F(PrintPreviewTabControllerBrowserTest,
                       ReloadInitiatorTab) {
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  // Lets start with one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());

  // Create a reference to initiator tab contents.
  TabContentsWrapper* initiator_tab =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(initiator_tab);

  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* preview_tab =
    tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created.
  EXPECT_EQ(1, browser()->tab_count());
  ASSERT_TRUE(preview_tab);
  ASSERT_NE(initiator_tab, preview_tab);
  TabDestroyedObserver tab_destroyed_observer(preview_tab->tab_contents());

  // Reload the initiator tab.
  ui_test_utils::WindowedNotificationObserver notification_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  browser()->Reload(CURRENT_TAB);
  notification_observer.Wait();

  ASSERT_TRUE(tab_destroyed_observer.tab_destroyed());

  // Get the print preview tab for initiator tab.
  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* new_preview_tab =
     tab_controller->GetOrCreatePreviewTab(initiator_tab);

  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_TRUE(new_preview_tab);
}

}  // namespace
