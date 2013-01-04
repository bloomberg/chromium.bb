// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#endif

typedef BrowserWithTestWindowTest BrowserCommandsTest;

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

// Tests IDC_SELECT_TAB_0, IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB and
// IDC_SELECT_LAST_TAB.
TEST_F(BrowserCommandsTest, TabNavigationAccelerators) {
  GURL about_blank(chrome::kAboutBlankURL);

  // Create three tabs.
  AddTab(browser(), about_blank);
  AddTab(browser(), about_blank);
  AddTab(browser(), about_blank);

  // Select the second tab.
  browser()->tab_strip_model()->ActivateTabAt(1, false);

  CommandUpdater* updater = browser()->command_controller()->command_updater();

  // Navigate to the first tab using an accelerator.
  updater->ExecuteCommand(IDC_SELECT_TAB_0);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  // Navigate to the second tab using the next accelerators.
  updater->ExecuteCommand(IDC_SELECT_NEXT_TAB);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());

  // Navigate back to the first tab using the previous accelerators.
  updater->ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  // Navigate to the last tab using the select last accelerator.
  updater->ExecuteCommand(IDC_SELECT_LAST_TAB);
  ASSERT_EQ(2, browser()->tab_strip_model()->active_index());
}

// Tests IDC_DUPLICATE_TAB.
TEST_F(BrowserCommandsTest, DuplicateTab) {
  GURL url1("http://foo/1");
  GURL url2("http://foo/2");
  GURL url3("http://foo/3");
  GURL url4("http://foo/4");

  // Navigate to three urls, plus a pending URL that hasn't committed.
  AddTab(browser(), url1);
  NavigateAndCommitActiveTab(url2);
  NavigateAndCommitActiveTab(url3);
  content::NavigationController& orig_controller =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  orig_controller.LoadURL(
      url4, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
  EXPECT_EQ(3, orig_controller.GetEntryCount());
  EXPECT_TRUE(orig_controller.GetPendingEntry());

  size_t initial_window_count = BrowserList::size();

  // Duplicate the tab.
  chrome::ExecuteCommand(browser(), IDC_DUPLICATE_TAB);

  // The duplicated tab should not end up in a new window.
  size_t window_count = BrowserList::size();
  ASSERT_EQ(initial_window_count, window_count);

  // And we should have a newly duplicated tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Verify the stack of urls.
  content::NavigationController& controller =
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetController();
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(2)->GetURL());
  EXPECT_FALSE(controller.GetPendingEntry());
}

// Tests IDC_VIEW_SOURCE (See http://crbug.com/138140).
TEST_F(BrowserCommandsTest, ViewSource) {
  GURL url1("http://foo/1");
  GURL url2("http://foo/2");

  // Navigate to a URL, plus a pending URL that hasn't committed.
  AddTab(browser(), url1);
  content::NavigationController& orig_controller =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  orig_controller.LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
  EXPECT_EQ(1, orig_controller.GetEntryCount());
  EXPECT_TRUE(orig_controller.GetPendingEntry());

  size_t initial_window_count = BrowserList::size();

  // View Source.
  chrome::ExecuteCommand(browser(), IDC_VIEW_SOURCE);

  // The view source tab should not end up in a new window.
  size_t window_count = BrowserList::size();
  ASSERT_EQ(initial_window_count, window_count);

  // And we should have a newly duplicated tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Verify we are viewing the source of the last committed entry.
  GURL view_source_url("view-source:http://foo/1");
  content::NavigationController& controller =
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetController();
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(view_source_url, controller.GetEntryAtIndex(0)->GetVirtualURL());
  EXPECT_FALSE(controller.GetPendingEntry());
}

TEST_F(BrowserCommandsTest, BookmarkCurrentPage) {
  // We use profile() here, since it's a TestingProfile.
  profile()->CreateBookmarkModel(true);
  profile()->BlockUntilBookmarkModelLoaded();

  // Navigate to a url.
  GURL url1("http://foo/1");
  AddTab(browser(), url1);
  browser()->OpenURL(OpenURLParams(
      url1, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));

  chrome::BookmarkCurrentPage(browser());

  // It should now be bookmarked in the bookmark model.
  EXPECT_EQ(profile(), browser()->profile());
  EXPECT_TRUE(BookmarkModelFactory::GetForProfile(
      browser()->profile())->IsBookmarked(url1));
}

// Tests back/forward in new tab (Control + Back/Forward button in the UI).
TEST_F(BrowserCommandsTest, BackForwardInNewTab) {
  GURL url1("http://foo/1");
  GURL url2("http://foo/2");

  // Make a tab with the two pages navigated in it.
  AddTab(browser(), url1);
  NavigateAndCommitActiveTab(url2);

  // Go back in a new background tab.
  chrome::GoBack(browser(), NEW_BACKGROUND_TAB);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // The original tab should be unchanged.
  WebContents* zeroth = browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(url2, zeroth->GetURL());
  EXPECT_TRUE(zeroth->GetController().CanGoBack());
  EXPECT_FALSE(zeroth->GetController().CanGoForward());

  // The new tab should be like the first one but navigated back.
  WebContents* first = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(url1, browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
  EXPECT_FALSE(first->GetController().CanGoBack());
  EXPECT_TRUE(first->GetController().CanGoForward());

  // Select the second tab and make it go forward in a new background tab.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  // TODO(brettw) bug 11055: It should not be necessary to commit the load here,
  // but because of this bug, it will assert later if we don't. When the bug is
  // fixed, one of the three commits here related to this bug should be removed
  // (to test both codepaths).
  CommitPendingLoad(&first->GetController());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  chrome::GoForward(browser(), NEW_BACKGROUND_TAB);

  // The previous tab should be unchanged and still in the foreground.
  EXPECT_EQ(url1, first->GetURL());
  EXPECT_FALSE(first->GetController().CanGoBack());
  EXPECT_TRUE(first->GetController().CanGoForward());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // There should be a new tab navigated forward.
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  WebContents* second = browser()->tab_strip_model()->GetWebContentsAt(2);
  EXPECT_EQ(url2, second->GetURL());
  EXPECT_TRUE(second->GetController().CanGoBack());
  EXPECT_FALSE(second->GetController().CanGoForward());

  // Now do back in a new foreground tab. Don't bother re-checking every sngle
  // thing above, just validate that it's opening properly.
  browser()->tab_strip_model()->ActivateTabAt(2, true);
  // TODO(brettw) bug 11055: see the comment above about why we need this.
  CommitPendingLoad(&second->GetController());
  chrome::GoBack(browser(), NEW_FOREGROUND_TAB);
  ASSERT_EQ(3, browser()->tab_strip_model()->active_index());
  ASSERT_EQ(url1,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Same thing again for forward.
  // TODO(brettw) bug 11055: see the comment above about why we need this.
  CommitPendingLoad(&
      browser()->tab_strip_model()->GetActiveWebContents()->GetController());
  chrome::GoForward(browser(), NEW_FOREGROUND_TAB);
  ASSERT_EQ(4, browser()->tab_strip_model()->active_index());
  ASSERT_EQ(url2,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

