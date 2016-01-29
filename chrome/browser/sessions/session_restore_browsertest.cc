// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "sync/protocol/session_specifics.pb.h"
#include "ui/base/page_transition_types.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;

class SessionRestoreTest : public InProcessBrowserTest {
 public:
  SessionRestoreTest() : active_browser_list_(NULL) {}

 protected:
#if defined(OS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(nkostylev): Investigate if we can remove this switch.
    command_line->AppendSwitch(switches::kCreateBrowserOnStartupForTests);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }
#endif

  void SetUpOnMainThread() override {
    active_browser_list_ = BrowserList::GetInstance();

    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(browser()->profile(), pref);
#if defined(OS_CHROMEOS)
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(), "NoSessionRestoreNewWindowChromeOS") != 0) {
      // Undo the effect of kBrowserAliveWithNoWindows in defaults.cc so that we
      // can get these test to work without quitting.
      SessionServiceTestHelper helper(
          SessionServiceFactory::GetForProfile(browser()->profile()));
      helper.SetForceBrowserNotAliveWithNoWindows(true);
      helper.ReleaseService();
    }
#endif

    InProcessBrowserTest::SetUpOnMainThread();
  }

  bool SetUpUserDataDirectory() override {
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

  Browser* QuitBrowserAndRestore(Browser* browser, int expected_tab_count) {
    return QuitBrowserAndRestoreWithURL(
        browser, expected_tab_count, GURL(), true);
  }

  Browser* QuitBrowserAndRestoreWithURL(Browser* browser,
                                        int expected_tab_count,
                                        const GURL& url,
                                        bool no_memory_pressure) {
    Profile* profile = browser->profile();

    // Close the browser.
    g_browser_process->AddRefModule();
    CloseBrowserSynchronously(browser);

    // Create a new window, which should trigger session restore.
    ui_test_utils::BrowserAddedObserver window_observer;
    SessionRestoreTestHelper restore_observer;
    if (url.is_empty()) {
      chrome::NewEmptyWindow(profile, chrome::HOST_DESKTOP_TYPE_NATIVE);
    } else {
      chrome::NavigateParams params(profile,
                                    url,
                                    ui::PAGE_TRANSITION_LINK);
      chrome::Navigate(&params);
    }
    Browser* new_browser = window_observer.WaitForSingleNewBrowser();
    // Stop loading anything more if we are running out of space.
    if (!no_memory_pressure) {
      base::MemoryPressureListener::NotifyMemoryPressure(
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
    }
    restore_observer.Wait();

    if (no_memory_pressure)
      WaitForTabsToLoad(new_browser);

    g_browser_process->ReleaseModule();

    return new_browser;
  }

  void GoBack(Browser* browser) {
    content::TestNavigationObserver observer(
        browser->tab_strip_model()->GetActiveWebContents());
    chrome::GoBack(browser, CURRENT_TAB);
    observer.Wait();
  }

  void GoForward(Browser* browser) {
    content::TestNavigationObserver observer(
        browser->tab_strip_model()->GetActiveWebContents());
    chrome::GoForward(browser, CURRENT_TAB);
    observer.Wait();
  }

  void AssertOneWindowWithOneTab(Browser* browser) {
    ASSERT_EQ(1u, active_browser_list_->size());
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

  void WaitForTabsToLoad(Browser* browser) {
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      content::WebContents* contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      contents->GetController().LoadIfNecessary();
      content::WaitForLoadStop(contents);
    }
  }

  GURL url1_;
  GURL url2_;
  GURL url3_;

  const BrowserList* active_browser_list_;
};

// Activates the smart restore behaviour and tracks the loading of tabs.
class SmartSessionRestoreTest : public SessionRestoreTest,
                                      public content::NotificationObserver {
 public:
  SmartSessionRestoreTest() {}
  void StartObserving(size_t num_tabs) {
    // Start by clearing everything so it can be reused in the same test.
    web_contents_.clear();
    registrar_.RemoveAll();
    num_tabs_ = num_tabs;
    registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                   content::NotificationService::AllSources());
  }
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case content::NOTIFICATION_LOAD_START: {
        content::NavigationController* controller =
            content::Source<content::NavigationController>(source).ptr();
        web_contents_.push_back(controller->GetWebContents());
        if (web_contents_.size() == num_tabs_)
          message_loop_runner_->Quit();
        break;
      }
    }
  }
  const std::vector<content::WebContents*>& web_contents() const {
    return web_contents_;
  }

  void WaitForAllTabsToStartLoading() {
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

 protected:
  static const size_t kExpectedNumTabs;
  static const char* const kUrls[];

 private:
  content::NotificationRegistrar registrar_;
  // Ordered by load start order.
  std::vector<content::WebContents*> web_contents_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  size_t num_tabs_;

  DISALLOW_COPY_AND_ASSIGN(SmartSessionRestoreTest);
};

// static
const size_t SmartSessionRestoreTest::kExpectedNumTabs = 6;
// static
const char* const SmartSessionRestoreTest::kUrls[] = {
    "http://google.com/1",
    "http://google.com/2",
    "http://google.com/3",
    "http://google.com/4",
    "http://google.com/5",
    "http://google.com/6"};

// Verifies that restored tabs have a root window. This is important
// otherwise the wrong information is communicated to the renderer.
// (http://crbug.com/342672).
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoredTabsShouldHaveWindow) {
  // Create tabs.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Restart and session restore the tabs.
  Browser* restored = QuitBrowserAndRestore(browser(), 3);
  TabStripModel* tab_strip_model = restored->tab_strip_model();
  const int tabs = tab_strip_model->count();
  ASSERT_EQ(3, tabs);

  // Check the restored tabs have a window to get screen info from.
  // On Aura it should also have a root window.
  for (int i = 0; i < tabs; ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    EXPECT_TRUE(contents->GetTopLevelNativeWindow());
#if defined(USE_AURA)
    EXPECT_TRUE(contents->GetNativeView()->GetRootWindow());
#endif
  }
}

// Verify that restored tabs have correct disposition. Only one tab should
// have "visible" visibility state, the rest should not.
// (http://crbug.com/155365 http://crbug.com/118269)
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
    RestoredTabsHaveCorrectVisibilityState) {
  // Create tabs.
  GURL test_page(ui_test_utils::GetTestUrl(base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("tab-restore-visibility.html"))));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_page, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_page, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Restart and session restore the tabs.
  content::DOMMessageQueue message_queue;
  Browser* restored = QuitBrowserAndRestore(browser(), 3);
  for (int i = 0; i < 2; ++i) {
    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"READY\"", message);
  }

  // There should be 3 restored tabs in the new browser.
  TabStripModel* tab_strip_model = restored->tab_strip_model();
  const int tabs = tab_strip_model->count();
  ASSERT_EQ(3, tabs);

  // The middle tab only should have visible disposition.
  for (int i = 0; i < tabs; ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    std::string document_visibility_state;
    const char kGetStateJS[] = "window.domAutomationController.send("
        "window.document.visibilityState);";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetStateJS, &document_visibility_state));
    if (i == 1) {
      EXPECT_EQ("visible", document_visibility_state);
    } else {
      EXPECT_EQ("hidden", document_visibility_state);
    }
  }
}

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
  chrome::AddTabAt(incognito_browser, GURL(), -1, true);
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

// Test that maximized applications get restored maximized.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, MaximizedApps) {
  const char* app_name = "TestApp";
  Browser* app_browser = CreateBrowserForApp(app_name, browser()->profile());
  app_browser->window()->Maximize();
  app_browser->window()->Show();
  EXPECT_TRUE(app_browser->window()->IsMaximized());
  EXPECT_TRUE(app_browser->is_app());
  EXPECT_TRUE(app_browser->is_type_popup());

  // Close the normal browser. After this we only have the app_browser window.
  CloseBrowserSynchronously(browser());

  // Create a new window, which should open NTP.
  ui_test_utils::BrowserAddedObserver browser_added_observer;
  chrome::NewWindow(app_browser);
  Browser* new_browser = browser_added_observer.WaitForSingleNewBrowser();

  ASSERT_TRUE(new_browser);
  EXPECT_TRUE(app_browser->window()->IsMaximized());
  EXPECT_TRUE(app_browser->is_app());
  EXPECT_TRUE(app_browser->is_type_popup());
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

  ASSERT_TRUE(new_browser);

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
  // Any page that will yield a 200 status code will work here.
  GURL url2("chrome://version");
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
                                  ui::PAGE_TRANSITION_LINK);
    observer.Wait();
  }
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), url3,
                                  ui::PAGE_TRANSITION_LINK);
    observer.Wait();
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  service->ClearEntries();

  chrome::HostDesktopType host_desktop_type = browser()->host_desktop_type();

  browser()->window()->Close();

  // Expect a window with three tabs.
  ASSERT_EQ(1U, service->entries().size());
  ASSERT_EQ(sessions::TabRestoreService::WINDOW,
            service->entries().front()->type);
  const sessions::TabRestoreService::Window* window =
      static_cast<sessions::TabRestoreService::Window*>(
          service->entries().front());
  EXPECT_EQ(3U, window->tabs.size());

  // Find the SessionID for entry2. Since the session service was destroyed,
  // there is no guarantee that the SessionID for the tab has remained the same.
  base::Time timestamp;
  int http_status_code = 0;
  for (const sessions::TabRestoreService::Tab& tab : window->tabs) {
    // If this tab held url2, then restore this single tab.
    if (tab.navigations[0].virtual_url() == url2) {
      timestamp = tab.navigations[0].timestamp();
      http_status_code = tab.navigations[0].http_status_code();
      std::vector<sessions::LiveTab*> content =
          service->RestoreEntryById(NULL, tab.id, host_desktop_type, UNKNOWN);
      ASSERT_EQ(1U, content.size());
      sessions::ContentLiveTab* live_tab =
          static_cast<sessions::ContentLiveTab*>(content[0]);
      ASSERT_TRUE(live_tab);
      EXPECT_EQ(url2, live_tab->web_contents()->GetURL());
      break;
    }
  }
  EXPECT_FALSE(timestamp.is_null());
  EXPECT_EQ(200, http_status_code);

  // Make sure that the restored tab is removed from the service.
  ASSERT_EQ(1U, service->entries().size());
  ASSERT_EQ(sessions::TabRestoreService::WINDOW,
            service->entries().front()->type);
  window = static_cast<sessions::TabRestoreService::Window*>(
      service->entries().front());
  EXPECT_EQ(2U, window->tabs.size());

  // Make sure that the restored tab was restored with the correct
  // timestamp and status code.
  const content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  const content::NavigationEntry* entry =
      contents->GetController().GetActiveEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(timestamp, entry->GetTimestamp());
  EXPECT_EQ(http_status_code, entry->GetHttpStatusCode());
}

// Flaky on Linux. https://crbug.com/537592.
#if defined (OS_LINUX)
#define MAYBE_WindowWithOneTab DISABLED_WindowWithOneTab
#else
#define MAYBE_WindowWithOneTab WindowWithOneTab
#endif
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, MAYBE_WindowWithOneTab) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  service->ClearEntries();
  EXPECT_EQ(0U, service->entries().size());

  chrome::HostDesktopType host_desktop_type = browser()->host_desktop_type();

  // Close the window.
  browser()->window()->Close();

  // Expect the window to be converted to a tab by the TRS.
  EXPECT_EQ(1U, service->entries().size());
  ASSERT_EQ(sessions::TabRestoreService::TAB, service->entries().front()->type);
  const sessions::TabRestoreService::Tab* tab =
      static_cast<sessions::TabRestoreService::Tab*>(
          service->entries().front());

  // Restore the tab.
  std::vector<sessions::LiveTab*> content =
      service->RestoreEntryById(NULL, tab->id, host_desktop_type, UNKNOWN);
  ASSERT_EQ(1U, content.size());
  ASSERT_TRUE(content[0]);
  EXPECT_EQ(url, static_cast<sessions::ContentLiveTab*>(content[0])
                     ->web_contents()
                     ->GetURL());

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
  chrome::AddTabAt(incognito_browser, GURL(), -1, true);
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
  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(url1.spec(), "one");
  SerializedNavigationEntry nav2 =
      SerializedNavigationEntryTestHelper::CreateNavigation(url2.spec(), "two");

  // Set up the restore data.
  sync_pb::SessionTab sync_data;
  sync_data.set_tab_visual_index(0);
  sync_data.set_current_navigation_index(1);
  sync_data.set_pinned(false);
  sync_data.add_navigation()->CopyFrom(nav1.ToSyncData());
  sync_data.add_navigation()->CopyFrom(nav2.ToSyncData());

  sessions::SessionTab tab;
  tab.SetFromSyncData(sync_data, base::Time::Now());
  EXPECT_EQ(2U, tab.navigations.size());
  for (size_t i = 0; i < tab.navigations.size(); ++i)
    EXPECT_TRUE(tab.navigations[i].timestamp().is_null());

  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Restore in the current tab.
  content::WebContents* tab_content = NULL;
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    tab_content = SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(), tab, CURRENT_TAB);
    observer.Wait();
  }
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  VerifyNavigationEntries(web_contents->GetController(), url1, url2);
  ASSERT_TRUE(web_contents->GetUserAgentOverride().empty());
  ASSERT_TRUE(tab_content);
  ASSERT_EQ(url2, tab_content->GetURL());

  // Restore in a new tab.
  tab_content = NULL;
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    tab_content = SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(),
        tab, NEW_BACKGROUND_TAB);
    observer.Wait();
  }
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  web_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  VerifyNavigationEntries(web_contents->GetController(), url1, url2);
  ASSERT_TRUE(web_contents->GetUserAgentOverride().empty());
  ASSERT_TRUE(tab_content);
  ASSERT_EQ(url2, tab_content->GetURL());

  // Restore in a new window.
  Browser* new_browser = NULL;
  tab_content = NULL;
  {
    ui_test_utils::BrowserAddedObserver browser_observer;
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    tab_content = SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(), tab, NEW_WINDOW);
    new_browser = browser_observer.WaitForSingleNewBrowser();
    observer.Wait();
  }

  ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  web_contents = new_browser->tab_strip_model()->GetWebContentsAt(0);
  VerifyNavigationEntries(web_contents->GetController(), url1, url2);
  ASSERT_TRUE(web_contents->GetUserAgentOverride().empty());
  ASSERT_TRUE(tab_content);
  ASSERT_EQ(url2, tab_content->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreForeignSession) {
  Profile* profile = browser()->profile();

  GURL url1("http://google.com");
  GURL url2("http://google2.com");
  SerializedNavigationEntry nav1 =
      SerializedNavigationEntryTestHelper::CreateNavigation(url1.spec(), "one");
  SerializedNavigationEntry nav2 =
      SerializedNavigationEntryTestHelper::CreateNavigation(url2.spec(), "two");
  SerializedNavigationEntryTestHelper::SetIsOverridingUserAgent(true, &nav2);

  // Set up the restore data -- one window with two tabs.
  std::vector<const sessions::SessionWindow*> session;
  sessions::SessionWindow window;
  sessions::SessionTab tab1;
  {
    sync_pb::SessionTab sync_data;
    sync_data.set_tab_visual_index(0);
    sync_data.set_current_navigation_index(0);
    sync_data.set_pinned(true);
    sync_data.add_navigation()->CopyFrom(nav1.ToSyncData());
    tab1.SetFromSyncData(sync_data, base::Time::Now());
  }
  window.tabs.push_back(&tab1);

  sessions::SessionTab tab2;
  {
    sync_pb::SessionTab sync_data;
    sync_data.set_tab_visual_index(1);
    sync_data.set_current_navigation_index(0);
    sync_data.set_pinned(false);
    sync_data.add_navigation()->CopyFrom(nav2.ToSyncData());
    tab2.SetFromSyncData(sync_data, base::Time::Now());
  }
  window.tabs.push_back(&tab2);

  // Leave tab3 empty. Should have no effect on restored session, but simulates
  // partially complete foreign session data.
  sessions::SessionTab tab3;
  window.tabs.push_back(&tab3);

  session.push_back(static_cast<const sessions::SessionWindow*>(&window));
  ui_test_utils::BrowserAddedObserver window_observer;
  std::vector<Browser*> browsers =
      SessionRestore::RestoreForeignSessionWindows(
          profile, browser()->host_desktop_type(), session.begin(),
          session.end());
  Browser* new_browser = window_observer.WaitForSingleNewBrowser();
  ASSERT_TRUE(new_browser);
  ASSERT_EQ(2u, active_browser_list_->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());

  ASSERT_EQ(1u, browsers.size());
  ASSERT_TRUE(browsers[0]);
  ASSERT_EQ(2, browsers[0]->tab_strip_model()->count());

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
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(url2_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  GoBack(new_browser);
  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, NoMemoryPressureLoadsAllTabs) {
  // Add several tabs to the browser. Restart the browser and check that all
  // tabs got loaded properly.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  Browser* restored =
      QuitBrowserAndRestoreWithURL(browser(), 1, GURL(), true);
  TabStripModel* tab_strip_model = restored->tab_strip_model();

  ASSERT_EQ(1u, active_browser_list_->size());

  ASSERT_EQ(3, tab_strip_model->count());
  // All render widgets should be initialized by now.
  ASSERT_TRUE(
      tab_strip_model->GetWebContentsAt(0)->GetRenderWidgetHostView() &&
      tab_strip_model->GetWebContentsAt(1)->GetRenderWidgetHostView() &&
      tab_strip_model->GetWebContentsAt(2)->GetRenderWidgetHostView());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, MemoryPressureLoadsNotAllTabs) {
  // Add several tabs to the browser. Restart the browser and check that all
  // tabs got loaded properly.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  // Restore the brwoser, but instead of directly waiting, we issue a critical
  // memory pressure event and finish then the loading.
  Browser* restored =
      QuitBrowserAndRestoreWithURL(browser(), 1, GURL(), false);

  TabStripModel* tab_strip_model = restored->tab_strip_model();

  ASSERT_EQ(1u, active_browser_list_->size());

  ASSERT_EQ(3, tab_strip_model->count());
  // At least one of the render widgets should not be initialized yet.
  ASSERT_FALSE(
      tab_strip_model->GetWebContentsAt(0)->GetRenderWidgetHostView() &&
      tab_strip_model->GetWebContentsAt(1)->GetRenderWidgetHostView() &&
      tab_strip_model->GetWebContentsAt(2)->GetRenderWidgetHostView());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreWebUI) {
  const GURL webui_url("chrome://omnibox");
  ui_test_utils::NavigateToURL(browser(), webui_url);
  const content::WebContents* old_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(content::BINDINGS_POLICY_WEB_UI,
            old_tab->GetRenderViewHost()->GetEnabledBindings());

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  const content::WebContents* new_tab =
      new_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(webui_url, new_tab->GetURL());
  EXPECT_EQ(content::BINDINGS_POLICY_WEB_UI,
            new_tab->GetRenderViewHost()->GetEnabledBindings());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreWebUISettings) {
  const GURL webui_url("chrome://settings");
  ui_test_utils::NavigateToURL(browser(), webui_url);
  const content::WebContents* old_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(content::BINDINGS_POLICY_WEB_UI,
            old_tab->GetRenderViewHost()->GetEnabledBindings());

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  const content::WebContents* new_tab =
      new_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(webui_url, new_tab->GetURL());
  EXPECT_EQ(content::BINDINGS_POLICY_WEB_UI,
            new_tab->GetRenderViewHost()->GetEnabledBindings());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoresForwardAndBackwardNavs) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  ui_test_utils::NavigateToURL(browser(), url2_);
  ui_test_utils::NavigateToURL(browser(), url3_);

  GoBack(browser());
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
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
// This test fails. See http://crbug.com/237497.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       DISABLED_RestoresCrossSiteForwardAndBackwardNavs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL cross_site_url(embedded_test_server()->GetURL("/title2.html"));

  // Visit URLs on different sites.
  ui_test_utils::NavigateToURL(browser(), url1_);
  ui_test_utils::NavigateToURL(browser(), cross_site_url);
  ui_test_utils::NavigateToURL(browser(), url2_);

  GoBack(browser());
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
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

  ASSERT_EQ(1u, active_browser_list_->size());
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

// Ensures active tab properly restored when tabs before it closed.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ActiveIndexUpdatedAtClose) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url3_, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  browser()->tab_strip_model()->CloseWebContentsAt(
      0,
      TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 2);

  ASSERT_EQ(url2_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  ASSERT_EQ(new_browser->tab_strip_model()->active_index(), 0);
}

// Ensures active tab properly restored when tabs are inserted before it .
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ActiveIndexUpdatedAtInsert) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  chrome::NavigateParams navigate_params(browser(), url3_,
                                         ui::PAGE_TRANSITION_TYPED);
  navigate_params.tabstrip_index = 0;
  navigate_params.disposition = NEW_BACKGROUND_TAB;
  ui_test_utils::NavigateToURL(&navigate_params);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 3);

  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  ASSERT_EQ(new_browser->tab_strip_model()->active_index(), 1);
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
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  ui_test_utils::NavigateToURL(browser(), url1_);

  // Launch an app.
  base::CommandLine app_launch_arguments = GetCommandLineForRelaunch();
  app_launch_arguments.AppendSwitchASCII(switches::kApp, url2_.spec());

  ui_test_utils::BrowserAddedObserver window_observer;

  base::LaunchProcess(app_launch_arguments, base::LaunchOptionsForTest());

  Browser* app_window = window_observer.WaitForSingleNewBrowser();
  ASSERT_EQ(2u, active_browser_list_->size());

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
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);

  ASSERT_EQ(2u, active_browser_list_->size());

  // Close it.
  Browser* new_window = active_browser_list_->get(1);
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
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(url::kAboutBlankURL),
      NEW_FOREGROUND_TAB,
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
  ASSERT_EQ(1u, active_browser_list_->size());
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
  ASSERT_EQ(1u, active_browser_list_->size());
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

// Regression test for crbug.com/240156. When restoring tabs with a navigation,
// the navigation should take active tab focus.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreWithNavigateSelectedTab) {
  // Create 2 tabs.
  ui_test_utils::NavigateToURL(browser(), url1_);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Restore the session by calling chrome::Navigate().
  Browser* new_browser =
      QuitBrowserAndRestoreWithURL(browser(), 3, url3_, true);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(3, new_browser->tab_strip_model()->count());
  // Navigated url should be the active tab.
  ASSERT_EQ(url3_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Do a clobber restore from the new tab page. This test follows the code path
// of a crash followed by the user clicking restore from the new tab page.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ClobberRestoreTest) {
  // Create 2 tabs.
  ui_test_utils::NavigateToURL(browser(), url1_);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  Profile* profile = browser()->profile();

  // This will also initiate a session restore, but we're not interested in it.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  ASSERT_EQ(1, new_browser->tab_strip_model()->active_index());
  // Close the first tab.
  chrome::CloseTab(new_browser);
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  ASSERT_EQ(0, new_browser->tab_strip_model()->active_index());
  // Use the existing tab to navigate to the NTP.
  ui_test_utils::NavigateToURL(new_browser, GURL(chrome::kChromeUINewTabURL));

  // Restore the session again, clobbering the existing tab.
  SessionRestore::RestoreSession(
      profile, new_browser,
      new_browser->host_desktop_type(),
      SessionRestore::CLOBBER_CURRENT_TAB | SessionRestore::SYNCHRONOUS,
      std::vector<GURL>());

  // 2 tabs should have been restored, with the existing tab clobbered, giving
  // us a total of 2 tabs.
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  EXPECT_EQ(1, new_browser->tab_strip_model()->active_index());
  EXPECT_EQ(url1_,
            new_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(url2_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, SessionStorage) {
  ui_test_utils::NavigateToURL(browser(), url1_);
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  ASSERT_TRUE(controller->GetDefaultSessionStorageNamespace());
  std::string session_storage_persistent_id =
      controller->GetDefaultSessionStorageNamespace()->persistent_id();
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
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
    session_storage_namespace_map[std::string()] =
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
  ASSERT_EQ(1u, active_browser_list_->size());
  EXPECT_EQ(1, new_browser->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(SmartSessionRestoreTest, PRE_CorrectLoadingOrder) {
  Profile* profile = browser()->profile();

  const int activation_order[] = {4, 2, 1, 5, 0, 3};

  // Replace the first tab and add the other tabs.
  ui_test_utils::NavigateToURL(browser(), GURL(kUrls[0]));
  for (size_t i = 1; i < kExpectedNumTabs; i++) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(kUrls[i]), NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }

  ASSERT_EQ(static_cast<int>(kExpectedNumTabs),
            browser()->tab_strip_model()->count());

  // Activate the tabs one by one following the specified activation order.
  for (int i : activation_order)
    browser()->tab_strip_model()->ActivateTabAt(i, true);

  // Close the browser.
  g_browser_process->AddRefModule();
  CloseBrowserSynchronously(browser());

  StartObserving(kExpectedNumTabs);

  // Create a new window, which should trigger session restore.
  ui_test_utils::BrowserAddedObserver window_observer;
  chrome::NewEmptyWindow(profile, chrome::HOST_DESKTOP_TYPE_NATIVE);
  Browser* new_browser = window_observer.WaitForSingleNewBrowser();
  ASSERT_TRUE(new_browser);
  WaitForAllTabsToStartLoading();
  g_browser_process->ReleaseModule();

  ASSERT_EQ(kExpectedNumTabs, web_contents().size());
  // Test that we have observed the tabs being loaded in the inverse order of
  // their activation (MRU). Also validate that their last active time is in the
  // correct order.
  for (size_t i = 0; i < web_contents().size(); i++) {
    GURL expected_url = GURL(kUrls[activation_order[kExpectedNumTabs - i - 1]]);
    ASSERT_EQ(expected_url, web_contents()[i]->GetLastCommittedURL());
    if (i > 0) {
      ASSERT_GT(web_contents()[i - 1]->GetLastActiveTime(),
                web_contents()[i]->GetLastActiveTime());
    }
  }

  // Activate the 2nd tab before the browser closes. This should be persisted in
  // the following test.
  new_browser->tab_strip_model()->ActivateTabAt(1, true);
}

// PRE_CorrectLoadingOrder is flaky on ChromeOS MSAN. https://crbug.com/582323.
#if defined (OS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_CorrectLoadingOrder DISABLED_CorrectLoadingOrder
#else
#define MAYBE_CorrectLoadingOrder CorrectLoadingOrder
#endif
IN_PROC_BROWSER_TEST_F(SmartSessionRestoreTest, MAYBE_CorrectLoadingOrder) {
  const int activation_order[] = {4, 2, 5, 0, 3, 1};
  Profile* profile = browser()->profile();

  // Close the browser that gets opened automatically so we can track the order
  // of loading of the tabs.
  g_browser_process->AddRefModule();
  CloseBrowserSynchronously(browser());
  // We have an extra tab that is added when the test starts, which gets ignored
  // later when we test for proper order.
  StartObserving(kExpectedNumTabs + 1);

  // Create a new window, which should trigger session restore.
  ui_test_utils::BrowserAddedObserver window_observer;
  chrome::NewEmptyWindow(profile, chrome::HOST_DESKTOP_TYPE_NATIVE);
  Browser* new_browser = window_observer.WaitForSingleNewBrowser();
  ASSERT_TRUE(new_browser);
  WaitForAllTabsToStartLoading();
  g_browser_process->ReleaseModule();

  ASSERT_EQ(kExpectedNumTabs + 1, web_contents().size());

  // Test that we have observed the tabs being loaded in the inverse order of
  // their activation (MRU). Also validate that their last active time is in the
  // correct order.
  //
  // Note that we ignore the first tab as it's an empty one that is added
  // automatically at the start of the test.
  for (size_t i = 1; i < web_contents().size(); i++) {
    GURL expected_url = GURL(kUrls[activation_order[kExpectedNumTabs - i]]);
    ASSERT_EQ(expected_url, web_contents()[i]->GetLastCommittedURL());
    if (i > 0) {
      ASSERT_GT(web_contents()[i - 1]->GetLastActiveTime(),
                web_contents()[i]->GetLastActiveTime());
    }
  }
}
