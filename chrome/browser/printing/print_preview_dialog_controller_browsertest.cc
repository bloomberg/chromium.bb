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

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
#if !defined(GOOGLE_CHROME_BUILD)
    command_line->AppendSwitch(switches::kEnablePrintPreview);
#endif
  }
};

class PrintPreviewDialogDestroyedObserver
    : public content::WebContentsObserver {
 public:
  explicit PrintPreviewDialogDestroyedObserver(WebContents* dialog)
      : content::WebContentsObserver(dialog),
        dialog_destroyed_(false) {
  }
  virtual ~PrintPreviewDialogDestroyedObserver() {}

  bool dialog_destroyed() { return dialog_destroyed_; }

 private:
  virtual void WebContentsDestroyed(WebContents* contents) OVERRIDE {
    dialog_destroyed_ = true;
  }

  bool dialog_destroyed_;
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

  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Get the preview tab for initiator tab.
  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator_tab);
  print_view_manager->PrintPreviewNow(false);
  WebContents* preview_tab =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  // New print preview tab is created.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(preview_tab);
  ASSERT_NE(initiator_tab, preview_tab);
  PrintPreviewDialogDestroyedObserver observer(preview_tab);

  // Navigate in the initiator tab.
  GURL url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_TRUE(observer.dialog_destroyed());

  // Get the print preview tab for initiator tab.
  print_view_manager->PrintPreviewNow(false);
  WebContents* new_preview_tab =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

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

  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Get the preview tab for initiator tab.
  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator_tab);
  print_view_manager->PrintPreviewNow(false);
  WebContents* preview_tab =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  // New print preview tab is created.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(preview_tab);
  ASSERT_NE(initiator_tab, preview_tab);
  PrintPreviewDialogDestroyedObserver dialog_destroyed_observer(preview_tab);

  // Reload the initiator tab.
  content::WindowedNotificationObserver notification_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::Reload(browser(), CURRENT_TAB);
  notification_observer.Wait();

  ASSERT_TRUE(dialog_destroyed_observer.dialog_destroyed());

  // Get the print preview tab for initiator tab.
  print_view_manager->PrintPreviewNow(false);
  WebContents* new_preview_tab =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_TRUE(new_preview_tab);
}
