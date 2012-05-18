// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/test/test_renderer_host.h"

typedef BrowserWithTestWindowTest BrowserListTest;

namespace {

// Helper function to iterate and count all the tabs.
size_t CountAllTabs() {
  size_t count = 0;
  for (TabContentsIterator iterator; !iterator.done(); ++iterator)
    ++count;
  return count;
}

// Helper function to navigate to the print preview page.
void NavigateToPrintUrl(TabContentsWrapper* tab, int page_id) {
  content::RenderViewHostTester::For(
      tab->web_contents()->GetRenderViewHost())->SendNavigate(
          page_id, GURL(chrome::kChromeUIPrintURL));
}

}  // namespace

TEST_F(BrowserListTest, TabContentsIteratorVerifyCount) {
  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, BrowserList::size());

  EXPECT_EQ(0U, CountAllTabs());

  // Create more browsers/windows.
  scoped_ptr<Browser> browser2(new Browser(Browser::TYPE_TABBED, profile()));
  scoped_ptr<Browser> browser3(new Browser(Browser::TYPE_TABBED, profile()));
  scoped_ptr<Browser> browser4(new Browser(Browser::TYPE_TABBED, profile()));

  scoped_ptr<TestBrowserWindow> window2(new TestBrowserWindow(browser2.get()));
  scoped_ptr<TestBrowserWindow> window3(new TestBrowserWindow(browser3.get()));
  scoped_ptr<TestBrowserWindow> window4(new TestBrowserWindow(browser4.get()));

  browser2->SetWindowForTesting(window2.get());
  browser3->SetWindowForTesting(window3.get());
  browser4->SetWindowForTesting(window4.get());

  // Sanity checks.
  EXPECT_EQ(4U, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_count());
  EXPECT_EQ(0, browser2->tab_count());
  EXPECT_EQ(0, browser3->tab_count());
  EXPECT_EQ(0, browser4->tab_count());

  EXPECT_EQ(0U, CountAllTabs());

  // Add some tabs.
  for (size_t i = 0; i < 3; ++i)
    browser2->NewTab();
  browser3->NewTab();

  EXPECT_EQ(4U, CountAllTabs());

  // Close some tabs.
  browser2->CloseAllTabs();

  EXPECT_EQ(1U, CountAllTabs());

  // Add lots of tabs.
  for (size_t i = 0; i < 41; ++i)
    browser()->NewTab();

  EXPECT_EQ(42U, CountAllTabs());
  // Close all remaining tabs to keep all the destructors happy.
  browser3->CloseAllTabs();
}

TEST_F(BrowserListTest, TabContentsIteratorVerifyBrowser) {
  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, BrowserList::size());

  // Create more browsers/windows.
  scoped_ptr<Browser> browser2(new Browser(Browser::TYPE_TABBED, profile()));
  scoped_ptr<Browser> browser3(new Browser(Browser::TYPE_TABBED, profile()));

  scoped_ptr<TestBrowserWindow> window2(new TestBrowserWindow(browser2.get()));
  scoped_ptr<TestBrowserWindow> window3(new TestBrowserWindow(browser3.get()));

  browser2->SetWindowForTesting(window2.get());
  browser3->SetWindowForTesting(window3.get());

  // Sanity checks.
  EXPECT_EQ(3U, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_count());
  EXPECT_EQ(0, browser2->tab_count());
  EXPECT_EQ(0, browser3->tab_count());

  EXPECT_EQ(0U, CountAllTabs());

  // Add some tabs.
  for (size_t i = 0; i < 3; ++i)
    browser2->NewTab();
  browser3->NewTab();

  size_t count = 0;
  for (TabContentsIterator iterator; !iterator.done(); ++iterator, ++count) {
    if (count < 3)
      EXPECT_EQ(browser2.get(), iterator.browser());
    else if (count == 3)
      EXPECT_EQ(browser3.get(), iterator.browser());
    else
      EXPECT_TRUE(false);
  }

  // Close some tabs.
  browser2->CloseAllTabs();

  count = 0;
  for (TabContentsIterator iterator; !iterator.done(); ++iterator, ++count) {
    if (count == 0)
      EXPECT_EQ(browser3.get(), iterator.browser());
    else
      EXPECT_TRUE(false);
  }

  // Now make it one tab per browser.
  browser()->NewTab();
  browser2->NewTab();

  count = 0;
  for (TabContentsIterator iterator; !iterator.done(); ++iterator, ++count) {
    if (count == 0)
      EXPECT_EQ(browser(), iterator.browser());
    else if (count == 1)
      EXPECT_EQ(browser2.get(), iterator.browser());
    else if (count == 2)
      EXPECT_EQ(browser3.get(), iterator.browser());
    else
      EXPECT_TRUE(false);
  }

  // Close all remaining tabs to keep all the destructors happy.
  browser2->CloseAllTabs();
  browser3->CloseAllTabs();
}

#if 0
// TODO(thestig) Fix or remove this test. http://crbug.com/100309
TEST_F(BrowserListTest, TabContentsIteratorBackgroundPrinting) {
  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, BrowserList::size());

  // Create more browsers/windows.
  scoped_ptr<Browser> browser2(new Browser(Browser::TYPE_TABBED, profile()));
  scoped_ptr<Browser> browser3(new Browser(Browser::TYPE_TABBED, profile()));

  scoped_ptr<TestBrowserWindow> window2(new TestBrowserWindow(browser2.get()));
  scoped_ptr<TestBrowserWindow> window3(new TestBrowserWindow(browser3.get()));

  browser2->SetWindowForTesting(window2.get());
  browser3->SetWindowForTesting(window3.get());

  EXPECT_EQ(0U, CountAllTabs());

  // Add some tabs.
  for (size_t i = 0; i < 3; ++i)
    browser2->NewTab();
  browser3->NewTab();

  EXPECT_EQ(4U, CountAllTabs());

  TestingBrowserProcess* browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  printing::BackgroundPrintingManager* bg_print_manager =
      browser_process->background_printing_manager();

  // Grab a tab and give ownership to BackgroundPrintingManager.
  TabContentsIterator tab_iterator;
  TabContentsWrapper* tab = *tab_iterator;
  int page_id = 1;
  NavigateToPrintUrl(tab, page_id++);

  bg_print_manager->OwnPrintPreviewTab(tab);

  EXPECT_EQ(4U, CountAllTabs());

  // Close remaining tabs.
  browser2->CloseAllTabs();
  browser3->CloseAllTabs();

  EXPECT_EQ(1U, CountAllTabs());

  // Delete the last remaining tab.
  delete tab;

  EXPECT_EQ(0U, CountAllTabs());

  // Add some tabs.
  for (size_t i = 0; i < 3; ++i) {
    browser2->NewTab();
    browser3->NewTab();
  }

  EXPECT_EQ(6U, CountAllTabs());

  // Tell BackgroundPrintingManager to take ownership of all tabs.
  // Save the tabs in |owned_tabs| because manipulating tabs in the middle of
  // TabContentsIterator is a bad idea.
  std::vector<TabContentsWrapper*> owned_tabs;
  for (TabContentsIterator iterator; !iterator.done(); ++iterator) {
    NavigateToPrintUrl(*iterator, page_id++);
    owned_tabs.push_back(*iterator);
  }
  for (std::vector<TabContentsWrapper*>::iterator it = owned_tabs.begin();
       it != owned_tabs.end(); ++it) {
    bg_print_manager->OwnPrintPreviewTab(*it);
  }

  EXPECT_EQ(6U, CountAllTabs());

  // Delete all tabs to clean up.
  for (std::vector<TabContentsWrapper*>::iterator it = owned_tabs.begin();
       it != owned_tabs.end(); ++it) {
    delete *it;
  }

  EXPECT_EQ(0U, CountAllTabs());
}
#endif

#if defined(OS_CHROMEOS)
// Calling AttemptRestart on ChromeOS will exit the test.
#define MAYBE_AttemptRestart DISABLED_AttemptRestart
#else
#define MAYBE_AttemptRestart AttemptRestart
#endif

TEST_F(BrowserListTest, MAYBE_AttemptRestart) {
  ASSERT_TRUE(g_browser_process);
  TestingPrefService testing_pref_service;
  testing_pref_service.RegisterBooleanPref(prefs::kWasRestarted, false);
  testing_pref_service.RegisterBooleanPref(prefs::kRestartLastSessionOnShutdown,
                                           false);

  TestingBrowserProcess* testing_browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  testing_browser_process->SetLocalState(&testing_pref_service);
  ASSERT_TRUE(g_browser_process->local_state());
  ProfileManager* profile_manager = new ProfileManager(FilePath());
  testing_browser_process->SetProfileManager(profile_manager);

  browser::AttemptRestart();
  // Cancel the effects of us calling browser::AttemptRestart. Otherwise tests
  // ran after this one will fail.
  browser_shutdown::SetTryingToQuit(false);

  EXPECT_TRUE(testing_pref_service.GetBoolean(prefs::kWasRestarted));
  testing_browser_process->SetLocalState(NULL);
}
