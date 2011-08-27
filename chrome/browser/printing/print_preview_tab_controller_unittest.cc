// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/url_constants.h"

typedef BrowserWithTestWindowTest PrintPreviewTabControllerUnitTest;

// Create/Get a preview tab for initiator tab.
TEST_F(PrintPreviewTabControllerUnitTest, GetOrCreatePreviewTab) {
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  // Lets start with one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_count());
  browser()->NewTab();
  EXPECT_EQ(1, browser()->tab_count());

  // Create a reference to initiator tab contents.
  TabContents* initiator_tab = browser()->GetSelectedTabContents();

  scoped_refptr<printing::PrintPreviewTabController>
      tab_controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  TabContents* preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created. Current focus is on preview tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_NE(initiator_tab, preview_tab);

  // Get the print preview tab for initiator tab.
  TabContents* new_preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // Preview tab already exists. Tab count remains the same.
  EXPECT_EQ(2, browser()->tab_count());

  // 1:1 relationship between initiator and preview tab.
  EXPECT_EQ(new_preview_tab, preview_tab);
}

// Test to verify the initiator tab title is stored in |PrintPreviewUI| after
// preview tab reload.
TEST_F(PrintPreviewTabControllerUnitTest, TitleAfterReload) {
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  // Lets start with one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_count());
  browser()->NewTab();
  EXPECT_EQ(1, browser()->tab_count());

  // Create a reference to initiator tab contents.
  TabContents* initiator_tab = browser()->GetSelectedTabContents();

  scoped_refptr<printing::PrintPreviewTabController>
      tab_controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  TabContents* preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created. Current focus is on preview tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_NE(initiator_tab, preview_tab);

  // Set up a PrintPreviewUI for |preview_tab|.
  PrintPreviewUI* preview_ui = new PrintPreviewUI(preview_tab);
  // RenderViewHostManager takes ownership of |preview_ui|.
  preview_tab->render_manager()->SetWebUIPostCommit(preview_ui);

  // Simulate a reload event on |preview_tab|.
  scoped_ptr<NavigationEntry> entry;
  entry.reset(new NavigationEntry());
  entry->set_transition_type(PageTransition::RELOAD);
  content::LoadCommittedDetails details;
  details.type = NavigationType::SAME_PAGE;
  details.entry = entry.get();
  NotificationService::current()->Notify(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      Source<NavigationController>(&preview_tab->controller()),
      Details<content::LoadCommittedDetails>(&details));
  EXPECT_EQ(initiator_tab->GetTitle(), preview_ui->initiator_tab_title_);
}

// To show multiple print preview tabs exist in the same browser for
// different initiator tabs. If preview tab already exists for an initiator, it
// gets focused.
TEST_F(PrintPreviewTabControllerUnitTest, MultiplePreviewTabs) {
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  // Lets start with one window and two tabs.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_count());

  browser()->NewTab();
  TabContents* tab_contents_1 = browser()->GetSelectedTabContents();
  ASSERT_TRUE(tab_contents_1);

  browser()->NewTab();
  TabContents* tab_contents_2 = browser()->GetSelectedTabContents();
  ASSERT_TRUE(tab_contents_2);
  EXPECT_EQ(2, browser()->tab_count());

  scoped_refptr<printing::PrintPreviewTabController>
      tab_controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(tab_controller);

  // Create preview tab for |tab_contents_1|
  TabContents* preview_tab_1 =
      tab_controller->GetOrCreatePreviewTab(tab_contents_1);

  EXPECT_NE(tab_contents_1, preview_tab_1);
  EXPECT_EQ(3, browser()->tab_count());

  // Create preview tab for |tab_contents_2|
  TabContents* preview_tab_2 =
      tab_controller->GetOrCreatePreviewTab(tab_contents_2);

  EXPECT_NE(tab_contents_2, preview_tab_2);
  // 2 initiator tab and 2 preview tabs exist in the same browser.
  EXPECT_EQ(4, browser()->tab_count());

  TabStripModel* model = browser()->tabstrip_model();
  ASSERT_TRUE(model);

  int preview_tab_1_index = model->GetWrapperIndex(preview_tab_1);
  int preview_tab_2_index = model->GetWrapperIndex(preview_tab_2);

  EXPECT_NE(-1, preview_tab_1_index);
  EXPECT_NE(-1, preview_tab_2_index);
  // Current tab is |preview_tab_2|.
  EXPECT_EQ(preview_tab_2_index, browser()->active_index());

  // When we get the preview tab for |tab_contents_1|,
  // |preview_tab_1| is activated and focused.
  tab_controller->GetOrCreatePreviewTab(tab_contents_1);
  EXPECT_EQ(preview_tab_1_index, browser()->active_index());
}

// Clear the initiator tab details associated with preview tab.
TEST_F(PrintPreviewTabControllerUnitTest, ClearInitiatorTabDetails) {
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  // Lets start with one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_count());
  browser()->NewTab();
  EXPECT_EQ(1, browser()->tab_count());

  // Create a reference to initiator tab contents.
  TabContents* initiator_tab = browser()->GetSelectedTabContents();

  scoped_refptr<printing::PrintPreviewTabController>
      tab_controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  TabContents* preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created. Current focus is on preview tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_NE(initiator_tab, preview_tab);

  // Clear the initiator tab details associated with the preview tab.
  tab_controller->EraseInitiatorTabInfo(preview_tab);

  // Get the print preview tab for initiator tab.
  TabContents* new_preview_tab =
      tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New preview tab is created.
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_NE(new_preview_tab, preview_tab);
}
