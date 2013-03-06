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
// preview dialog for the new tab contents.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       NavigateFromInitiatorTab) {
  // Create a reference to initiator tab contents.
  WebContents* initiator_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(initiator_tab);

  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Get the preview dialog for the initiator tab.
  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator_tab);
  print_view_manager->PrintPreviewNow(false);
  WebContents* preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  // Check a new print preview dialog got created.
  ASSERT_TRUE(preview_dialog);
  ASSERT_NE(initiator_tab, preview_dialog);

  // Navigate in the initiator tab. Make sure navigating destroys the print
  // preview dialog.
  PrintPreviewDialogDestroyedObserver observer(preview_dialog);
  GURL url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(observer.dialog_destroyed());

  // Get the print preview dialog for the initiator tab.
  print_view_manager->PrintPreviewNow(false);
  WebContents* new_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  // Check a new preview dialog got created.
  EXPECT_TRUE(new_preview_dialog);
}

// Test to verify that after reloading the initiator tab, it creates a new
// print preview dialog.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       ReloadInitiatorTab) {
  // Create a reference to initiator tab contents.
  WebContents* initiator_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(initiator_tab);

  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Create a preview dialog for the initiator tab.
  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator_tab);
  print_view_manager->PrintPreviewNow(false);
  WebContents* preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  // Check a new print preview dialog got created.
  ASSERT_TRUE(preview_dialog);
  ASSERT_NE(initiator_tab, preview_dialog);

  // Reload the initiator tab. Make sure reloading destroys the print preview
  // dialog.
  PrintPreviewDialogDestroyedObserver dialog_destroyed_observer(preview_dialog);
  content::WindowedNotificationObserver notification_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::Reload(browser(), CURRENT_TAB);
  notification_observer.Wait();
  ASSERT_TRUE(dialog_destroyed_observer.dialog_destroyed());

  // Create a preview dialog for the initiator tab.
  print_view_manager->PrintPreviewNow(false);
  WebContents* new_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  EXPECT_TRUE(new_preview_dialog);
}
