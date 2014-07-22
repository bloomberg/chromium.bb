// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
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
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
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

using extensions::Extension;

namespace {

// Check that there are two browsers. Find the one that is not |browser|.
Browser* FindOneOtherBrowser(Browser* browser) {
  // There should only be one other browser.
  EXPECT_EQ(2u, chrome::GetBrowserCount(browser->profile(),
                                        browser->host_desktop_type()));

  // Find the new browser.
  Browser* other_browser = NULL;
  for (chrome::BrowserIterator it; !it.done() && !other_browser; it.Next()) {
    if (*it != browser)
      other_browser = *it;
  }
  return other_browser;
}

}  // namespace

class StartupBrowserCreatorTest : public ExtensionBrowserTest {
 protected:
  virtual bool SetUpUserDataDirectory() OVERRIDE {
    return ExtensionBrowserTest::SetUpUserDataDirectory();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
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
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    extensions::SetLaunchType(service, app_id, launch_type);
  }

  Browser* FindOneOtherBrowserForProfile(Profile* profile,
                                         Browser* not_this_browser) {
    for (chrome::BrowserIterator it; !it.done(); it.Next()) {
      if (*it != not_this_browser && it->profile() == profile)
        return *it;
    }
    return NULL;
  }
};

class OpenURLsPopupObserver : public chrome::BrowserListObserver {
 public:
  OpenURLsPopupObserver() : added_browser_(NULL) { }

  virtual void OnBrowserAdded(Browser* browser) OVERRIDE {
    added_browser_ = browser;
  }

  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE { }

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

  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  ASSERT_TRUE(test_server()->Start());
  std::vector<GURL> urls;
  urls.push_back(test_server()->GetURL("files/title1.html"));
  urls.push_back(test_server()->GetURL("files/title2.html"));

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  // Close the browser.
  browser()->window()->Close();

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.  |browser()| is still
  // around at this point, even though we've closed its window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // The new browser should have one tab for each URL.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(static_cast<int>(urls.size()), tab_strip->count());
  for (size_t i=0; i < urls.size(); i++) {
    EXPECT_EQ(urls[i], tab_strip->GetWebContentsAt(i)->GetURL());
  }

  // The two tabs, despite having the same site, should be in different
  // SiteInstances.
  EXPECT_NE(tab_strip->GetWebContentsAt(0)->GetSiteInstance(),
            tab_strip->GetWebContentsAt(1)->GetSiteInstance());
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
  CommandLine dummy(CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // The new browser should have exactly one tab (not the startup URLs).
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());
}

// App shortcuts are not implemented on mac os.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppShortcutNoPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Add --app-id=<extension->id()> to the command line.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  chrome::startup::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  StartupBrowserCreatorImpl launch(base::FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // No pref was set, so the app should have opened in a window.
  // The launch should have created a new browser.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  // Expect an app window.
  EXPECT_TRUE(new_browser->is_app());

  // The browser's app_name should include the app's ID.
  EXPECT_NE(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, OpenAppShortcutWindowPref) {
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), extensions::LAUNCH_TYPE_WINDOW);

  CommandLine command_line(CommandLine::NO_PROGRAM);
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

  CommandLine command_line(CommandLine::NO_PROGRAM);
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
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title1.html"));
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title2.html"));

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title1.html"));
  browser_creator.AddFirstRunTab(GURL("http://new_tab_page"));
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title2.html"));
  browser_creator.AddFirstRunTab(GURL("http://welcome_page"));

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  CommandLine dummy(CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());

  if (signin::ShouldShowPromoAtStartup(browser()->profile(), true)) {
    EXPECT_EQ(signin::GetPromoURL(signin::SOURCE_START_PAGE, false),
              tab_strip->GetWebContentsAt(0)->GetURL());
  } else {
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(0)->GetURL());
  }
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, SyncPromoWithWelcomePage) {
  first_run::SetShouldShowWelcomePage();

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
  StartupBrowserCreatorImpl launch(base::FilePath(), dummy,
                                   chrome::startup::IS_FIRST_RUN);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false,
                            browser()->host_desktop_type()));

  // This should have created a new browser window.
  Browser* new_browser = FindOneOtherBrowser(browser());
  ASSERT_TRUE(new_browser);

  TabStripModel* tab_strip = new_browser->tab_strip_model();
  EXPECT_EQ(2, tab_strip->count());

  if (signin::ShouldShowPromoAtStartup(browser()->profile(), true)) {
    EXPECT_EQ(signin::GetPromoURL(signin::SOURCE_START_PAGE, false),
              tab_strip->GetWebContentsAt(0)->GetURL());
  } else {
    EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
              tab_strip->GetWebContentsAt(0)->GetURL());
  }
  EXPECT_EQ(internals::GetWelcomePageURL(),
            tab_strip->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, SyncPromoWithFirstRunTabs) {
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title1.html"));

  // The welcome page should not be shown, even if
  // first_run::ShouldShowWelcomePage() says so, when there are already
  // more than 2 first run tabs.
  first_run::SetShouldShowWelcomePage();

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
    EXPECT_EQ(signin::GetPromoURL(signin::SOURCE_START_PAGE, false),
              tab_strip->GetWebContentsAt(0)->GetURL());
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
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title1.html"));
  browser_creator.AddFirstRunTab(GURL("http://welcome_page"));

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
    EXPECT_EQ(signin::GetPromoURL(signin::SOURCE_START_PAGE, false),
              tab_strip->GetWebContentsAt(0)->GetURL());
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
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
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
  browser()->window()->Close();

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);

  int return_code;
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(default_profile);
  last_opened_profiles.push_back(other_profile);
  browser_creator.Start(dummy, profile_manager->user_data_dir(),
                        default_profile, last_opened_profiles, &return_code);

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
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(urls1[0], tab_strip->GetWebContentsAt(0)->GetURL());

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

  ASSERT_TRUE(test_server()->Start());

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
                               test_server()->GetURL("files/empty.html"));
  browser1->window()->Close();

  Browser* browser2 = new Browser(
      Browser::CreateParams(Browser::TYPE_TABBED, profile2,
                            browser()->host_desktop_type()));
  chrome::NewTab(browser2);
  ui_test_utils::NavigateToURL(browser2,
                               test_server()->GetURL("files/form.html"));
  browser2->window()->Close();

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
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
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
  CommandLine dummy(CommandLine::NO_PROGRAM);
  int return_code;
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile1);
  last_opened_profiles.push_back(profile2);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile1,
                        last_opened_profiles, &return_code);

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
  EXPECT_EQ("/files/empty.html",
            tab_strip->GetWebContentsAt(0)->GetURL().path());

  ASSERT_EQ(1u, chrome::GetBrowserCount(profile2,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(profile2, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ("/files/form.html",
            tab_strip->GetWebContentsAt(0)->GetURL().path());
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest,
                       ProfilesWithoutPagesNotLaunched) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

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
                               test_server()->GetURL("files/empty.html"));
  browser_last->window()->Close();

  // Close the main browser.
  chrome::HostDesktopType original_desktop_type =
      browser()->host_desktop_type();
  browser()->window()->Close();

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);

  int return_code;
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile_home1);
  last_opened_profiles.push_back(profile_home2);
  last_opened_profiles.push_back(profile_last);
  last_opened_profiles.push_back(profile_urls);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile_home1,
                        last_opened_profiles, &return_code);

  while (SessionRestore::IsRestoring(default_profile) ||
         SessionRestore::IsRestoring(profile_home1) ||
         SessionRestore::IsRestoring(profile_home2) ||
         SessionRestore::IsRestoring(profile_last) ||
         SessionRestore::IsRestoring(profile_urls))
    base::MessageLoop::current()->RunUntilIdle();

  Browser* new_browser = NULL;
  // The last open profile (the profile_home1 in this case) will always be
  // launched, even if it will open just the home page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_home1, original_desktop_type));
  new_browser = FindOneOtherBrowserForProfile(profile_home1, NULL);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            tab_strip->GetWebContentsAt(0)->GetURL());

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
  EXPECT_EQ("/files/empty.html",
            tab_strip->GetWebContentsAt(0)->GetURL().path());

  // profile_home2 was not launched since it would've only opened the home page.
  ASSERT_EQ(0u, chrome::GetBrowserCount(profile_home2, original_desktop_type));
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorTest, ProfilesLaunchedAfterCrash) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
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
  browser()->window()->Close();
  static_cast<ProfileImpl*>(profile_home)->last_session_exit_type_ =
      Profile::EXIT_CRASHED;
  static_cast<ProfileImpl*>(profile_last)->last_session_exit_type_ =
      Profile::EXIT_CRASHED;
  static_cast<ProfileImpl*>(profile_urls)->last_session_exit_type_ =
      Profile::EXIT_CRASHED;

  CommandLine dummy(CommandLine::NO_PROGRAM);
  dummy.AppendSwitchASCII(switches::kTestType, "browser");
  int return_code;
  StartupBrowserCreator browser_creator;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile_home);
  last_opened_profiles.push_back(profile_last);
  last_opened_profiles.push_back(profile_urls);
  browser_creator.Start(dummy, profile_manager->user_data_dir(), profile_home,
                        last_opened_profiles, &return_code);

  // No profiles are getting restored, since they all display the crash info
  // bar.
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_home));
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_last));
  EXPECT_FALSE(SessionRestore::IsRestoring(profile_urls));

  // The profile which normally opens the home page displays the new tab page.
  Browser* new_browser = NULL;
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_home,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(profile_home, NULL);
  ASSERT_TRUE(new_browser);
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  content::WebContents* web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), web_contents->GetURL());
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  EXPECT_EQ(1U, infobar_service->infobar_count());

  // The profile which normally opens last open pages displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_last,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(profile_last, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), web_contents->GetURL());
  infobar_service = InfoBarService::FromWebContents(web_contents);
  EXPECT_EQ(1U, infobar_service->infobar_count());

  // The profile which normally opens URLs displays the new tab page.
  ASSERT_EQ(1u, chrome::GetBrowserCount(profile_urls,
                                        browser()->host_desktop_type()));
  new_browser = FindOneOtherBrowserForProfile(profile_urls, NULL);
  ASSERT_TRUE(new_browser);
  tab_strip = new_browser->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  web_contents = tab_strip->GetWebContentsAt(0);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), web_contents->GetURL());
  infobar_service = InfoBarService::FromWebContents(web_contents);
  EXPECT_EQ(1U, infobar_service->infobar_count());
}

class SupervisedUserBrowserCreatorTest : public InProcessBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kSupervisedUserId, "asdf");
  }
};

IN_PROC_BROWSER_TEST_F(SupervisedUserBrowserCreatorTest,
                       StartupSupervisedUserProfile) {
  StartupBrowserCreator browser_creator;

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  // There should be only one tab.
  EXPECT_EQ(1, tab_strip->count());
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
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::MockConfigurationPolicyProvider provider_;
  policy::PolicyMap policy_map_;
#endif  // defined(ENABLE_CONFIGURATION_POLICY)
};

void StartupBrowserCreatorFirstRunTest::SetUpCommandLine(
    CommandLine* command_line) {
  command_line->AppendSwitch(switches::kForceFirstRun);
}

void StartupBrowserCreatorFirstRunTest::SetUpInProcessBrowserTestFixture() {
#if defined(ENABLE_CONFIGURATION_POLICY)
#if defined(OS_LINUX) && defined(GOOGLE_CHROME_BUILD)
  // Set a policy that prevents the first-run dialog from being shown.
  policy_map_.Set(policy::key::kMetricsReportingEnabled,
                  policy::POLICY_LEVEL_MANDATORY,
                  policy::POLICY_SCOPE_USER,
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
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            tab_strip->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(internals::GetWelcomePageURL(),
            tab_strip->GetWebContentsAt(1)->GetURL());
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_MACOSX)
// http://crbug.com/314819
#define MAYBE_SyncPromoAllowed DISABLED_SyncPromoAllowed
#else
#define MAYBE_SyncPromoAllowed SyncPromoAllowed
#endif
IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorFirstRunTest,
                       MAYBE_SyncPromoAllowed) {
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
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  EXPECT_EQ(signin::GetPromoURL(signin::SOURCE_START_PAGE, false),
            tab_strip->GetWebContentsAt(0)->GetURL());
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
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "files/title1.html"
  //  ],
  //  "sync_promo": {
  //    "show_on_first_run_allowed": true
  //  }
  // }
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title1.html"));
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, true);

  // Do a process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  EXPECT_EQ(signin::GetPromoURL(signin::SOURCE_START_PAGE, false),
            tab_strip->GetWebContentsAt(0)->GetURL());
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
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "files/title1.html",
  //    "chrome://signin/?source=0&next_page=chrome%3A%2F%2Fnewtab%2F"
  //  ],
  //  "sync_promo": {
  //    "show_on_first_run_allowed": true
  //  }
  // }
  ASSERT_TRUE(test_server()->Start());
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title1.html"));
  browser_creator.AddFirstRunTab(signin::GetPromoURL(signin::SOURCE_START_PAGE,
                                                     false));
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, true);

  // Do a process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  EXPECT_EQ(signin::GetPromoURL(signin::SOURCE_START_PAGE, false),
            tab_strip->GetWebContentsAt(1)->GetURL());
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
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "new_tab_page",
  //    "files/title1.html"
  //  ],
  //  "sync_promo": {
  //    "show_on_first_run_allowed": true
  //  }
  // }
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(GURL("http://new_tab_page"));
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title1.html"));
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, true);

  // Do a process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  EXPECT_EQ(signin::GetPromoURL(signin::SOURCE_START_PAGE, false),
            tab_strip->GetWebContentsAt(0)->GetURL());
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
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "new_tab_page",
  //    "files/title1.html"
  //  ],
  //  "sync_promo": {
  //    "show_on_first_run_allowed": false
  //  }
  // }
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(GURL("http://new_tab_page"));
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title1.html"));
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, false);

  // Do a process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  // Simulate the following master_preferences:
  // {
  //  "first_run_tabs" : [
  //    "files/title1.html"
  //  ],
  //  "sync_promo": {
  //    "show_on_first_run_allowed": false
  //  }
  // }
  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTab(test_server()->GetURL("files/title1.html"));
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, false);

  // Do a process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
  // Simulate the following master_preferences:
  // {
  //  "sync_promo": {
  //    "show_on_first_run_allowed": true
  //  }
  // }
  StartupBrowserCreator browser_creator;
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSignInPromoShowOnFirstRunAllowed, true);

  // Set the following user policies:
  // * RestoreOnStartup = RestoreOnStartupIsURLs
  // * RestoreOnStartupURLs = [ "files/title1.html" ]
  policy_map_.Set(
      policy::key::kRestoreOnStartup,
      policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER,
      new base::FundamentalValue(SessionStartupPref::kPrefValueURLs),
      NULL);
  base::ListValue startup_urls;
  startup_urls.Append(
      new base::StringValue(test_server()->GetURL("files/title1.html").spec()));
  policy_map_.Set(policy::key::kRestoreOnStartupURLs,
                  policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                  startup_urls.DeepCopy(), NULL);
  provider_.UpdateChromePolicy(policy_map_);
  base::RunLoop().RunUntilIdle();

  // Do a process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
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
