// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/session_types_test_helper.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list_impl.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/test_navigation_observer.h"
#include "sync/protocol/session_specifics.pb.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

class SessionRestoreTest : public InProcessBrowserTest {
 public:
  SessionRestoreTest()
      : native_browser_list(chrome::BrowserListImpl::GetInstance(
                                chrome::HOST_DESKTOP_TYPE_NATIVE)) {
  }

 protected:
#if defined(OS_CHROMEOS)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // TODO(nkostylev): Investigate if we can remove this switch.
    command_line->AppendSwitch(switches::kCreateBrowserOnStartupForTests);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }
#endif

  virtual void SetUpOnMainThread() OVERRIDE {
    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(browser()->profile(), pref);
#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(), "NoSessionRestoreNewWindowChromeOS")) {
      // Undo the effect of kBrowserAliveWithNoWindows in defaults.cc so that we
      // can get these test to work without quitting.
      SessionServiceTestHelper helper(
          SessionServiceFactory::GetForProfile(browser()->profile()));
      helper.SetForceBrowserNotAliveWithNoWindows(true);
      helper.ReleaseService();
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
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot1.html"));
    url2_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot2.html"));
    url3_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot3.html"));

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

  void CloseBrowserSynchronously(Browser* browser) {
    content::WindowedNotificationObserver observer(
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
    Profile* profile = browser->profile();

    // Close the browser.
    g_browser_process->AddRefModule();
    CloseBrowserSynchronously(browser);

    // Create a new window, which should trigger session restore.
    ui_test_utils::BrowserAddedObserver window_observer;
    content::TestNavigationObserver navigation_observer(
        content::NotificationService::AllSources(), NULL, expected_tab_count);
    chrome::NewEmptyWindow(profile);
    Browser* new_browser = window_observer.WaitForSingleNewBrowser();
    navigation_observer.Wait();
    g_browser_process->ReleaseModule();

    return new_browser;
  }

  void GoBack(Browser* browser) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::GoBack(browser, CURRENT_TAB);
    observer.Wait();
  }

  void GoForward(Browser* browser) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::GoForward(browser, CURRENT_TAB);
    observer.Wait();
  }

  void AssertOneWindowWithOneTab(Browser* browser) {
    ASSERT_EQ(1u, native_browser_list->size());
    ASSERT_EQ(1, browser->tab_strip_model()->count());
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

  // The SessionRestore browser tests only uses the native desktop for now.
  const chrome::BrowserListImpl* native_browser_list;
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
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  Browser* incognito_browser = CreateIncognitoBrowser();
  chrome::AddBlankTabAt(incognito_browser, -1, true);
  incognito_browser->window()->Show();

  // Close the normal browser. After this we only have the incognito window
  // open.
  CloseBrowserSynchronously(browser());

  // Create a new window, which should open NTP.
  ui_test_utils::BrowserAddedObserver browser_added_observer;
  chrome::NewWindow(incognito_browser);
  Browser* new_browser = browser_added_observer.WaitForSingleNewBrowser();

  ASSERT_TRUE(new_browser);
  EXPECT_EQ(1, new_browser->tab_strip_model()->count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            new_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
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

  const base::FilePath::CharType* kTitle1File =
      FILE_PATH_LITERAL("title1.html");
  GURL url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kTitle1File)));
  ui_test_utils::NavigateToURL(browser(), url);

  // Turn on session restore.
  SessionStartupPref::SetStartupPref(
      browser()->profile(),
      SessionStartupPref(SessionStartupPref::LAST));

  // Create a new popup.
  Profile* profile = browser()->profile();
  Browser* popup =
      new Browser(Browser::CreateParams(Browser::TYPE_POPUP, profile,
                                        browser()->host_desktop_type()));
  popup->window()->Show();

  // Close the browser.
  CloseBrowserSynchronously(browser());

  // Create a new window, which should trigger session restore.
  ui_test_utils::BrowserAddedObserver observer;
  chrome::NewWindow(popup);
  Browser* new_browser = observer.WaitForSingleNewBrowser();

  ASSERT_TRUE(new_browser != NULL);

  // The browser should only have one tab.
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());

  // And the first url should be url.
  EXPECT_EQ(url, new_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
}
#endif  // !OS_CHROMEOS

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreIndividualTabFromWindow) {
  GURL url1(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  GURL url2(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));
  GURL url3(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title3.html"))));

  // Add and navigate three tabs.
  ui_test_utils::NavigateToURL(browser(), url1);
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), url2,
                                  content::PAGE_TRANSITION_LINK);
    observer.Wait();
  }
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), url3,
                                  content::PAGE_TRANSITION_LINK);
    observer.Wait();
  }

  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  service->ClearEntries();

  chrome::HostDesktopType host_desktop_type = browser()->host_desktop_type();

  browser()->window()->Close();

  // Expect a window with three tabs.
  ASSERT_EQ(1U, service->entries().size());
  ASSERT_EQ(TabRestoreService::WINDOW, service->entries().front()->type);
  const TabRestoreService::Window* window =
      static_cast<TabRestoreService::Window*>(service->entries().front());
  EXPECT_EQ(3U, window->tabs.size());

  // Find the SessionID for entry2. Since the session service was destroyed,
  // there is no guarantee that the SessionID for the tab has remained the same.
  base::Time timestamp;
  for (std::vector<TabRestoreService::Tab>::const_iterator it =
           window->tabs.begin(); it != window->tabs.end(); ++it) {
    const TabRestoreService::Tab& tab = *it;
    // If this tab held url2, then restore this single tab.
    if (tab.navigations[0].virtual_url() == url2) {
      timestamp = SessionTypesTestHelper::GetTimestamp(tab.navigations[0]);
      service->RestoreEntryById(NULL, tab.id, host_desktop_type, UNKNOWN);
      break;
    }
  }
  EXPECT_FALSE(timestamp.is_null());

  // Make sure that the restored tab is removed from the service.
  ASSERT_EQ(1U, service->entries().size());
  ASSERT_EQ(TabRestoreService::WINDOW, service->entries().front()->type);
  window = static_cast<TabRestoreService::Window*>(service->entries().front());
  EXPECT_EQ(2U, window->tabs.size());

  // Make sure that the restored tab was restored with the correct
  // timestamp.
  const content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  const content::NavigationEntry* entry =
      contents->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(timestamp, entry->GetTimestamp());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, WindowWithOneTab) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  service->ClearEntries();
  EXPECT_EQ(0U, service->entries().size());

  chrome::HostDesktopType host_desktop_type = browser()->host_desktop_type();

  // Close the window.
  browser()->window()->Close();

  // Expect the window to be converted to a tab by the TRS.
  EXPECT_EQ(1U, service->entries().size());
  ASSERT_EQ(TabRestoreService::TAB, service->entries().front()->type);
  const TabRestoreService::Tab* tab =
      static_cast<TabRestoreService::Tab*>(service->entries().front());

  // Restore the tab.
  service->RestoreEntryById(NULL, tab->id, host_desktop_type, UNKNOWN);

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
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  // Create a new incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  chrome::AddBlankTabAt(incognito_browser, -1, true);
  incognito_browser->window()->Show();

  // Close the normal browser. After this we only have the incognito window
  // open.
  CloseBrowserSynchronously(browser());

  // Create a new window, which should trigger session restore.
  ui_test_utils::BrowserAddedObserver browser_added_observer;
  chrome::NewWindow(incognito_browser);
  Browser* new_browser = browser_added_observer.WaitForSingleNewBrowser();

  // The first tab should have 'url' as its url.
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(url, new_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
}
#endif  // !OS_CHROMEOS

namespace {

// Verifies that the given NavigationController has exactly two
// entries that correspond to the given URLs and that all but the last
// entry have null timestamps.
void VerifyNavigationEntries(
    const content::NavigationController& controller,
    GURL url1, GURL url2) {
  ASSERT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_TRUE(controller.GetEntryAtIndex(0)->GetTimestamp().is_null());
  EXPECT_FALSE(controller.GetEntryAtIndex(1)->GetTimestamp().is_null());
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreForeignTab) {
  GURL url1("http://google.com");
  GURL url2("http://google2.com");
  TabNavigation nav1 =
      SessionTypesTestHelper::CreateNavigation(url1.spec(), "one");
  TabNavigation nav2 =
      SessionTypesTestHelper::CreateNavigation(url2.spec(), "two");

  // Set up the restore data.
  sync_pb::SessionTab sync_data;
  sync_data.set_tab_visual_index(0);
  sync_data.set_current_navigation_index(1);
  sync_data.set_pinned(false);
  sync_data.add_navigation()->CopyFrom(nav1.ToSyncData());
  sync_data.add_navigation()->CopyFrom(nav2.ToSyncData());

  SessionTab tab;
  tab.SetFromSyncData(sync_data, base::Time::Now());
  EXPECT_EQ(2U, tab.navigations.size());
  for (size_t i = 0; i < tab.navigations.size(); ++i) {
    EXPECT_TRUE(
        SessionTypesTestHelper::GetTimestamp(tab.navigations[i]).is_null());
  }

  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Restore in the current tab.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(), tab, CURRENT_TAB);
    observer.Wait();
  }
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  VerifyNavigationEntries(web_contents->GetController(), url1, url2);
  ASSERT_TRUE(web_contents->GetUserAgentOverride().empty());

  // Restore in a new tab.
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(),
        tab, NEW_BACKGROUND_TAB);
    observer.Wait();
  }
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  web_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  VerifyNavigationEntries(web_contents->GetController(), url1, url2);
  ASSERT_TRUE(web_contents->GetUserAgentOverride().empty());

  // Restore in a new window.
  Browser* new_browser = NULL;
  {
    ui_test_utils::BrowserAddedObserver browser_observer;
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(), tab, NEW_WINDOW);
    new_browser = browser_observer.WaitForSingleNewBrowser();
    observer.Wait();
  }

  ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  web_contents = new_browser->tab_strip_model()->GetWebContentsAt(0);
  VerifyNavigationEntries(web_contents->GetController(), url1, url2);
  ASSERT_TRUE(web_contents->GetUserAgentOverride().empty());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreForeignSession) {
  Profile* profile = browser()->profile();

  GURL url1("http://google.com");
  GURL url2("http://google2.com");
  TabNavigation nav1 =
      SessionTypesTestHelper::CreateNavigation(url1.spec(), "one");
  TabNavigation nav2 =
      SessionTypesTestHelper::CreateNavigation(url2.spec(), "two");
  SessionTypesTestHelper::SetIsOverridingUserAgent(&nav2, true);

  // Set up the restore data -- one window with two tabs.
  std::vector<const SessionWindow*> session;
  SessionWindow window;
  SessionTab tab1;
  {
    sync_pb::SessionTab sync_data;
    sync_data.set_tab_visual_index(0);
    sync_data.set_current_navigation_index(0);
    sync_data.set_pinned(true);
    sync_data.add_navigation()->CopyFrom(nav1.ToSyncData());
    tab1.SetFromSyncData(sync_data, base::Time::Now());
  }
  window.tabs.push_back(&tab1);

  SessionTab tab2;
  {
    sync_pb::SessionTab sync_data;
    sync_data.set_tab_visual_index(1);
    sync_data.set_current_navigation_index(0);
    sync_data.set_pinned(false);
    sync_data.add_navigation()->CopyFrom(nav2.ToSyncData());
    tab2.SetFromSyncData(sync_data, base::Time::Now());
  }
  window.tabs.push_back(&tab2);

  session.push_back(static_cast<const SessionWindow*>(&window));
  ui_test_utils::BrowserAddedObserver window_observer;
  SessionRestore::RestoreForeignSessionWindows(
      profile, browser()->host_desktop_type(), session.begin(), session.end());
  Browser* new_browser = window_observer.WaitForSingleNewBrowser();
  ASSERT_TRUE(new_browser);
  ASSERT_EQ(2u, native_browser_list->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());

  content::WebContents* web_contents_1 =
      new_browser->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* web_contents_2 =
      new_browser->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_EQ(url1, web_contents_1->GetURL());
  ASSERT_EQ(url2, web_contents_2->GetURL());

  // Check user agent override state.
  ASSERT_TRUE(web_contents_1->GetUserAgentOverride().empty());
  ASSERT_TRUE(web_contents_2->GetUserAgentOverride().empty());

  content::NavigationEntry* entry =
      web_contents_1->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);
  ASSERT_FALSE(entry->GetIsOverridingUserAgent());

  entry = web_contents_2->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);
  ASSERT_FALSE(entry->GetIsOverridingUserAgent());

  // The SessionWindow destructor deletes the tabs, so we have to clear them
  // here to avoid a crash.
  window.tabs.clear();
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, Basic) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  ui_test_utils::NavigateToURL(browser(), url2_);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, native_browser_list->size());
  ASSERT_EQ(url2_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  GoBack(new_browser);
  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoresForwardAndBackwardNavs) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  ui_test_utils::NavigateToURL(browser(), url2_);
  ui_test_utils::NavigateToURL(browser(), url3_);

  GoBack(browser());
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, native_browser_list->size());
  ASSERT_EQ(url2_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  GoForward(new_browser);
  ASSERT_EQ(url3_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  GoBack(new_browser);
  ASSERT_EQ(url2_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Test renderer-initiated back/forward as well.
  GURL go_back_url("javascript:history.back();");
  ui_test_utils::NavigateToURL(new_browser, go_back_url);
  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
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
  ASSERT_EQ(1u, native_browser_list->size());
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());

  // Check that back and forward work as expected.
  ASSERT_EQ(cross_site_url,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  GoBack(new_browser);
  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  GoForward(new_browser);
  ASSERT_EQ(cross_site_url,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Test renderer-initiated back/forward as well.
  GURL go_forward_url("javascript:history.forward();");
  ui_test_utils::NavigateToURL(new_browser, go_forward_url);
  ASSERT_EQ(url2_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, TwoTabsSecondSelected) {
  ui_test_utils::NavigateToURL(browser(), url1_);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 2);

  ASSERT_EQ(1u, native_browser_list->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  ASSERT_EQ(1, new_browser->tab_strip_model()->active_index());
  ASSERT_EQ(url2_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
}

// Creates two tabs, closes one, quits and makes sure only one tab is restored.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ClosedTabStaysClosed) {
  ui_test_utils::NavigateToURL(browser(), url1_);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  chrome::CloseTab(browser());

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);

  AssertOneWindowWithOneTab(new_browser);
  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Creates a tabbed browser and popup and makes sure we restore both.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, NormalAndPopup) {
  if (!browser_defaults::kRestorePopups)
    return;  // Test only applicable if restoring popups.

  ui_test_utils::NavigateToURL(browser(), url1_);

  // Make sure we have one window.
  AssertOneWindowWithOneTab(browser());

  // Open a popup.
  Browser* popup = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(),
                            browser()->host_desktop_type()));
  popup->window()->Show();
  ASSERT_EQ(2u, native_browser_list->size());

  ui_test_utils::NavigateToURL(popup, url1_);

  // Simulate an exit by shuting down the session service. If we don't do this
  // the first window close is treated as though the user closed the window
  // and won't be restored.
  SessionServiceFactory::ShutdownForProfile(browser()->profile());

  // Restart and make sure we have two windows.
  QuitBrowserAndRestore(browser(), 1);

  ASSERT_EQ(2u, native_browser_list->size());

  Browser* browser1 = native_browser_list->get(0);
  Browser* browser2 = native_browser_list->get(1);

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
  ASSERT_EQ(2u, native_browser_list->size());

  // Close the first window. The only window left is the App window.
  CloseBrowserSynchronously(browser());

  // Restore the session, which should bring back the first window with url1_.
  Browser* new_browser = QuitBrowserAndRestore(app_window, 1);

  AssertOneWindowWithOneTab(new_browser);

  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

#endif  // !defined(OS_CHROMEOS) && !defined(OS_MACOSX)

// Creates two windows, closes one, restores, make sure only one window open.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, TwoWindowsCloseOneRestoreOnlyOne) {
  ui_test_utils::NavigateToURL(browser(), url1_);

  // Open a second window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kAboutBlankURL), NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);

  ASSERT_EQ(2u, native_browser_list->size());

  // Close it.
  Browser* new_window = native_browser_list->get(1);
  CloseBrowserSynchronously(new_window);

  // Restart and make sure we have only one window with one tab and the url
  // is url1_.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);

  AssertOneWindowWithOneTab(new_browser);

  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
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

  ASSERT_EQ(3, new_browser->tab_strip_model()->count());

  ASSERT_EQ(expected_process_count, RenderProcessHostCount());
}

// Test that changing the user agent override will persist it to disk.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, PersistAndRestoreUserAgentOverride) {
  // Create a tab with an overridden user agent.
  ui_test_utils::NavigateToURL(browser(), url1_);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  browser()->tab_strip_model()->GetWebContentsAt(0)->
      SetUserAgentOverride("override");

  // Create a tab without an overridden user agent.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());

  // Kill the original browser then open a new one to trigger a restore.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, native_browser_list->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  ASSERT_EQ(1, new_browser->tab_strip_model()->active_index());

  // Confirm that the user agent overrides are properly set.
  EXPECT_EQ("override",
            new_browser->tab_strip_model()->GetWebContentsAt(0)->
                GetUserAgentOverride());
  EXPECT_EQ("",
            new_browser->tab_strip_model()->GetWebContentsAt(1)->
                GetUserAgentOverride());
}

// Regression test for crbug.com/125958. When restoring a pinned selected tab in
// a setting where there are existing tabs, the selected index computation was
// wrong, leading to the wrong tab getting selected, DCHECKs firing, and the
// pinned tab not getting loaded.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestorePinnedSelectedTab) {
  // Create a pinned tab.
  ui_test_utils::NavigateToURL(browser(), url1_);
  browser()->tab_strip_model()->SetTabPinned(0, true);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  // Create a nonpinned tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  // Select the pinned tab.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  Profile* profile = browser()->profile();

  // This will also initiate a session restore, but we're not interested in it.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, native_browser_list->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  ASSERT_EQ(0, new_browser->tab_strip_model()->active_index());
  // Close the pinned tab.
  chrome::CloseTab(new_browser);
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  ASSERT_EQ(0, new_browser->tab_strip_model()->active_index());
  // Use the existing tab to navigate away, so that we can verify it was really
  // clobbered.
  ui_test_utils::NavigateToURL(new_browser, url3_);

  // Restore the session again, clobbering the existing tab.
  SessionRestore::RestoreSession(
      profile, new_browser,
      new_browser->host_desktop_type(),
      SessionRestore::CLOBBER_CURRENT_TAB | SessionRestore::SYNCHRONOUS,
      std::vector<GURL>());

  // The pinned tab is the selected tab.
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  EXPECT_EQ(0, new_browser->tab_strip_model()->active_index());
  EXPECT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  EXPECT_EQ(url2_,
            new_browser->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, SessionStorage) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  ASSERT_TRUE(controller->GetDefaultSessionStorageNamespace());
  std::string session_storage_persistent_id =
      controller->GetDefaultSessionStorageNamespace()->persistent_id();
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, native_browser_list->size());
  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  content::NavigationController* new_controller =
      &new_browser->tab_strip_model()->GetActiveWebContents()->GetController();
  ASSERT_TRUE(new_controller->GetDefaultSessionStorageNamespace());
  std::string restored_session_storage_persistent_id =
      new_controller->GetDefaultSessionStorageNamespace()->persistent_id();
  EXPECT_EQ(session_storage_persistent_id,
            restored_session_storage_persistent_id);
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, SessionStorageAfterTabReplace) {
  // Simulate what prerendering does: create a new WebContents with the same
  // SessionStorageNamespace as an existing tab, then replace the tab with it.
  {
    content::NavigationController* controller =
        &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
    ASSERT_TRUE(controller->GetDefaultSessionStorageNamespace());

    content::SessionStorageNamespaceMap session_storage_namespace_map;
    session_storage_namespace_map[""] =
        controller->GetDefaultSessionStorageNamespace();
    scoped_ptr<content::WebContents> web_contents(
        content::WebContents::CreateWithSessionStorage(
            content::WebContents::CreateParams(browser()->profile()),
            session_storage_namespace_map));

    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    scoped_ptr<content::WebContents> old_web_contents(
        tab_strip_model->ReplaceWebContentsAt(
            tab_strip_model->active_index(), web_contents.release()));
    // Navigate with the new tab.
    ui_test_utils::NavigateToURL(browser(), url2_);
    // old_web_contents goes out of scope.
  }

  // Check that the sessionStorage data is going to be persisted.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  EXPECT_TRUE(
      controller->GetDefaultSessionStorageNamespace()->should_persist());

  // Quit and restore. Check that no extra tabs were created.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, native_browser_list->size());
  EXPECT_EQ(1, new_browser->tab_strip_model()->count());
}
