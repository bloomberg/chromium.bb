// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

namespace {

typedef InProcessBrowserTest PrintPreviewTabControllerBrowserTest;

// Test to verify that when a preview tab navigates, we can create a new print
// preview tab for both initiator tab and new preview tab contents.
IN_PROC_BROWSER_TEST_F(PrintPreviewTabControllerBrowserTest,
                       NavigateFromPrintPreviewTab) {
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

  scoped_refptr<printing::PrintPreviewTabController>
     tab_controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  TabContentsWrapper* preview_tab =
    tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created. Current focus is on preview tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_NE(initiator_tab, preview_tab);

  GURL url(chrome::kAboutBlankURL);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(url, preview_tab->tab_contents()->GetURL());

  // Get the print preview tab for initiator tab.
  TabContentsWrapper* new_preview_tab =
     tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New preview tab is created.
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_NE(new_preview_tab, preview_tab);

  // Get the print preview tab for old preview tab.
  TabContentsWrapper* newest_preview_tab =
  tab_controller->GetOrCreatePreviewTab(preview_tab);

  // Newest preview tab is created and the previously created preview tab is not
  // merely activated.
  EXPECT_EQ(4, browser()->tab_count());
  EXPECT_NE(newest_preview_tab, new_preview_tab);
}

// Test to verify that when a initiator tab navigates, we can create a new
// preview tab for the new tab contents. But we cannot create a preview tab for
// the old preview tab.
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

  scoped_refptr<printing::PrintPreviewTabController>
     tab_controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  TabContentsWrapper* preview_tab =
    tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created. Current focus is on preview tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_NE(initiator_tab, preview_tab);

  // Activate initiator tab.
  browser()->ActivateTabAt(0, true);
  GURL url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), url);

  // Get the print preview tab for initiator tab.
  TabContentsWrapper* new_preview_tab =
     tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New preview tab is created.
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_NE(new_preview_tab, preview_tab);

  // Get the print preview tab for old preview tab.
  TabContentsWrapper* newest_preview_tab =
  tab_controller->GetOrCreatePreviewTab(preview_tab);

  // Make sure preview tab is not created for |preview_tab|.
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(newest_preview_tab, preview_tab);
}

// Test to verify that even after reloading initiator tab and preview tab,
// their association exists.
IN_PROC_BROWSER_TEST_F(PrintPreviewTabControllerBrowserTest,
                       ReloadInitiatorTabAndPreviewTab) {
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

  scoped_refptr<printing::PrintPreviewTabController>
     tab_controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(tab_controller);

  // Get the preview tab for initiator tab.
  TabContentsWrapper* preview_tab =
    tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // New print preview tab is created. Current focus is on preview tab.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_NE(initiator_tab, preview_tab);

  // Activate initiator tab and reload.
  browser()->ActivateTabAt(0, true);
  browser()->Reload(CURRENT_TAB);

  // Get the print preview tab for initiator tab.
  TabContentsWrapper* new_preview_tab =
     tab_controller->GetOrCreatePreviewTab(initiator_tab);

  // Old preview tab is activated.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(new_preview_tab, preview_tab);

  // Reload preview tab.
  browser()->Reload(CURRENT_TAB);
  // Get the print preview tab for old preview tab.
  TabContentsWrapper* newest_preview_tab =
  tab_controller->GetOrCreatePreviewTab(preview_tab);

  // Make sure new preview tab is not created for |preview_tab|.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(newest_preview_tab, preview_tab);
}

// Test that print preview tabs are placed correctly.
IN_PROC_BROWSER_TEST_F(PrintPreviewTabControllerBrowserTest,
                       OpenPreviewTabInCorrectPosition) {
  const int kTabCount = 4;
  // Create kTabCount - 1 tabs since we start with 1 tab already.
  for (int i = 0; i < kTabCount - 1; ++i) {
    browser::NavigateParams p(browser(), GURL(), PageTransition::LINK);
    p.disposition = NEW_FOREGROUND_TAB;
    browser::Navigate(&p);
  }
  EXPECT_EQ(kTabCount, browser()->tab_count());

  // Create a print preview tab.
  scoped_refptr<printing::PrintPreviewTabController>
     tab_controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(tab_controller);

  const int kInitiatorTabIndex = 1;
  TabContentsWrapper* initiator_tab =
      browser()->GetTabContentsWrapperAt(kInitiatorTabIndex);
  ASSERT_TRUE(initiator_tab);
  TabContentsWrapper* preview_tab =
    tab_controller->GetOrCreatePreviewTab(initiator_tab);
  EXPECT_TRUE(preview_tab);

  // Check the preview tab's location.
  EXPECT_EQ(preview_tab,
            browser()->GetTabContentsWrapperAt(kInitiatorTabIndex + 1));
  EXPECT_EQ(preview_tab, browser()->GetSelectedTabContentsWrapper());
}

// Test that print preview tabs created by pop-up windows are placed correctly.
IN_PROC_BROWSER_TEST_F(PrintPreviewTabControllerBrowserTest,
                       OpenPreviewTabFromPopupInCorrectPosition) {
  const int kTabCount = 4;
  // Create kTabCount - 1 tabs since we start with 1 tab already.
  for (int i = 0; i < kTabCount - 1; ++i) {
    browser::NavigateParams p(browser(), GURL(), PageTransition::LINK);
    p.disposition = NEW_FOREGROUND_TAB;
    browser::Navigate(&p);
  }
  EXPECT_EQ(kTabCount, browser()->tab_count());

  // Create a popup
  browser::NavigateParams p(browser(), GURL(), PageTransition::LINK);
  p.disposition = NEW_POPUP;
  ui_test_utils::NavigateToURL(&p);

  // Navigate() should have opened a new popup window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(Browser::TYPE_POPUP, p.browser->type());
  ASSERT_TRUE(p.target_contents);

  // Create a print preview tab.
  scoped_refptr<printing::PrintPreviewTabController>
     tab_controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(tab_controller);

  TabContentsWrapper* preview_tab =
    tab_controller->GetOrCreatePreviewTab(p.target_contents);
  EXPECT_TRUE(preview_tab);

  // Check the preview tab's location.
  EXPECT_EQ(preview_tab, browser()->GetTabContentsWrapperAt(kTabCount));
  EXPECT_EQ(preview_tab, browser()->GetSelectedTabContentsWrapper());
}

}  // namespace
