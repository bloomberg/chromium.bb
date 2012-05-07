// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
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
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/test/test_navigation_observer.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

class SessionRestoreTest : public InProcessBrowserTest {
 protected:

  virtual void SetUpOnMainThread() OVERRIDE {
    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(browser()->profile(), pref);
#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(), "NoSessionRestoreNewWindowChromeOS")) {
      // Undo the effect of kBrowserAliveWithNoWindows in defaults.cc so that we
      // can get these test to work without quitting.
      SessionServiceFactory::GetForProfile(browser()->profile())->
          force_browser_not_alive_with_no_windows_ = true;
    }
#endif
  }

  virtual bool SetUpUserDataDirectory() OVERRIDE {
    // Make sure the first run sentinel file exists before running these tests,
    // since some of them customize the session startup pref whose value can
    // be different than the default during the first run.
    // TODO(bauerb): set the first run flag instead of creating a sentinel file.
    first_run::CreateSentinel();

    url1_ = ui_test_utils::GetTestUrl(
        FilePath().AppendASCII("session_history"),
        FilePath().AppendASCII("bot1.html"));
    url2_ = ui_test_utils::GetTestUrl(
        FilePath().AppendASCII("session_history"),
        FilePath().AppendASCII("bot2.html"));
    url3_ = ui_test_utils::GetTestUrl(
        FilePath().AppendASCII("session_history"),
        FilePath().AppendASCII("bot3.html"));

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

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
#if defined(OS_MACOSX)
    // BrowserWindowController depends on the auto release pool being recycled
    // in the message loop to delete itself, which frees the Browser object
    // which fires this event.
    AutoreleasePool()->Recycle();
#endif
    observer.Wait();
  }

  Browser* QuitBrowserAndRestore(Browser* browser, int expected_tab_count) {
    // Create a new popup.
    Profile* profile = browser->profile();

    // Close the browser.
    g_browser_process->AddRefModule();
    CloseBrowserSynchronously(browser);

    // Create a new window, which should trigger session restore.
    ui_test_utils::BrowserAddedObserver window_observer;
    TestNavigationObserver navigation_observer(
        content::NotificationService::AllSources(), NULL, expected_tab_count);
    Browser::NewEmptyWindow(profile);
    Browser* new_browser = window_observer.WaitForSingleNewBrowser();
    navigation_observer.Wait();
    g_browser_process->ReleaseModule();

    return new_browser;
  }

  void GoBack(Browser* browser) {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser->GoBack(CURRENT_TAB);
    observer.Wait();
  }

  void GoForward(Browser* browser) {
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    browser->GoForward(CURRENT_TAB);
    observer.Wait();
  }

  void AssertOneWindowWithOneTab(Browser* browser) {
    ASSERT_EQ(1u, BrowserList::size());
    ASSERT_EQ(1, browser->tab_count());
  }

  int RenderProcessHostCount() {
    content::RenderProcessHost::iterator hosts =
        content::RenderProcessHost::AllHostsIterator();
    int count = 0;
    while (!hosts.IsAtEnd()) {
      if (hosts.GetCurrentValue()->HasConnection())
        count++;
      hosts.Advance();
    }
    return count;
  }

  GURL url1_;
  GURL url2_;
  GURL url3_;
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
  Browser* popup = Browser::CreateWithParams(
      Browser::CreateParams(Browser::TYPE_POPUP, profile));
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
  ASSERT_EQ(0, browser()->active_index());
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

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreForeignSession) {
  Profile* profile = browser()->profile();

  GURL url1("http://google.com");
  GURL url2("http://google2.com");
  TabNavigation nav1(0, url1, content::Referrer(), ASCIIToUTF16("one"),
        std::string(), content::PAGE_TRANSITION_TYPED);
  TabNavigation nav2(0, url2, content::Referrer(), ASCIIToUTF16("two"),
        std::string(), content::PAGE_TRANSITION_TYPED);

  // Set up the restore data -- one window with two tabs.
  std::vector<const SessionWindow*> session;
  SessionWindow window;
  SessionTab tab1;
  tab1.tab_visual_index = 0;
  tab1.current_navigation_index = 0;
  tab1.pinned = true;
  tab1.navigations.push_back(nav1);
  window.tabs.push_back(&tab1);

  SessionTab tab2;
  tab2.tab_visual_index = 1;
  tab2.current_navigation_index = 0;
  tab2.pinned = false;
  tab2.navigations.push_back(nav2);
  window.tabs.push_back(&tab2);

  session.push_back(static_cast<const SessionWindow*>(&window));
  ui_test_utils::BrowserAddedObserver window_observer;
  SessionRestore::RestoreForeignSessionWindows(
      profile, session.begin(), session.end());
  Browser* new_browser = window_observer.WaitForSingleNewBrowser();
  ASSERT_TRUE(new_browser);
  ASSERT_EQ(2u, BrowserList::size());
  ASSERT_EQ(2, new_browser->tab_count());

  ASSERT_EQ(url1, new_browser->GetWebContentsAt(0)->GetURL());
  ASSERT_EQ(url2, new_browser->GetWebContentsAt(1)->GetURL());

  // The SessionWindow destructor deletes the tabs, so we have to clear them
  // here to avoid a crash.
  window.tabs.clear();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, Basic) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  ui_test_utils::NavigateToURL(browser(), url2_);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, BrowserList::size());
  ASSERT_EQ(url2_, new_browser->GetSelectedWebContents()->GetURL());
  GoBack(new_browser);
  ASSERT_EQ(url1_, new_browser->GetSelectedWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoresForwardAndBackwardNavs) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  ui_test_utils::NavigateToURL(browser(), url2_);
  ui_test_utils::NavigateToURL(browser(), url3_);

  GoBack(browser());
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, BrowserList::size());
  ASSERT_EQ(url2_, new_browser->GetSelectedWebContents()->GetURL());
  GoForward(new_browser);
  ASSERT_EQ(url3_, new_browser->GetSelectedWebContents()->GetURL());
  GoBack(new_browser);
  ASSERT_EQ(url2_, new_browser->GetSelectedWebContents()->GetURL());

  // Test renderer-initiated back/forward as well.
  GURL go_back_url("javascript:history.back();");
  ui_test_utils::NavigateToURL(new_browser, go_back_url);
  ASSERT_EQ(url1_, new_browser->GetSelectedWebContents()->GetURL());
}

// Tests that the SiteInstances used for entries in a restored tab's history
// are given appropriate max page IDs, so that going back to a restored
// cross-site page and then forward again works.  (Bug 1204135)
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       RestoresCrossSiteForwardAndBackwardNavs) {
  ASSERT_TRUE(test_server()->Start());

  GURL cross_site_url(test_server()->GetURL("files/title2.html"));

  // Visit URLs on different sites.
  ui_test_utils::NavigateToURL(browser(), url1_);
  ui_test_utils::NavigateToURL(browser(), cross_site_url);
  ui_test_utils::NavigateToURL(browser(), url2_);

  GoBack(browser());
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, BrowserList::size());
  ASSERT_EQ(1, new_browser->tab_count());

  // Check that back and forward work as expected.
  ASSERT_EQ(cross_site_url, new_browser->GetSelectedWebContents()->GetURL());

  GoBack(new_browser);
  ASSERT_EQ(url1_, new_browser->GetSelectedWebContents()->GetURL());

  GoForward(new_browser);
  ASSERT_EQ(cross_site_url, new_browser->GetSelectedWebContents()->GetURL());

  // Test renderer-initiated back/forward as well.
  GURL go_forward_url("javascript:history.forward();");
  ui_test_utils::NavigateToURL(new_browser, go_forward_url);
  ASSERT_EQ(url2_, new_browser->GetSelectedWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, TwoTabsSecondSelected) {
  ui_test_utils::NavigateToURL(browser(), url1_);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 2);

  ASSERT_EQ(1u, BrowserList::size());
  ASSERT_EQ(2, new_browser->tab_count());
  ASSERT_EQ(1, new_browser->active_index());
  ASSERT_EQ(url2_, new_browser->GetSelectedWebContents()->GetURL());

  ASSERT_EQ(url1_, new_browser->GetWebContentsAt(0)->GetURL());
}

// Creates two tabs, closes one, quits and makes sure only one tab is restored.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ClosedTabStaysClosed) {
  ui_test_utils::NavigateToURL(browser(), url1_);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  browser()->CloseTab();

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);

  AssertOneWindowWithOneTab(new_browser);
  ASSERT_EQ(url1_, new_browser->GetSelectedWebContents()->GetURL());
}

// Test to verify that the print preview tab is not restored.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, DontRestorePrintPreviewTabTest) {
  ui_test_utils::NavigateToURL(browser(), url1_);

  // Append the print preview tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIPrintURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Restart and make sure we have only one window with one tab and the url
  // is url1_.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);

  AssertOneWindowWithOneTab(new_browser);
  ASSERT_EQ(url1_, new_browser->GetSelectedWebContents()->GetURL());
}

// Creates a tabbed browser and popup and makes sure we restore both.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, NormalAndPopup) {
  if (!browser_defaults::kRestorePopups)
    return;  // Test only applicable if restoring popups.

  ui_test_utils::NavigateToURL(browser(), url1_);

  // Make sure we have one window.
  AssertOneWindowWithOneTab(browser());

  // Open a popup.
  Browser* popup = Browser::CreateWithParams(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile()));
  popup->window()->Show();
  ASSERT_EQ(2u, BrowserList::size());

  ui_test_utils::NavigateToURL(popup, url1_);

  // Simulate an exit by shuting down the session service. If we don't do this
  // the first window close is treated as though the user closed the window
  // and won't be restored.
  SessionServiceFactory::ShutdownForProfile(browser()->profile());

  // Restart and make sure we have two windows.
  QuitBrowserAndRestore(browser(), 1);

  ASSERT_EQ(2u, BrowserList::size());

  Browser* browser1 = *BrowserList::begin();
  Browser* browser2 = *(++BrowserList::begin());

  Browser::Type type1 = browser1->type();
  Browser::Type type2 = browser2->type();

  // The order of whether the normal window or popup is first depends upon
  // activation order, which is not necessarily consistant across runs.
  if (type1 == Browser::TYPE_TABBED) {
    EXPECT_EQ(type2, Browser::TYPE_POPUP);
  } else {
    EXPECT_EQ(type1, Browser::TYPE_POPUP);
    EXPECT_EQ(type2, Browser::TYPE_TABBED);
  }
}

#if !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
// This test doesn't apply to the Mac version; see GetCommandLineForRelaunch
// for details. It was disabled for a long time so might never have worked on
// ChromeOS.

// Launches an app window, closes tabbed browser, launches and makes sure
// we restore the tabbed browser url.
// If this test flakes, use http://crbug.com/29110
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       RestoreAfterClosingTabbedBrowserWithAppAndLaunching) {
  ui_test_utils::NavigateToURL(browser(), url1_);

  // Launch an app.
  CommandLine app_launch_arguments = GetCommandLineForRelaunch();
  app_launch_arguments.AppendSwitchASCII(switches::kApp, url2_.spec());

  ui_test_utils::BrowserAddedObserver window_observer;

  base::LaunchProcess(app_launch_arguments, base::LaunchOptions(), NULL);

  Browser* app_window = window_observer.WaitForSingleNewBrowser();
  ASSERT_EQ(2u, BrowserList::size());

  // Close the first window. The only window left is the App window.
  CloseBrowserSynchronously(browser());

  // Restore the session, which should bring back the first window with url1_.
  Browser* new_browser = QuitBrowserAndRestore(app_window, 1);

  AssertOneWindowWithOneTab(new_browser);

  ASSERT_EQ(url1_, new_browser->GetSelectedWebContents()->GetURL());
}

#endif  // !defined(OS_CHROMEOS) && !defined(OS_MACOSX)

// Creates two windows, closes one, restores, make sure only one window open.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, TwoWindowsCloseOneRestoreOnlyOne) {
  ui_test_utils::NavigateToURL(browser(), url1_);

  // Open a second window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kAboutBlankURL), NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);

  ASSERT_EQ(2u, BrowserList::size());

  // Close it.
  Browser* new_window = *(++BrowserList::begin());
  CloseBrowserSynchronously(new_window);

  // Restart and make sure we have only one window with one tab and the url
  // is url1_.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);

  AssertOneWindowWithOneTab(new_browser);

  ASSERT_EQ(url1_, new_browser->GetSelectedWebContents()->GetURL());
}

// Make sure after a restore the number of processes matches that of the number
// of processes running before the restore. This creates a new tab so that
// we should have two new tabs running.  (This test will pass in both
// process-per-site and process-per-site-instance, because we treat the new tab
// as a special case in process-per-site-instance so that it only ever uses one
// process.)
//
// Flaky: http://code.google.com/p/chromium/issues/detail?id=52022
// Unfortunately, the fix at http://codereview.chromium.org/6546078
// breaks NTP background image refreshing, so ThemeSource had to revert to
// replacing the existing data source.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ShareProcessesOnRestore) {
  // Create two new tabs.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kAboutBlankURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kAboutBlankURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  int expected_process_count = RenderProcessHostCount();

  // Restart.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 3);

  ASSERT_EQ(3, new_browser->tab_count());

  ASSERT_EQ(expected_process_count, RenderProcessHostCount());
}
