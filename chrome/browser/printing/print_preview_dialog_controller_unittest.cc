// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
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
#define MAYBE_GetOrCreatePreviewTab DISABLED_GetOrCreatePreviewTab
#define MAYBE_MultiplePreviewTabs DISABLED_MultiplePreviewTabs
#define MAYBE_ClearInitiatorTabDetails DISABLED_ClearInitiatorTabDetails
#else
#define MAYBE_GetOrCreatePreviewTab GetOrCreatePreviewTab
#define MAYBE_MultiplePreviewTabs MultiplePreviewTabs
#define MAYBE_ClearInitiatorTabDetails ClearInitiatorTabDetails
#endif

typedef PrintPreviewTest PrintPreviewDialogControllerUnitTest;

// Create/Get a preview tab for initiator tab.
TEST_F(PrintPreviewDialogControllerUnitTest, MAYBE_GetOrCreatePreviewTab) {
  // Lets start with one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator tab contents.
  WebContents* initiator_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  printing::PrintPreviewDialogController* tab_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  printing::PrintViewManager::FromWebContents(initiator_tab)->PrintPreviewNow();
  WebContents* preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(initiator_tab, preview_tab);

  // Get the print preview tab for initiator tab.
  WebContents* new_preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // Preview tab already exists. Tab count remains the same.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // 1:1 relationship between initiator and preview tab.
  EXPECT_EQ(new_preview_tab, preview_tab);
}

// To show multiple print preview tabs exist in the same browser for
// different initiator tabs. If preview tab already exists for an initiator, it
// gets focused.
TEST_F(PrintPreviewDialogControllerUnitTest, MAYBE_MultiplePreviewTabs) {
  // Lets start with one window and two tabs.
  EXPECT_EQ(1u, BrowserList::size());
  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_TRUE(model);

  EXPECT_EQ(0, model->count());

  chrome::NewTab(browser());
  WebContents* web_contents_1 = model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_1);

  chrome::NewTab(browser());
  WebContents* web_contents_2 = model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_2);
  EXPECT_EQ(2, model->count());

  printing::PrintPreviewDialogController* tab_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(tab_controller);

  // Create preview tab for |tab_contents_1|
  printing::PrintViewManager::FromWebContents(web_contents_1)->
      PrintPreviewNow();
  WebContents* preview_tab_1 =
      tab_controller->GetOrCreatePreviewTab(web_contents_1);

  EXPECT_NE(web_contents_1, preview_tab_1);
  EXPECT_EQ(2, model->count());

  // Create preview tab for |tab_contents_2|
  printing::PrintViewManager::FromWebContents(web_contents_2)->
      PrintPreviewNow();
  WebContents* preview_tab_2 =
      tab_controller->GetOrCreatePreviewTab(web_contents_2);

  EXPECT_NE(web_contents_2, preview_tab_2);
  // 2 initiator tab and 2 preview tabs exist in the same browser.
  // The preview tabs are constrained in their respective initiator tabs.
  EXPECT_EQ(2, model->count());

  int tab_1_index = model->GetIndexOfWebContents(web_contents_1);
  int tab_2_index = model->GetIndexOfWebContents(web_contents_2);
  int preview_tab_1_index = model->GetIndexOfWebContents(preview_tab_1);
  int preview_tab_2_index = model->GetIndexOfWebContents(preview_tab_2);

  EXPECT_EQ(-1, preview_tab_1_index);
  EXPECT_EQ(-1, preview_tab_2_index);
  EXPECT_EQ(tab_2_index, model->active_index());

  // When we get the preview tab for |tab_contents_1|,
  // |preview_tab_1| is activated and focused.
  tab_controller->GetOrCreatePreviewTab(web_contents_1);
  EXPECT_EQ(tab_1_index, model->active_index());
}

// Clear the initiator tab details associated with preview tab.
TEST_F(PrintPreviewDialogControllerUnitTest, MAYBE_ClearInitiatorTabDetails) {
  // Lets start with one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator tab contents.
  WebContents* initiator_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  printing::PrintPreviewDialogController* tab_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  printing::PrintViewManager::FromWebContents(initiator_tab)->PrintPreviewNow();
  WebContents* preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created. Current focus is on preview tab.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(initiator_tab, preview_tab);

  // Clear the initiator tab details associated with the preview tab.
  tab_controller->EraseInitiatorTabInfo(preview_tab);

  // Get the print preview tab for initiator tab.
  WebContents* new_preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New preview tab is created.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(new_preview_tab, preview_tab);
}
