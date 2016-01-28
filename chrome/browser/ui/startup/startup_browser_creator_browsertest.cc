// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/metrics_pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;
#endif  // defined(ENABLE_CONFIGURATION_POLICY) && !defined(OS_CHROMEOS)

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using extensions::Extension;

namespace {

// Check that there are two browsers. Find the one that is not |browser|.
Browser* FindOneOtherBrowser(Browser* browser) {
  // There should only be one other browser.
  EXPECT_EQ(2u, chrome::GetBrowserCount(browser->profile(),
                                        browser->host_desktop_type()));

  // Find the new browser.
  Browser* other_browser = NULL;
  for (auto* b : *BrowserList::GetInstance()) {
    if (b != browser)
      other_browser = b;
  }
  return other_browser;
}

bool IsWindows10OrNewer() {
#if defined(OS_WIN)
  return base::win::GetVersion() >= base::win::VERSION_WIN10;
#else
  return false;
#endif
}

#if defined(OS_WIN)
// This function is used to verify a callback was successfully invoked.
void SetTrue(bool* value) {
  ASSERT_TRUE(value);
  *value = true;
}

bool TabStripContainsUrl(TabStripModel* tab_strip, GURL url) {
  for (int i = 0; i < tab_strip->count(); ++i) {
    if (tab_strip->GetWebContentsAt(i)->GetURL() == url)
      return true;
  }
  return false;
}

void ProcessCommandLineAlreadyRunningDefaultProfile(
    const base::CommandLine& cmdline) {
  StartupBrowserCreator browser_creator;

  base::FilePath current_dir;
  ASSERT_TRUE(base::GetCurrentDirectory(&current_dir));
  base::FilePath user_data_dir =
      g_browser_process->profile_manager()->user_data_dir();
  base::FilePath startup_profile_dir =
      g_browser_process->profile_manager()->GetLastUsedProfileDir(
          user_data_dir);

  browser_creator.ProcessCommandLineAlreadyRunning(cmdline, current_dir,
                                                   startup_profile_dir);
}
#endif  // defined(OS_WIN)

GURL GetSigninPromoURL() {
  return signin::GetPromoURL(
      signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT, false);
}

}  // namespace

class StartupBrowserCreatorTest : public ExtensionBrowserTest {
 protected:
  StartupBrowserCreatorTest() {}

  bool SetUpUserDataDirectory() override {
    return ExtensionBrowserTest::SetUpUserDataDirectory();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
    command_line->AppendSwitchASCII(switches::kHomePage, url::kAboutBlankURL);
#if defined(OS_CHROMEOS)
    // TODO(nkostylev): Investigate if we can remove this switch.
    command_line->AppendSwitch(switches::kCreateBrowserOnStartupForTests);
#endif
  }

  // Helper functions return void so that we can ASSERT*().
  // Use ASSERT_NO_FATAL_FAILURE around calls to these functions to stop the
  // test if an assert fails.
  void LoadApp(const std::string& app_name,
               const Extension** out_app_extension) {
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(app_name.c_str())));

    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    *out_app_extension = service->GetExtensionById(
        last_loaded_extension_id(), false);
    ASSERT_TRUE(*out_app_extension);

    // Code that opens a new browser assumes we start with exactly one.
    ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile(),
                                          browser()->host_desktop_type()));
  }

  void SetAppLaunchPref(const std::string& app_id,
                        extensions::LaunchType launch_type) {
    extensions::SetLaunchType(browser()->profile(), app_id, launch_type);
  }

  Browser* FindOneOtherBrowserForProfile(Profile* profile,
                                         Browser* not_this_browser) {
    for (auto* browser : *BrowserList::GetInstance()) {
      if (browser != not_this_browser && browser->profile() == profile)
        return browser;
    }
    return NULL;
  }

  // A helper function that checks the session restore UI (infobar) is shown
  // when Chrome starts up after crash.
  void EnsureRestoreUIWasShown(content::WebContents* web_contents) {
#if defined(OS_MACOSX)
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    EXPECT_EQ(1U, infobar_service->infobar_count());
#endif  // defined(OS_MACOSX)
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StartupBrowserCreatorTest);
};

class OpenURLsPopupObserver : public chrome::BrowserListObserver {
 public:
  OpenURLsPopupObserver() : added_browser_(NULL) { }

  void OnBrowserAdded(Browser* browser) override { added_browser_ = browser; }

  void OnBrowserRemoved(Browser* browser) override {}

  Browser* added_browser_;
};

// Test that when there is a popup as the active browser any requests to
// StartupBrowserCreatorImpl::OpenURLsInBrowser don't crash because there's no
// explicit profile given.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenURLsPopup) {
  std::vector<GURL> urls;
  urls.push_back(GURL("http://localhost"));

  // Note that in our testing we do not ever query the BrowserList for the "last
  // active" browser. That's because the browsers are set as "active" by
  // platform UI toolkit messages, and those messages are not sent during unit
  // testing sessions.

  OpenURLsPopupObserver observer;
  BrowserList::AddObserver(&observer);

  Browser* popup = new Browser(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(),
                            browser()->host_desktop_type()));
  ASSERT_TRUE(popup->is_type_popup());
  ASSERT_EQ(popup, observer.added_browser_);

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  // This should create a new window, but re-use the profile from |popup|. If
  // it used a NULL or invalid profile, it would crash.
  launch.OpenURLsInBrowser(popup, false, urls, chrome::GetActiveDesktop());
  ASSERT_NE(popup, observer.added_browser_);
  BrowserList::RemoveObserver(&observer);
}

// We don't do non-process-startup browser launches on ChromeOS.
// Session restore for process-startup browser launches is tested
// in session_restore_uitest.
#if !defined(OS_CHROMEOS)
// Verify that startup URLs are honored when the process already exists but has
// no tabbed browser windows (eg. as if the process is running only due to a
// background application.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       StartupURLsOnNewWindowWithNoTabbedBrowsers) {
  // Use a couple same-site HTTP URLs.
  ASSERT_TRUE(embedded_test_server()->Start());
  std::vector<GURL> urls;
  urls.push_back(embedded_test_server()->GetURL("/title1.html"));
  urls.push_back(embedded_test_server()->GetURL("/title2.html"));

  Profile* profile = browser()->profile();
  chrome::HostDesktopType host_desktop_type = browser()->host_desktop_type();

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(profile, pref);

  // Keep the browser process running while browsers are closed.
  g_browser_process->AddRefModule();

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  {
    StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
    ASSERT_TRUE(
        launch.Launch(profile, std::vector<GURL>(), false, host_desktop_type));
  }

  // This should have created a new browser window.  |browser()| is still
  // around at this point, even though we've closed its window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  std::vector<GURL> expected_urls(urls);
  if (IsWindows10OrNewer())
    expected_urls.insert(expected_urls.begin(), internals::GetWelcomePageURL());

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(static_cast<int>(expected_urls.size()), tab_strip->count());
  for (size_t i = 0; i < expected_urls.size(); i++)
    EXPECT_EQ(expected_urls[i], tab_strip->GetWebContentsAt(i)->GetURL());

  // The two test_server tabs, despite having the same site, should be in
  // different SiteInstances.
  EXPECT_NE(
      tab_strip->GetWebContentsAt(tab_strip->count() - 2)->GetSiteInstance(),
      tab_strip->GetWebContentsAt(tab_strip->count() - 1)->GetSiteInstance());

  // Test that the welcome page is not shown the second time through if it was
  // above.
  if (IsWindows10OrNewer()) {
    // Close the browser opened above.
    {
      content::WindowedNotificationObserver observer(
          chrome::NOTIFICATION_BROWSER_CLOSED,
          content::Source<Browser>(new_browser));
      new_browser->window()->Close();
      observer.Wait();
    }

    {
      StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
      ASSERT_TRUE(launch.Launch(profile, std::vector<GURL>(), false,
                                host_desktop_type));
    }

    // Find the new browser and ensure that it has only the specified URLs this
    // time. Both the original browser created by the fixture and the one
    // created above have been closed, so the new browser is the only one
    // remaining.
    new_browser = FindTabbedBrowser(profile, true, host_desktop_type);
    ASSERT_TRUE(new_browser);
    ASSERT_EQ(static_cast<int>(urls.size()),
              new_browser->tab_strip_model()->count());
  }

  g_browser_process->ReleaseModule();
}

// Verify that startup URLs aren't used when the process already exists
// and has other tabbed browser windows.  This is the common case of starting a
// new browser.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       StartupURLsOnNewWindow) {
  // Use a couple arbitrary URLs.
  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  {
    StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
    ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                              browser()->host_desktop_type()));
  }

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  if (IsWindows10OrNewer()) {
    // The new browser should have two tabs (not the startup URLs).
    ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  } else {
    // The new browser should have exactly one tab (not the startup URLs).
    ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  }

  // Test that the welcome page is not shown the second time through if it was
  // above.
  if (!IsWindows10OrNewer()) {
    // Close the browser opened above.
    {
      content::WindowedNotificationObserver observer(
          chrome::NOTIFICATION_BROWSER_CLOSED,
          content::Source<Browser>(new_browser));
      new_browser->window()->Close();
      observer.Wait();
    }

    {
      StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
      ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(),
                                false, browser()->host_desktop_type()));
    }

    // Find the new browser and ensure that it has only the one tab this time.
    new_browser = FindOneOtherBrowser(browser());
    ASSERT_TRUE(new_browser);
    ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  }
}

// App shortcuts are not implemented on mac os.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppShortcutNoPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Add --app-id=<extension->id()> to the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // No pref was set, so the app should have opened in a tab in a new window.
  // The launch should have created a new browser.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // If new bookmark apps are enabled, it should be a standard tabbed window,
  // not an app window; otherwise the reverse should be true.
  bool new_bookmark_apps_enabled = extensions::util::IsNewBookmarkAppsEnabled();
  EXPECT_EQ(!new_bookmark_apps_enabled, new_browser->is_app());
  EXPECT_EQ(new_bookmark_apps_enabled, new_browser->is_type_tabbed());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppShortcutWindowPref) {
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_WINDOW);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // Pref was set to open in a window, so the app should have opened in a
  // window.  The launch should have created a new browser. Find the new
  // browser.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Expect an app window.
  EXPECT_TRUE(new_browser->is_app());

  // The browser's app_name should include the app's ID.
  EXPECT_NE(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppShortcutTabPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_REGULAR);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // When an app shortcut is open and the pref indicates a tab should
  // open, the tab is open in a new browser window.  Expect a new window.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile(),
                                        browser()->host_desktop_type()));

  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // The tab should be in a tabbed window.
  EXPECT_TRUE(new_browser->is_type_tabbed());

  // The browser's app_name should not include the app's ID: It is in a
  // normal browser.
  EXPECT_EQ(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

#endif  // !defined(OS_MACOSX)

#endif  // !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ReadingWasRestartedAfterRestart) {
  // Tests that StartupBrowserCreator::WasRestarted reads and resets the
  // preference kWasRestarted correctly.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);
  EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_TRUE(StartupBrowserCreator::WasRestarted());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ReadingWasRestartedAfterNormalStart) {
  // Tests that StartupBrowserCreator::WasRestarted reads and resets the
  // preference kWasRestarted correctly.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, false);
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_FALSE(StartupBrowserCreator::WasRestarted());
}

// Fails on official builds. See http://crbug.com/313856
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_AddFirstRunTab DISABLED_AddFirstRunTab
#else
#define MAYBE_AddFirstRunTab AddFirstRunTab
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, MAYBE_AddFirstRunTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title2.html"));

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ(2, tab_strip->count());

  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
  EXPECT_EQ("title2.html",
            tab_strip->GetWebContentsAt(1)->GetURL().ExtractFileName());
}

// Test hard-coded special first run tabs (defined in
// StartupBrowserCreatorImpl::AddStartupURLs()).
// Fails on official builds. See http://crbug.com/313856
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_AddCustomFirstRunTab DISABLED_AddCustomFirstRunTab
#else
#define MAYBE_AddCustomFirstRunTab AddCustomFirstRunTab
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, MAYBE_AddCustomFirstRunTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser_creator.AddFirstRunTab(GURL("http://new_tab_page"));
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title2.html"));
  browser_creator.AddFirstRunTab(GURL("http://welcome_page"));

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ(4, tab_strip->count());

  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            tab_strip->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ("title2.html",
            tab_strip->GetWebContentsAt(2)->GetURL().ExtractFileName());
  EXPECT_EQ(internals::GetWelcomePageURL(),
            tab_strip->GetWebContentsAt(3)->GetURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, SyncPromoNoWelcomePage) {
  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();

  if (signin::ShouldShowPromoAtStartup(browser()->profile(), true)) {
    // The browser should show only the promo.
    ASSERT_EQ(1, tab_strip->count());
    EXPECT_EQ(GetSigninPromoURL(), tab_strip->GetWebContentsAt(0)->GetURL());
  } else if (IsWindows10OrNewer()) {
    // The browser should show the welcome page and the NTP.
    ASSERT_EQ(2, tab_strip->count());
    EXPECT_EQ(GURL(internals::GetWelcomePageURL()),
              tab_strip->GetWebContentsAt(0)->GetURL());
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(1)->GetURL());
  } else {
    // The browser should show only the NTP.
    ASSERT_EQ(1, tab_strip->count());
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(0)->GetURL());
  }
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, SyncPromoWithWelcomePage) {
  first_run::SetShouldShowWelcomePage();

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ(2, tab_strip->count());

  if (IsWindows10OrNewer()) {
    EXPECT_EQ(internals::GetWelcomePageURL(),
              tab_strip->GetWebContentsAt(0)->GetURL());
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(1)->GetURL());
  } else {
    if (signin::ShouldShowPromoAtStartup(browser()->profile(), true)) {
      EXPECT_EQ(GetSigninPromoURL(), tab_strip->GetWebContentsAt(0)->GetURL());
    } else {
      EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
                tab_strip->GetWebContentsAt(0)->GetURL());
    }
    EXPECT_EQ(internals::GetWelcomePageURL(),
              tab_strip->GetWebContentsAt(1)->GetURL());
  }
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, SyncPromoWithFirstRunTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));

  // The welcome page should not be shown, even if
  // first_run::ShouldShowWelcomePage() says so, when there are already
  // more than 2 first run tabs.
  first_run::SetShouldShowWelcomePage();

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  if (signin::ShouldShowPromoAtStartup(browser()->profile(), true)) {
    EXPECT_EQ(2, tab_strip->count());
    EXPECT_EQ(GetSigninPromoURL(), tab_strip->GetWebContentsAt(0)->GetURL());
    EXPECT_EQ("title1.html",
              tab_strip->GetWebContentsAt(1)->GetURL().ExtractFileName());
  } else {
    EXPECT_EQ(1, tab_strip->count());
    EXPECT_EQ("title1.html",
              tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
  }
}

// The welcome page should still be shown if there are more than 2 first run
// tabs, but the welcome page was explcitly added to the first run tabs.
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       SyncPromoWithFirstRunTabsIncludingWelcomePage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser_creator.AddFirstRunTab(GURL("http://welcome_page"));

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  if (signin::ShouldShowPromoAtStartup(browser()->profile(), true)) {
    EXPECT_EQ(3, tab_strip->count());
    EXPECT_EQ(GetSigninPromoURL(), tab_strip->GetWebContentsAt(0)->GetURL());
    EXPECT_EQ("title1.html",
              tab_strip->GetWebContentsAt(1)->GetURL().ExtractFileName());
    EXPECT_EQ(internals::GetWelcomePageURL(),
              tab_strip->GetWebContentsAt(2)->GetURL());
  } else {
    EXPECT_EQ(2, tab_strip->count());
    EXPECT_EQ("title1.html",
              tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
    EXPECT_EQ(internals::GetWelcomePageURL(),
              tab_strip->GetWebContentsAt(1)->GetURL());
  }
}

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, StartupURLsForTwoProfiles) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  Profile* default_profile = browser()->profile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create another profile.
  base::FilePath dest_path = profile_manager->user_data_dir();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile 1"));

  Profile* other_profile = profile_manager->GetProfile(dest_path);
  ASSERT_TRUE(other_profile);

  // Use a couple arbitrary URLs.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(default_profile, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(other_profile, pref2);

  // Close the browser.
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(default_profile);
  last_opened_profiles.push_back(other_profile);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        default_profile, last_opened_profiles);

  // urls1 were opened in a browser for default_profile, and urls2 were opened
  // in a browser for other_profile.
  Browser* new_browser = NULL;
  // |browser()| is still around at this point, even though we've closed its
  // window. Thus the browser count for default_profile is 2.
  ASSERT_EQ(2u, chrome::GetBrowserCount(default_profile,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(default_profile, browser());
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  if (IsWindows10OrNewer()) {
    // The new browser should have the welcome tab and the URL for the profile.
    ASSERT_EQ(2, tab_strip->count());
    EXPECT_EQ(GURL(internals::GetWelcomePageURL()),
              tab_strip->GetWebContentsAt(0)->GetURL());
    EXPECT_EQ(urls1[0], tab_strip->GetWebContentsAt(1)->GetURL());
  } else {
    // The new browser should have only the desired URL for the profile.
    ASSERT_EQ(1, tab_strip->count());
    EXPECT_EQ(urls1[0], tab_strip->GetWebContentsAt(0)->GetURL());
  }

  ASSERT_EQ(1u, chrome::GetBrowserCount(other_profile,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(other_profile, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls2[0], tab_strip->GetWebContentsAt(0)->GetURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, PRE_UpdateWithTwoProfiles) {
  // Simulate a browser restart by creating the profiles in the PRE_ part.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Create two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = profile_manager->GetProfile(
      dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  ASSERT_TRUE(profile1);

  Profile* profile2 = profile_manager->GetProfile(
      dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
  ASSERT_TRUE(profile2);

  // Open some urls with the browsers, and close them.
  Browser* browser1 = new Browser(
      Browser::CreateParams(Browser::TYPE_TABBED, profile1,
                            browser()->host_desktop_type()));
  chrome::NewTab(browser1);
  ui_test_utils::NavigateToURL(browser1,
                               embedded_test_server()->GetURL("/empty.html"));
  CloseBrowserSynchronously(browser1);

  Browser* browser2 = new Browser(
      Browser::CreateParams(Browser::TYPE_TABBED, profile2,
                            browser()->host_desktop_type()));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2,
                               embedded_test_server()->GetURL("/form.html"));
  CloseBrowserSynchronously(browser2);

  // Set different startup preferences for the 2 profiles.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(profile2, pref2);

  profile1->GetPrefs()->CommitPendingWrite();
  profile2->GetPrefs()->CommitPendingWrite();
}

// See crbug.com/376184 about improvements to this test on Mac.
// Disabled because it's flaky. http://crbug.com/379579
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       DISABLED_UpdateWithTwoProfiles) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  // Make StartupBrowserCreator::WasRestarted() return true.
  StartupBrowserCreator::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Open the two profiles.
  base::FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = profile_manager->GetProfile(
      dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  ASSERT_TRUE(profile1);

  Profile* profile2 = profile_manager->GetProfile(
      dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
  ASSERT_TRUE(profile2);

  // Simulate a launch after a browser update.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile1);
  last_opened_profiles.push_back(profile2);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile1,
                        last_opened_profiles);

  while (SessionRestore::IsRestoring(profile1) ||
         SessionRestore::IsRestoring(profile2))
    base::MessageLoop::current()->RunUntilIdle();

  // The startup URLs are ignored, and instead the last open sessions are
  // restored.
  EXPECT_TRUE(profile1->restored_last_session());
  EXPECT_TRUE(profile2->restored_last_session());

  Browser* new_browser = NULL;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile1,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(profile1, NULL);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(profile2, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/form.html", tab_strip->GetWebContentsAt(0)->GetURL().path());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ProfilesWithoutPagesNotLaunched) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif
  ASSERT_TRUE(embedded_test_server()->Start());

  Profile* default_profile = browser()->profile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 4 more profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  base::FilePath dest_path3 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 3"));
  base::FilePath dest_path4 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 4"));

  Profile* profile_home1 = profile_manager->GetProfile(dest_path1);
  ASSERT_TRUE(profile_home1);
  Profile* profile_home2 = profile_manager->GetProfile(dest_path2);
  ASSERT_TRUE(profile_home2);
  Profile* profile_last = profile_manager->GetProfile(dest_path3);
  ASSERT_TRUE(profile_last);
  Profile* profile_urls = profile_manager->GetProfile(dest_path4);
  ASSERT_TRUE(profile_urls);

  // Set the profiles to open urls, open last visited pages or display the home
  // page.
  SessionStartupPref pref_home(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(profile_home1, pref_home);
  SessionStartupPref::SetStartupPref(profile_home2, pref_home);

  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile_last, pref_last);

  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls = urls;
  SessionStartupPref::SetStartupPref(profile_urls, pref_urls);

  // Open a page with profile_last.
  Browser* browser_last = new Browser(
      Browser::CreateParams(Browser::TYPE_TABBED, profile_last,
                            browser()->host_desktop_type()));
  chrome::NewTab(browser_last);
  ui_test_utils::NavigateToURL(browser_last,
                               embedded_test_server()->GetURL("/empty.html"));
  CloseBrowserAsynchronously(browser_last);

  // Close the main browser.
  chrome::HostDesktopType original_desktop_type =
      browser()->host_desktop_type();
  CloseBrowserAsynchronously(browser());

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);

  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile_home1);
  last_opened_profiles.push_back(profile_home2);
  last_opened_profiles.push_back(profile_last);
  last_opened_profiles.push_back(profile_urls);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile_home1,
                        last_opened_profiles);

  while (SessionRestore::IsRestoring(default_profile) ||
         SessionRestore::IsRestoring(profile_home1) ||
         SessionRestore::IsRestoring(profile_home2) ||
         SessionRestore::IsRestoring(profile_last) ||
         SessionRestore::IsRestoring(profile_urls))
    base::MessageLoop::current()->RunUntilIdle();

  Browser* new_browser = NULL;
  // The last open profile (the profile_home1 in this case) will always be
  // launched, even if it will open just the NTP (and the welcome page on
  // relevant platforms).
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_home1, original_desktop_type));
  new_browser = FindOneOtherBrowserForProfile(profile_home1, NULL);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  if (IsWindows10OrNewer()) {
    // The new browser should have the welcome tab and the NTP.
    ASSERT_EQ(2, tab_strip->count());
    EXPECT_EQ(GURL(internals::GetWelcomePageURL()),
              tab_strip->GetWebContentsAt(0)->GetURL());
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(1)->GetURL());
  } else {
    // The new browser should have only the NTP.
    ASSERT_EQ(1, tab_strip->count());
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(0)->GetURL());
  }

  // profile_urls opened the urls.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_urls, original_desktop_type));
  new_browser = FindOneOtherBrowserForProfile(profile_urls, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls[0], tab_strip->GetWebContentsAt(0)->GetURL());

  // profile_last opened the last open pages.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_last, original_desktop_type));
  new_browser = FindOneOtherBrowserForProfile(profile_last, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/empty.html", tab_strip->GetWebContentsAt(0)->GetURL().path());

  // profile_home2 was not launched since it would've only opened the home page.
  ASSERT_EQ(0u, chrome::GetBrowserCount(profile_home2, original_desktop_type));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, ProfilesLaunchedAfterCrash) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  // After an unclean exit, all profiles will be launched. However, they won't
  // open any pages automatically.

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create 3 profiles.
  base::FilePath dest_path1 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 2"));
  base::FilePath dest_path3 = profile_manager->user_data_dir().Append(
      FILE_PATH_LITERAL("New Profile 3"));

  Profile* profile_home = profile_manager->GetProfile(dest_path1);
  ASSERT_TRUE(profile_home);
  Profile* profile_last = profile_manager->GetProfile(dest_path2);
  ASSERT_TRUE(profile_last);
  Profile* profile_urls = profile_manager->GetProfile(dest_path3);
  ASSERT_TRUE(profile_urls);

  // Set the profiles to open the home page, last visited pages or URLs.
  SessionStartupPref pref_home(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(profile_home, pref_home);

  SessionStartupPref pref_last(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile_last, pref_last);

  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  SessionStartupPref pref_urls(SessionStartupPref::URLS);
  pref_urls.urls = urls;
  SessionStartupPref::SetStartupPref(profile_urls, pref_urls);

  // Simulate a launch after an unclear exit.
  CloseBrowserAsynchronously(browser());
  static_cast<ProfileImpl*>(profile_home)->last_session_exit_type_ =
      Profile::EXIT_CRASHED;
  static_cast<ProfileImpl*>(profile_last)->last_session_exit_type_ =
      Profile::EXIT_CRASHED;
  static_cast<ProfileImpl*>(profile_urls)->last_session_exit_type_ =
      Profile::EXIT_CRASHED;

#if !defined(OS_MACOSX) && !defined(GOOGLE_CHROME_BUILD)
  // Use HistogramTester to make sure a bubble is shown when it's not on
  // platform Mac OS X and it's not official Chrome build.
  //
  // On Mac OS X, an infobar is shown to restore the previous session, which
  // is tested by function EnsureRestoreUIWasShown.
  //
  // Under a Google Chrome build, it is not tested because a task is posted to
  // the file thread before the bubble is shown. It is difficult to make sure
  // that the histogram check runs after all threads have finished their tasks.
  base::HistogramTester histogram_tester;
#endif  // !defined(OS_MACOSX) && !defined(GOOGLE_CHROME_BUILD)

  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  dummy.AppendSwitchASCII(switches::kTestType, "browser");
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile_home);
  last_opened_profiles.push_back(profile_last);
  last_opened_profiles.push_back(profile_urls);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile_home,
                        last_opened_profiles);

  // No profiles are getting restored, since they all display the crash info
  // bar.
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_home));
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_last));
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_urls));

  // The profile which normally opens the home page displays the new tab page.
  // The welcome page is also shown for relevant platforms.
  Browser* new_browser = NULL;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_home,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(profile_home, NULL);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  if (IsWindows10OrNewer()) {
    // The new browser should have the welcome tab and the NTP.
    ASSERT_EQ(2, tab_strip->count());
    EXPECT_EQ(GURL(internals::GetWelcomePageURL()),
              tab_strip->GetWebContentsAt(0)->GetURL());
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(1)->GetURL());
  } else {
    // The new browser should have only the NTP.
    ASSERT_EQ(1, tab_strip->count());
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(0)->GetURL());
  }
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens last open pages displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_last,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(profile_last, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            tab_strip->GetWebContentsAt(0)->GetURL());
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

  // The profile which normally opens URLs displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_urls,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(profile_urls, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            tab_strip->GetWebContentsAt(0)->GetURL());
  EnsureRestoreUIWasShown(tab_strip->GetWebContentsAt(0));

#if !defined(OS_MACOSX) && !defined(GOOGLE_CHROME_BUILD)
  // Each profile should have one session restore bubble shown, so we should
  // observe count 3 in bucket 0 (which represents bubble shown).
  histogram_tester.ExpectBucketCount("SessionCrashed.Bubble", 0, 3);
#endif  // !defined(OS_MACOSX) && !defined(GOOGLE_CHROME_BUILD)
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, DefaultBrowserCallback) {
  bool callback_called = false;

  // Set the default browser callback.
  StartupBrowserCreator::SetDefaultBrowserCallback(
      base::Bind(&SetTrue, &callback_called));

  // Set the command line to open the default browser url.
  base::CommandLine cmdline(base::CommandLine::NO_PROGRAM);
  cmdline.AppendArgNative(StartupBrowserCreator::GetDefaultBrowserUrl());

  // Open url.
  ProcessCommandLineAlreadyRunningDefaultProfile(cmdline);

  // The url should have been intercepted and the callback invoked.
  GURL default_browser_url =
      GURL(StartupBrowserCreator::GetDefaultBrowserUrl());
  EXPECT_FALSE(
      TabStripContainsUrl(browser()->tab_strip_model(), default_browser_url));
  EXPECT_TRUE(callback_called);

  // Clear default browser callback.
  callback_called = false;
  StartupBrowserCreator::ClearDefaultBrowserCallback();

  // Open url.
  ProcessCommandLineAlreadyRunningDefaultProfile(cmdline);

  // The url should not have been intercepted and the callback not invoked.
  EXPECT_TRUE(
      TabStripContainsUrl(browser()->tab_strip_model(), default_browser_url));
  EXPECT_FALSE(callback_called);
}
#endif  // defined(OS_WIN)

class SupervisedUserBrowserCreatorTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kSupervisedUserId, "asdf");
  }
};

IN_PROC_BROWSER_TEST_F(SupervisedUserBrowserCreatorTest,
                       StartupSupervisedUserProfile) {
  StartupBrowserCreator browser_creator;

  // Do a simple non-process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  // There should be only one tab, except on Windows 10. See crbug.com/505029.
  const int tab_count = IsWindows10OrNewer() ? 2 : 1;
  EXPECT_EQ(tab_count, tab_strip->count());
}

#endif  // !defined(OS_CHROMEOS)

// These tests are not applicable to Chrome OS as neither master_preferences nor
// the sync promo exist there.
#if !defined(OS_CHROMEOS)

// On a branded Linux build, policy is required to suppress the first-run
// dialog.
#if !defined(OS_LINUX) || !defined(GOOGLE_CHROME_BUILD) || \
    defined(ENABLE_CONFIGURATION_POLICY)

class StartupBrowserCreatorFirstRunTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;

  // Returns true if the platform supports showing the sync promo on first run.
  static bool PlatformSupportsSyncPromo() {
    return !IsWindows10OrNewer();
  }

#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::MockConfigurationPolicyProvider provider_;
  policy::PolicyMap policy_map_;
#endif  // defined(ENABLE_CONFIGURATION_POLICY)
};

void StartupBrowserCreatorFirstRunTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kForceFirstRun);
}

void StartupBrowserCreatorFirstRunTest::SetUpInProcessBrowserTestFixture() {
#if defined(ENABLE_CONFIGURATION_POLICY)
#if defined(OS_LINUX) && defined(GOOGLE_CHROME_BUILD)
  // Set a policy that prevents the first-run dialog from being shown.
  policy_map_.Set(policy::key::kMetricsReportingEnabled,
                  policy::POLICY_LEVEL_MANDATORY,
                  policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD,
                  new base::FundamentalValue(false),
                  NULL);
  provider_.UpdateChromePolicy(policy_map_);
#endif  // defined(OS_LINUX) && defined(GOOGLE_CHROME_BUILD)

  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
#endif  // defined(ENABLE_CONFIGURATION_POLICY)
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_SyncPromoForbidden DISABLED_SyncPromoForbidden
#else
#define MAYBE_SyncPromoForbidden SyncPromoForbidden
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_SyncPromoForbidden) {
  // Consistently enable the welcome page on all platforms.
  first_run::SetShouldShowWelcomePage();

  // Simulate the following master_preferences:
  // {
  //  "sync_promo": {
  //    "show_on_first_run_allowed": false
  //  }
  // }
  StartupBrowserCreator browser_creator;
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, false);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the NTP and the welcome page are shown.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  if (IsWindows10OrNewer()) {
    EXPECT_EQ(internals::GetWelcomePageURL(),
              tab_strip->GetWebContentsAt(0)->GetURL());
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(1)->GetURL());
  } else {
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(0)->GetURL());
    EXPECT_EQ(internals::GetWelcomePageURL(),
              tab_strip->GetWebContentsAt(1)->GetURL());
  }
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_SyncPromoAllowed DISABLED_SyncPromoAllowed
#else
#define MAYBE_SyncPromoAllowed SyncPromoAllowed
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_SyncPromoAllowed) {
  if (!PlatformSupportsSyncPromo())
    return;
  // Consistently enable the welcome page on all platforms.
  first_run::SetShouldShowWelcomePage();

  // Simulate the following master_preferences:
  // {
  //  "sync_promo": {
  //    "show_on_first_run_allowed": true
  //  }
  // }
  StartupBrowserCreator browser_creator;
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, true);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the sync promo and the welcome page are shown.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ(GetSigninPromoURL(), tab_strip->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(internals::GetWelcomePageURL(),
            tab_strip->GetWebContentsAt(1)->GetURL());
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_FirstRunTabsPromoAllowed DISABLED_FirstRunTabsPromoAllowed
#else
#define MAYBE_FirstRunTabsPromoAllowed FirstRunTabsPromoAllowed
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_FirstRunTabsPromoAllowed) {
  if (!PlatformSupportsSyncPromo())
    return;
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "/title1.html"
  //  ],
  //  "sync_promo": {
  //    "show_on_first_run_allowed": true
  //  }
  // }
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, true);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the first-run tab is shown and the sync promo has been added.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ(GetSigninPromoURL(), tab_strip->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(1)->GetURL().ExtractFileName());
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_FirstRunTabsContainSyncPromo \
    DISABLED_FirstRunTabsContainSyncPromo
#else
#define MAYBE_FirstRunTabsContainSyncPromo FirstRunTabsContainSyncPromo
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_FirstRunTabsContainSyncPromo) {
  if (!PlatformSupportsSyncPromo())
    return;
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "/title1.html",
  //    "chrome://signin/?source=0&next_page=chrome%3A%2F%2Fnewtab%2F"
  //  ],
  //  "sync_promo": {
  //    "show_on_first_run_allowed": true
  //  }
  // }
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser_creator.AddFirstRunTab(GetSigninPromoURL());
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, true);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the first-run tabs are shown and no sync promo has been added
  // as the first-run tabs contain it already.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
  EXPECT_EQ(GetSigninPromoURL(), tab_strip->GetWebContentsAt(1)->GetURL());
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_FirstRunTabsContainNTPSyncPromoAllowed \
    DISABLED_FirstRunTabsContainNTPSyncPromoAllowed
#else
#define MAYBE_FirstRunTabsContainNTPSyncPromoAllowed \
    FirstRunTabsContainNTPSyncPromoAllowed
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_FirstRunTabsContainNTPSyncPromoAllowed) {
  if (!PlatformSupportsSyncPromo())
    return;
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "new_tab_page",
  //    "/title1.html"
  //  ],
  //  "sync_promo": {
  //    "show_on_first_run_allowed": true
  //  }
  // }
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(GURL("http://new_tab_page"));
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, true);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the first-run tabs are shown but the NTP that they contain has
  // been replaced by the sync promo.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ(GetSigninPromoURL(), tab_strip->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(1)->GetURL().ExtractFileName());
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_FirstRunTabsContainNTPSyncPromoForbidden \
    DISABLED_FirstRunTabsContainNTPSyncPromoForbidden
#else
#define MAYBE_FirstRunTabsContainNTPSyncPromoForbidden \
    FirstRunTabsContainNTPSyncPromoForbidden
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_FirstRunTabsContainNTPSyncPromoForbidden) {
  if (!PlatformSupportsSyncPromo())
    return;
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "new_tab_page",
  //    "/title1.html"
  //  ],
  //  "sync_promo": {
  //    "show_on_first_run_allowed": false
  //  }
  // }
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(GURL("http://new_tab_page"));
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, false);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the first-run tabs are shown, the NTP that they contain has not
  // not been replaced by the sync promo and no sync promo has been added.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(2, tab_strip->count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            tab_strip->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(1)->GetURL().ExtractFileName());
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_FirstRunTabsSyncPromoForbidden \
    DISABLED_FirstRunTabsSyncPromoForbidden
#else
#define MAYBE_FirstRunTabsSyncPromoForbidden FirstRunTabsSyncPromoForbidden
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_FirstRunTabsSyncPromoForbidden) {
  if (!PlatformSupportsSyncPromo())
    return;
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "/title1.html"
  //  ],
  //  "sync_promo": {
  //    "show_on_first_run_allowed": false
  //  }
  // }
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(
      embedded_test_server()->GetURL("/title1.html"));
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, false);

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the first-run tab is shown and no sync promo has been added.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
}

#if defined(ENABLE_CONFIGURATION_POLICY)
#if defined(GOOGLE_CHROME_BUILD) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_RestoreOnStartupURLsPolicySpecified \
    DISABLED_RestoreOnStartupURLsPolicySpecified
#else
#define MAYBE_RestoreOnStartupURLsPolicySpecified \
    RestoreOnStartupURLsPolicySpecified
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_RestoreOnStartupURLsPolicySpecified) {
  if (!PlatformSupportsSyncPromo())
    return;
  // Simulate the following master_preferences:
  // {
  //  "sync_promo": {
  //    "show_on_first_run_allowed": true
  //  }
  // }
  ASSERT_TRUE(embedded_test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, true);

  // Set the following user policies:
  // * RestoreOnStartup = RestoreOnStartupIsURLs
  // * RestoreOnStartupURLs = [ "/title1.html" ]
  policy_map_.Set(
      policy::key::kRestoreOnStartup,
      policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER,
      policy::POLICY_SOURCE_CLOUD,
      new base::FundamentalValue(SessionStartupPref::kPrefValueURLs),
      NULL);
  base::ListValue startup_urls;
  startup_urls.Append(new base::StringValue(
      embedded_test_server()->GetURL("/title1.html").spec()));
  policy_map_.Set(policy::key::kRestoreOnStartupURLs,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  policy::POLICY_SOURCE_CLOUD, startup_urls.DeepCopy(),
                  nullptr);
  provider_.UpdateChromePolicy(policy_map_);
  base::RunLoop().RunUntilIdle();

  // Do a process-startup browser launch.
  base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, &browser_creator,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), true,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Verify that the URL specified through policy is shown and no sync promo has
  // been added.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("title1.html",
            tab_strip->GetWebContentsAt(0)->GetURL().ExtractFileName());
}
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

#endif  // !defined(OS_LINUX) || !defined(GOOGLE_CHROME_BUILD) ||
        // defined(ENABLE_CONFIGURATION_POLICY)

#endif  // !defined(OS_CHROMEOS)
