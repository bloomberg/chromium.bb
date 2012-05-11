// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_preview_unit_test_base.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/url_constants.h"

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

class PrintPreviewTabControllerUnitTest : public PrintPreviewUnitTestBase {
 public:
  PrintPreviewTabControllerUnitTest() {}
  virtual ~PrintPreviewTabControllerUnitTest() {}
};

// Create/Get a preview tab for initiator tab.
TEST_F(PrintPreviewTabControllerUnitTest, MAYBE_GetOrCreatePreviewTab) {
  // Lets start with one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_count());
  browser()->NewTab();
  EXPECT_EQ(1, browser()->tab_count());

  // Create a reference to initiator tab contents.
  TabContentsWrapper* initiator_tab =
      browser()->GetSelectedTabContentsWrapper();

  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_NE(initiator_tab, preview_tab);

  // Get the print preview tab for initiator tab.
  TabContentsWrapper* new_preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // Preview tab already exists. Tab count remains the same.
  EXPECT_EQ(1, browser()->tab_count());

  // 1:1 relationship between initiator and preview tab.
  EXPECT_EQ(new_preview_tab, preview_tab);
}

// To show multiple print preview tabs exist in the same browser for
// different initiator tabs. If preview tab already exists for an initiator, it
// gets focused.
TEST_F(PrintPreviewTabControllerUnitTest, MAYBE_MultiplePreviewTabs) {
  // Lets start with one window and two tabs.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_count());

  browser()->NewTab();
  TabContentsWrapper* tab_contents_1 =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(tab_contents_1);

  browser()->NewTab();
  TabContentsWrapper* tab_contents_2 =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(tab_contents_2);
  EXPECT_EQ(2, browser()->tab_count());

  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(tab_controller);

  // Create preview tab for |tab_contents_1|
  tab_contents_1->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* preview_tab_1 =
      tab_controller->GetOrCreatePreviewTab(tab_contents_1);

  EXPECT_NE(tab_contents_1, preview_tab_1);
  EXPECT_EQ(2, browser()->tab_count());

  // Create preview tab for |tab_contents_2|
  tab_contents_2->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* preview_tab_2 =
      tab_controller->GetOrCreatePreviewTab(tab_contents_2);

  EXPECT_NE(tab_contents_2, preview_tab_2);
  // 2 initiator tab and 2 preview tabs exist in the same browser.
  // The preview tabs are constrained in their respective initiator tabs.
  EXPECT_EQ(2, browser()->tab_count());

  TabStripModel* model = browser()->tabstrip_model();
  ASSERT_TRUE(model);

  int tab_1_index = model->GetIndexOfTabContents(tab_contents_1);
  int tab_2_index = model->GetIndexOfTabContents(tab_contents_2);
  int preview_tab_1_index = model->GetIndexOfTabContents(preview_tab_1);
  int preview_tab_2_index = model->GetIndexOfTabContents(preview_tab_2);

  EXPECT_EQ(-1, preview_tab_1_index);
  EXPECT_EQ(-1, preview_tab_2_index);
  EXPECT_EQ(tab_2_index, browser()->active_index());

  // When we get the preview tab for |tab_contents_1|,
  // |preview_tab_1| is activated and focused.
  tab_controller->GetOrCreatePreviewTab(tab_contents_1);
  EXPECT_EQ(tab_1_index, browser()->active_index());
}

// Clear the initiator tab details associated with preview tab.
TEST_F(PrintPreviewTabControllerUnitTest, MAYBE_ClearInitiatorTabDetails) {
  // Lets start with one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_count());
  browser()->NewTab();
  EXPECT_EQ(1, browser()->tab_count());

  // Create a reference to initiator tab contents.
  TabContentsWrapper* initiator_tab =
      browser()->GetSelectedTabContentsWrapper();

  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created. Current focus is on preview tab.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_NE(initiator_tab, preview_tab);

  // Clear the initiator tab details associated with the preview tab.
  tab_controller->EraseInitiatorTabInfo(preview_tab);

  // Get the print preview tab for initiator tab.
  TabContentsWrapper* new_preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New preview tab is created.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_NE(new_preview_tab, preview_tab);
}
