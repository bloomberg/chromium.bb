// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/url_constants.h"

using content::WebContents;

// Test crashes on Aura due to initiator tab's native view having no parent.
// http://crbug.com/104284
#if defined(USE_AURA)
#define MAYBE_GetOrCreatePreviewDialog DISABLED_GetOrCreatePreviewDialog
#define MAYBE_MultiplePreviewDialogs DISABLED_MultiplePreviewDialogs
#define MAYBE_ClearInitiatorTabDetails DISABLED_ClearInitiatorTabDetails
#else
#define MAYBE_GetOrCreatePreviewDialog GetOrCreatePreviewDialog
#define MAYBE_MultiplePreviewDialogs MultiplePreviewDialogs
#define MAYBE_ClearInitiatorTabDetails ClearInitiatorTabDetails
#endif

namespace printing {

typedef PrintPreviewTest PrintPreviewDialogControllerUnitTest;

// Create/Get a preview dialog for initiator tab.
TEST_F(PrintPreviewDialogControllerUnitTest, MAYBE_GetOrCreatePreviewDialog) {
  // Lets start with one window with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator tab contents.
  WebContents* initiator_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Get the preview dialog for initiator tab.
  PrintViewManager::FromWebContents(initiator_tab)->PrintPreviewNow(false);
  WebContents* preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  // New print preview dialog is a constrained window, so the number of tabs is
  // still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(initiator_tab, preview_dialog);

  // Get the print preview dialog for the same initiator tab.
  WebContents* new_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  // Preview dialog already exists. Tab count remains the same.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // 1:1 relationship between initiator tab and preview dialog.
  EXPECT_EQ(new_preview_dialog, preview_dialog);
}

// Tests multiple print preview dialogs exist in the same browser for
// different initiator tabs. If a preview dialog already exists for an
// initiator tab, that initiator tab gets focused.
TEST_F(PrintPreviewDialogControllerUnitTest, MAYBE_MultiplePreviewDialogs) {
  // Lets start with one window and two tabs.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  EXPECT_EQ(0, tab_strip_model->count());

  // Create some new initiator tabs.
  chrome::NewTab(browser());
  WebContents* web_contents_1 = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_1);

  chrome::NewTab(browser());
  WebContents* web_contents_2 = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_2);
  EXPECT_EQ(2, tab_strip_model->count());

  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Create preview dialog for |web_contents_1|
  PrintViewManager::FromWebContents(web_contents_1)->PrintPreviewNow(false);
  WebContents* preview_dialog_1 =
      dialog_controller->GetOrCreatePreviewDialog(web_contents_1);

  EXPECT_NE(web_contents_1, preview_dialog_1);
  EXPECT_EQ(2, tab_strip_model->count());

  // Create preview dialog for |web_contents_2|
  PrintViewManager::FromWebContents(web_contents_2)->PrintPreviewNow(false);
  WebContents* preview_dialog_2 =
      dialog_controller->GetOrCreatePreviewDialog(web_contents_2);

  EXPECT_NE(web_contents_2, preview_dialog_2);
  EXPECT_NE(preview_dialog_1, preview_dialog_2);
  // 2 initiator tabs and 2 preview dialogs exist in the same browser.
  // The preview dialogs are constrained in their respective initiator tabs.
  EXPECT_EQ(2, tab_strip_model->count());

  int tab_1_index = tab_strip_model->GetIndexOfWebContents(web_contents_1);
  int tab_2_index = tab_strip_model->GetIndexOfWebContents(web_contents_2);
  int preview_dialog_1_index =
      tab_strip_model->GetIndexOfWebContents(preview_dialog_1);
  int preview_dialog_2_index =
      tab_strip_model->GetIndexOfWebContents(preview_dialog_2);

  // Constrained dialogs are not in the TabStripModel.
  EXPECT_EQ(-1, preview_dialog_1_index);
  EXPECT_EQ(-1, preview_dialog_2_index);

  // Since |preview_dialog_2_index| was the most recently created dialog, its
  // initiator tab should have focus.
  EXPECT_EQ(tab_2_index, tab_strip_model->active_index());

  // When we get the preview dialog for |web_contents_1|,
  // |preview_dialog_1| is activated and focused.
  dialog_controller->GetOrCreatePreviewDialog(web_contents_1);
  EXPECT_EQ(tab_1_index, tab_strip_model->active_index());
}

// Check clearing the initiator tab details associated with a print preview
// dialog allows the initiator tab to create another print preview dialog.
TEST_F(PrintPreviewDialogControllerUnitTest, MAYBE_ClearInitiatorTabDetails) {
  // Lets start with one window with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator tab contents.
  WebContents* initiator_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Get the preview dialog for the initiator tab.
  PrintViewManager::FromWebContents(initiator_tab)->PrintPreviewNow(false);
  WebContents* preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  // New print preview dialog is a constrained window, so the number of tabs is
  // still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(initiator_tab, preview_dialog);

  // Clear the initiator tab details associated with the preview dialog.
  dialog_controller->EraseInitiatorTabInfo(preview_dialog);

  // Get a new print preview dialog for the initiator tab.
  WebContents* new_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator_tab);

  // New print preview dialog is a constrained window, so the number of tabs is
  // still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  // Verify a new print preview dialog has been created.
  EXPECT_NE(new_preview_dialog, preview_dialog);
}

}  // namespace printing
