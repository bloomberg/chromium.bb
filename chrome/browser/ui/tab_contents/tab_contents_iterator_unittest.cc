// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"

typedef BrowserWithTestWindowTest BrowserListTest;

namespace {

// Helper function to iterate and count all the tabs.
size_t CountAllTabs() {
  size_t count = 0;
  for (TabContentsIterator iterator; !iterator.done(); ++iterator)
    ++count;
  return count;
}

}  // namespace

TEST_F(BrowserListTest, TabContentsIteratorVerifyCount) {
  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, BrowserList::size());

  EXPECT_EQ(0U, CountAllTabs());

  // Create more browsers/windows.
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForProfile(profile()));
  scoped_ptr<Browser> browser3(
      chrome::CreateBrowserWithTestWindowForProfile(profile()));
  scoped_ptr<Browser> browser4(
      chrome::CreateBrowserWithTestWindowForProfile(profile()));

  // Sanity checks.
  EXPECT_EQ(4U, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser2->tab_strip_model()->count());
  EXPECT_EQ(0, browser3->tab_strip_model()->count());
  EXPECT_EQ(0, browser4->tab_strip_model()->count());

  EXPECT_EQ(0U, CountAllTabs());

  // Add some tabs.
  for (size_t i = 0; i < 3; ++i)
    chrome::NewTab(browser2.get());
  chrome::NewTab(browser3.get());

  EXPECT_EQ(4U, CountAllTabs());

  // Close some tabs.
  browser2->tab_strip_model()->CloseAllTabs();

  EXPECT_EQ(1U, CountAllTabs());

  // Add lots of tabs.
  for (size_t i = 0; i < 41; ++i)
    chrome::NewTab(browser());

  EXPECT_EQ(42U, CountAllTabs());
  // Close all remaining tabs to keep all the destructors happy.
  browser3->tab_strip_model()->CloseAllTabs();
}

TEST_F(BrowserListTest, TabContentsIteratorVerifyBrowser) {
  // Make sure we have 1 window to start with.
  EXPECT_EQ(1U, BrowserList::size());

  // Create more browsers/windows.
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForProfile(profile()));
  scoped_ptr<Browser> browser3(
      chrome::CreateBrowserWithTestWindowForProfile(profile()));

  // Sanity checks.
  EXPECT_EQ(3U, BrowserList::size());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser2->tab_strip_model()->count());
  EXPECT_EQ(0, browser3->tab_strip_model()->count());

  EXPECT_EQ(0U, CountAllTabs());

  // Add some tabs.
  for (size_t i = 0; i < 3; ++i)
    chrome::NewTab(browser2.get());
  chrome::NewTab(browser3.get());

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
  browser2->tab_strip_model()->CloseAllTabs();

  count = 0;
  for (TabContentsIterator iterator; !iterator.done(); ++iterator, ++count) {
    if (count == 0)
      EXPECT_EQ(browser3.get(), iterator.browser());
    else
      EXPECT_TRUE(false);
  }

  // Now make it one tab per browser.
  chrome::NewTab(browser());
  chrome::NewTab(browser2.get());

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
  browser2->tab_strip_model()->CloseAllTabs();
  browser3->tab_strip_model()->CloseAllTabs();
}

#if defined(OS_CHROMEOS)
// Calling AttemptRestart on ChromeOS will exit the test.
#define MAYBE_AttemptRestart DISABLED_AttemptRestart
#else
#define MAYBE_AttemptRestart AttemptRestart
#endif

TEST_F(BrowserListTest, MAYBE_AttemptRestart) {
  ASSERT_TRUE(g_browser_process);
  TestingPrefServiceSimple testing_pref_service;
  testing_pref_service.RegisterBooleanPref(prefs::kWasRestarted, false);
  testing_pref_service.RegisterBooleanPref(prefs::kRestartLastSessionOnShutdown,
                                           false);

  TestingBrowserProcess* testing_browser_process =
      TestingBrowserProcess::GetGlobal();
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
