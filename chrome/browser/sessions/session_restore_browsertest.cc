// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"

namespace {

// Verifies that the given NavigationController has exactly two entries that
// correspond to the given URLs.
void VerifyNavigationEntries(
    content::NavigationController& controller, GURL url1, GURL url2) {
  ASSERT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(1)->GetURL());
}

void CloseBrowserSynchronously(Browser* browser) {
  ui_test_utils::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::NotificationService::AllSources());
  browser->window()->Close();
  observer.Wait();
}

}  // namespace

class SessionRestoreTest : public InProcessBrowserTest {
 protected:
  virtual bool SetUpUserDataDirectory() OVERRIDE {
    // Make sure the first run sentinel file exists before running these tests,
    // since some of them customize the session startup pref whose value can
    // be different than the default during the first run.
    // TODO(bauerb): set the first run flag instead of creating a sentinel file.
    first_run::CreateSentinel();
    return InProcessBrowserTest::SetUpUserDataDirectory();
  }
};

#if defined(OS_CHROMEOS)
// Verify that session restore does not occur when a user opens a browser window
// when no other browser windows are open on ChromeOS.
// TODO(pkotwicz): Add test which doesn't open incognito browser once
// disable-zero-browsers-open-for-tests is removed.
// (http://crbug.com/119175)
// TODO(pkotwicz): Mac should have the behavior outlined by this test. It should
// not do session restore if an incognito window is already open.
// (http://crbug.com/120927)
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, NoSessionRestoreNewWindowChromeOS) {
  // Turn on session restore.
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  GURL url(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  Browser* incognito_browser = CreateIncognitoBrowser();
  incognito_browser->AddBlankTab(true);
  incognito_browser->window()->Show();

  // Close the normal browser. After this we only have the incognito window
  // open.
  CloseBrowserSynchronously(browser());

  // Create a new window, which should open NTP.
  ui_test_utils::BrowserAddedObserver browser_added_observer;
  incognito_browser->NewWindow();
  Browser* new_browser = browser_added_observer.WaitForSingleNewBrowser();

  ASSERT_TRUE(new_browser);
  EXPECT_EQ(1, new_browser->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            new_browser->GetWebContentsAt(0)->GetURL());
}
#endif  // OS_CHROMEOS

#if !defined(OS_CHROMEOS)
// This test does not apply to ChromeOS as it does not do session restore when
// a new window is opened.

#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
// Crashes on Linux Views: http://crbug.com/39476
#define MAYBE_RestoreOnNewWindowWithNoTabbedBrowsers \
        DISABLED_RestoreOnNewWindowWithNoTabbedBrowsers
#else
#define MAYBE_RestoreOnNewWindowWithNoTabbedBrowsers \
        RestoreOnNewWindowWithNoTabbedBrowsers
#endif

// Makes sure when session restore is triggered in the same process we don't end
// up with an extra tab.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       MAYBE_RestoreOnNewWindowWithNoTabbedBrowsers) {
  if (browser_defaults::kRestorePopups)
    return;

  const FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));
  ui_test_utils::NavigateToURL(browser(), url);

  // Turn on session restore.
  SessionStartupPref::SetStartupPref(
      browser()->profile(),
      SessionStartupPref(SessionStartupPref::LAST));

  // Create a new popup.
  Profile* profile = browser()->profile();
  Browser* popup = Browser::CreateForType(Browser::TYPE_POPUP, profile);
  popup->window()->Show();

  // Close the browser.
  CloseBrowserSynchronously(browser());

  // Create a new window, which should trigger session restore.
  ui_test_utils::BrowserAddedObserver observer;
  popup->NewWindow();
  Browser* new_browser = observer.WaitForSingleNewBrowser();

  ASSERT_TRUE(new_browser != NULL);

  // The browser should only have one tab.
  ASSERT_EQ(1, new_browser->tab_count());

  // And the first url should be url.
  EXPECT_EQ(url, new_browser->GetWebContentsAt(0)->GetURL());
}
#endif  // !OS_CHROMEOS

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreIndividualTabFromWindow) {
  GURL url1(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));
  GURL url2(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title2.html"))));
  GURL url3(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title3.html"))));

  // Add and navigate three tabs.
  ui_test_utils::NavigateToURL(browser(), url1);
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->AddSelectedTabWithURL(url2, content::PAGE_TRANSITION_LINK);
    observer.Wait();
  }
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser()->AddSelectedTabWithURL(url3, content::PAGE_TRANSITION_LINK);
    observer.Wait();
  }

  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  service->ClearEntries();

  browser()->window()->Close();

  // Expect a window with three tabs.
  EXPECT_EQ(1U, service->entries().size());
  ASSERT_EQ(TabRestoreService::WINDOW, service->entries().front()->type);
  const TabRestoreService::Window* window =
      static_cast<TabRestoreService::Window*>(service->entries().front());
  EXPECT_EQ(3U, window->tabs.size());

  // Find the SessionID for entry2. Since the session service was destroyed,
  // there is no guarantee that the SessionID for the tab has remained the same.
  std::vector<TabRestoreService::Tab>::const_iterator it = window->tabs.begin();
  for ( ; it != window->tabs.end(); ++it) {
    const TabRestoreService::Tab& tab = *it;
    // If this tab held url2, then restore this single tab.
    if (tab.navigations[0].virtual_url() == url2) {
      service->RestoreEntryById(NULL, tab.id, UNKNOWN);
      break;
    }
  }

  // Make sure that the Window got updated.
  EXPECT_EQ(1U, service->entries().size());
  ASSERT_EQ(TabRestoreService::WINDOW, service->entries().front()->type);
  window = static_cast<TabRestoreService::Window*>(service->entries().front());
  EXPECT_EQ(2U, window->tabs.size());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, WindowWithOneTab) {
  GURL url(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  service->ClearEntries();
  EXPECT_EQ(0U, service->entries().size());

  // Close the window.
  browser()->window()->Close();

  // Expect the window to be converted to a tab by the TRS.
  EXPECT_EQ(1U, service->entries().size());
  ASSERT_EQ(TabRestoreService::TAB, service->entries().front()->type);
  const TabRestoreService::Tab* tab =
      static_cast<TabRestoreService::Tab*>(service->entries().front());

  // Restore the tab.
  service->RestoreEntryById(NULL, tab->id, UNKNOWN);

  // Make sure the restore was successful.
  EXPECT_EQ(0U, service->entries().size());
}

#if !defined(OS_CHROMEOS)
// This test does not apply to ChromeOS as ChromeOS does not do session
// restore when a new window is open.

// Verifies we remember the last browser window when closing the last
// non-incognito window while an incognito window is open.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, IncognitotoNonIncognito) {
  // Turn on session restore.
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  GURL url(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  // Create a new incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  incognito_browser->AddBlankTab(true);
  incognito_browser->window()->Show();

  // Close the normal browser. After this we only have the incognito window
  // open.
  CloseBrowserSynchronously(browser());

  // Create a new window, which should trigger session restore.
  ui_test_utils::BrowserAddedObserver browser_added_observer;
  incognito_browser->NewWindow();
  Browser* new_browser = browser_added_observer.WaitForSingleNewBrowser();

  // The first tab should have 'url' as its url.
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(url, new_browser->GetWebContentsAt(0)->GetURL());
}
#endif  // !OS_CHROMEOS

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreForeignTab) {
  Profile* profile = browser()->profile();

  GURL url1("http://google.com");
  GURL url2("http://google2.com");
  TabNavigation nav1(0, url1, content::Referrer(), ASCIIToUTF16("one"),
        std::string(), content::PAGE_TRANSITION_TYPED);
  TabNavigation nav2(0, url2, content::Referrer(), ASCIIToUTF16("two"),
        std::string(), content::PAGE_TRANSITION_TYPED);

  // Set up the restore data.
  SessionTab tab;
  tab.tab_visual_index = 0;
  tab.current_navigation_index = 1;
  tab.pinned = false;
  tab.navigations.push_back(nav1);
  tab.navigations.push_back(nav2);

  ASSERT_EQ(1, browser()->tab_count());

  // Restore in the current tab.
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    SessionRestore::RestoreForeignSessionTab(profile, tab, CURRENT_TAB);
    observer.Wait();
  }
  ASSERT_EQ(1, browser()->tab_count());
  VerifyNavigationEntries(
      browser()->GetWebContentsAt(0)->GetController(), url1, url2);

  // Restore in a new tab.
  {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    SessionRestore::RestoreForeignSessionTab(profile, tab, NEW_BACKGROUND_TAB);
    observer.Wait();
  }
  ASSERT_EQ(2, browser()->tab_count());
  VerifyNavigationEntries(
      browser()->GetWebContentsAt(1)->GetController(), url1, url2);

  // Restore in a new window.
  ui_test_utils::BrowserAddedObserver browser_observer;
  SessionRestore::RestoreForeignSessionTab(profile, tab, NEW_WINDOW);
  Browser* new_browser = browser_observer.WaitForSingleNewBrowser();

  ASSERT_EQ(1, new_browser->tab_count());
  VerifyNavigationEntries(
      new_browser->GetWebContentsAt(0)->GetController(), url1, url2);
}
