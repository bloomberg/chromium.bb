// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "googleurl/src/gurl.h"

using content::WebContents;

class PrintPreviewDialogControllerBrowserTest : public InProcessBrowserTest {
 public:
  PrintPreviewDialogControllerBrowserTest() {}
  virtual ~PrintPreviewDialogControllerBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
#if !defined(GOOGLE_CHROME_BUILD)
    command_line->AppendSwitch(switches::kEnablePrintPreview);
#endif
  }
};

class TabDestroyedObserver : public content::WebContentsObserver {
 public:
  explicit TabDestroyedObserver(WebContents* contents)
      : content::WebContentsObserver(contents),
        tab_destroyed_(false) {
  }
  virtual ~TabDestroyedObserver() {}

  bool tab_destroyed() { return tab_destroyed_; }

 private:
  virtual void WebContentsDestroyed(WebContents* tab) OVERRIDE {
    tab_destroyed_ = true;
  }

  bool tab_destroyed_;
};

// Test to verify that when a initiator tab navigates, we can create a new
// preview tab for the new tab contents.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       NavigateFromInitiatorTab) {
  // Lets start with one tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator tab contents.
  WebContents* initiator_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(initiator_tab);

  printing::PrintPreviewDialogController* tab_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator_tab);
  print_view_manager->PrintPreviewNow();
  WebContents* preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(preview_tab);
  ASSERT_NE(initiator_tab, preview_tab);
  TabDestroyedObserver observer(preview_tab);

  // Navigate in the initiator tab.
  GURL url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_TRUE(observer.tab_destroyed());

  // Get the print preview tab for initiator tab.
  print_view_manager->PrintPreviewNow();
  WebContents* new_preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New preview tab is created.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(new_preview_tab);
}

// Test to verify that after reloading the initiator tab, it creates a new
// print preview tab.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       ReloadInitiatorTab) {
  // Lets start with one tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator tab contents.
  WebContents* initiator_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(initiator_tab);

  printing::PrintPreviewDialogController* tab_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator_tab);
  print_view_manager->PrintPreviewNow();
  WebContents* preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(preview_tab);
  ASSERT_NE(initiator_tab, preview_tab);
  TabDestroyedObserver tab_destroyed_observer(preview_tab);

  // Reload the initiator tab.
  content::WindowedNotificationObserver notification_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::Reload(browser(), CURRENT_TAB);
  notification_observer.Wait();

  ASSERT_TRUE(tab_destroyed_observer.tab_destroyed());

  // Get the print preview tab for initiator tab.
  print_view_manager->PrintPreviewNow();
  WebContents* new_preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(new_preview_tab);
}
