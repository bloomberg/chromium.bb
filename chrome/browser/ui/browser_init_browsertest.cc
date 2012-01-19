// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserInitTest : public ExtensionBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }

  // Helper functions return void so that we can ASSERT*().
  // Use ASSERT_NO_FATAL_FAILURE around calls to these functions to stop the
  // test if an assert fails.
  void LoadApp(const std::string& app_name,
               const Extension** out_app_extension) {
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(app_name.c_str())));

    ExtensionService* service = browser()->profile()->GetExtensionService();
    *out_app_extension = service->GetExtensionById(
        last_loaded_extension_id_, false);
    ASSERT_TRUE(*out_app_extension);

    // Code that opens a new browser assumes we start with exactly one.
    ASSERT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));
  }

  void SetAppLaunchPref(const std::string& app_id,
                        ExtensionPrefs::LaunchType launch_type) {
    ExtensionService* service = browser()->profile()->GetExtensionService();
    service->extension_prefs()->SetLaunchType(app_id, launch_type);
  }

  // Check that there are two browsers.  Find the one that is not |browser()|.
  void FindOneOtherBrowser(Browser** out_other_browser) {
    // There should only be one other browser.
    ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));

    // Find the new browser.
    Browser* other_browser = NULL;
    for (BrowserList::const_iterator i = BrowserList::begin();
         i != BrowserList::end() && !other_browser; ++i) {
      if (*i != browser())
        other_browser = *i;
    }
    ASSERT_TRUE(other_browser);
    ASSERT_TRUE(other_browser != browser());
    *out_other_browser = other_browser;
  }

  Browser* FindOneOtherBrowserForProfile(Profile* profile,
                                         Browser* not_this_browser) {
    for (BrowserList::const_iterator i = BrowserList::begin();
         i != BrowserList::end(); ++i) {
      if (*i != not_this_browser && (*i)->profile() == profile)
        return *i;
    }
    return NULL;
  }
};

class OpenURLsPopupObserver : public BrowserList::Observer {
 public:
  OpenURLsPopupObserver() : added_browser_(NULL) { }

  virtual void OnBrowserAdded(const Browser* browser) {
    added_browser_ = browser;
  }

  virtual void OnBrowserRemoved(const Browser* browser) { }

  const Browser* added_browser_;
};

// Test that when there is a popup as the active browser any requests to
// BrowserInit::LaunchWithProfile::OpenURLsInBrowser don't crash because
// there's no explicit profile given.
IN_PROC_BROWSER_TEST_F(BrowserInitTest, OpenURLsPopup) {
  std::vector<GURL> urls;
  urls.push_back(GURL("http://localhost"));

  // Note that in our testing we do not ever query the BrowserList for the "last
  // active" browser. That's because the browsers are set as "active" by
  // platform UI toolkit messages, and those messages are not sent during unit
  // testing sessions.

  OpenURLsPopupObserver observer;
  BrowserList::AddObserver(&observer);

  Browser* popup = Browser::CreateForType(Browser::TYPE_POPUP,
                                          browser()->profile());
  ASSERT_TRUE(popup->is_type_popup());
  ASSERT_EQ(popup, observer.added_browser_);

  CommandLine dummy(CommandLine::NO_PROGRAM);
  BrowserInit::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN : BrowserInit::IS_NOT_FIRST_RUN;
  BrowserInit::LaunchWithProfile launch(FilePath(), dummy, first_run);
  // This should create a new window, but re-use the profile from |popup|. If
  // it used a NULL or invalid profile, it would crash.
  launch.OpenURLsInBrowser(popup, false, urls);
  ASSERT_NE(popup, observer.added_browser_);
  BrowserList::RemoveObserver(&observer);
}

// Verify that startup URLs are honored when the process already exists but has
// no tabbed browser windows (eg. as if the process is running only due to a
// background application.
IN_PROC_BROWSER_TEST_F(BrowserInitTest,
                       StartupURLsOnNewWindowWithNoTabbedBrowsers) {
  // Use a couple arbitrary URLs.
  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));
  urls.push_back(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  // Close the browser.
  browser()->window()->Close();

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
  BrowserInit::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN : BrowserInit::IS_NOT_FIRST_RUN;
  BrowserInit::LaunchWithProfile launch(FilePath(), dummy, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // This should have created a new browser window.  |browser()| is still
  // around at this point, even though we've closed it's window.
  Browser* new_browser = NULL;
  ASSERT_NO_FATAL_FAILURE(FindOneOtherBrowser(&new_browser));

  // The new browser should have one tab for each URL.
  ASSERT_EQ(static_cast<int>(urls.size()), new_browser->tab_count());
  for (size_t i=0; i < urls.size(); i++) {
    EXPECT_EQ(urls[i], new_browser->GetWebContentsAt(i)->GetURL());
  }
}

// Verify that startup URLs aren't used when the process already exists
// and has other tabbed browser windows.  This is the common case of starting a
// new browser.
IN_PROC_BROWSER_TEST_F(BrowserInitTest,
                       StartupURLsOnNewWindow) {
  // Use a couple arbitrary URLs.
  std::vector<GURL> urls;
  urls.push_back(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));
  urls.push_back(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set the startup preference to open these URLs.
  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls = urls;
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  // Do a simple non-process-startup browser launch.
  CommandLine dummy(CommandLine::NO_PROGRAM);
  BrowserInit::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN : BrowserInit::IS_NOT_FIRST_RUN;
  BrowserInit::LaunchWithProfile launch(FilePath(), dummy, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // This should have created a new browser window.
  Browser* new_browser = NULL;
  ASSERT_NO_FATAL_FAILURE(FindOneOtherBrowser(&new_browser));

  // The new browser should have exactly one tab (not the startup URLs).
  ASSERT_EQ(1, new_browser->tab_count());
}

// App shortcuts are not implemented on mac os.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserInitTest, OpenAppShortcutNoPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Add --app-id=<extension->id()> to the command line.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  BrowserInit::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN : BrowserInit::IS_NOT_FIRST_RUN;
  BrowserInit::LaunchWithProfile launch(FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // No pref was set, so the app should have opened in a window.
  // The launch should have created a new browser.
  Browser* new_browser = NULL;
  ASSERT_NO_FATAL_FAILURE(FindOneOtherBrowser(&new_browser));

  // Expect an app window.
  EXPECT_TRUE(new_browser->is_app());

  // The browser's app_name should include the app's ID.
  EXPECT_NE(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

IN_PROC_BROWSER_TEST_F(BrowserInitTest, OpenAppShortcutWindowPref) {
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), ExtensionPrefs::LAUNCH_WINDOW);

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  BrowserInit::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN : BrowserInit::IS_NOT_FIRST_RUN;
  BrowserInit::LaunchWithProfile launch(FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // Pref was set to open in a window, so the app should have opened in a
  // window.  The launch should have created a new browser. Find the new
  // browser.
  Browser* new_browser = NULL;
  ASSERT_NO_FATAL_FAILURE(FindOneOtherBrowser(&new_browser));

  // Expect an app window.
  EXPECT_TRUE(new_browser->is_app());

  // The browser's app_name should include the app's ID.
  EXPECT_NE(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

IN_PROC_BROWSER_TEST_F(BrowserInitTest, OpenAppShortcutTabPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_tab_container", &extension_app));

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), ExtensionPrefs::LAUNCH_REGULAR);

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  BrowserInit::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN : BrowserInit::IS_NOT_FIRST_RUN;
  BrowserInit::LaunchWithProfile launch(FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // When an app shortcut is open and the pref indicates a tab should
  // open, the tab is open in a new browser window.  Expect a new window.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));

  Browser* new_browser = NULL;
  ASSERT_NO_FATAL_FAILURE(FindOneOtherBrowser(&new_browser));

  // The tab should be in a tabbed window.
  EXPECT_TRUE(new_browser->is_type_tabbed());

  // The browser's app_name should not include the app's ID: It is in a
  // normal browser.
  EXPECT_EQ(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

IN_PROC_BROWSER_TEST_F(BrowserInitTest, OpenAppShortcutPanel) {
  // Load an app with launch.container = 'panel'.
  const Extension* extension_app = NULL;
  ASSERT_NO_FATAL_FAILURE(LoadApp("app_with_panel_container", &extension_app));

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  BrowserInit::IsFirstRun first_run = first_run::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN : BrowserInit::IS_NOT_FIRST_RUN;
  BrowserInit::LaunchWithProfile launch(FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // The launch should have created a new browser, with a panel type.
  Browser* new_browser = NULL;
  ASSERT_NO_FATAL_FAILURE(FindOneOtherBrowser(&new_browser));

  // Expect an app panel.
  EXPECT_TRUE(new_browser->is_type_panel() && new_browser->is_app());

  // The new browser's app_name should include the app's ID.
  EXPECT_NE(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

#endif  // !defined(OS_MACOSX)

IN_PROC_BROWSER_TEST_F(BrowserInitTest, ReadingWasRestartedAfterRestart) {
  // Tests that BrowserInit::WasRestarted reads and resets the preference
  // kWasRestarted correctly.
  BrowserInit::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);
  EXPECT_TRUE(BrowserInit::WasRestarted());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_TRUE(BrowserInit::WasRestarted());
}

IN_PROC_BROWSER_TEST_F(BrowserInitTest, ReadingWasRestartedAfterNormalStart) {
  // Tests that BrowserInit::WasRestarted reads and resets the preference
  // kWasRestarted correctly.
  BrowserInit::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, false);
  EXPECT_FALSE(BrowserInit::WasRestarted());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kWasRestarted));
  EXPECT_FALSE(BrowserInit::WasRestarted());
}

IN_PROC_BROWSER_TEST_F(BrowserInitTest, StartupURLsForTwoProfiles) {
  Profile* default_profile = browser()->profile();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create another profile.
  FilePath dest_path = profile_manager->user_data_dir();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile 1"));

  Profile* other_profile = profile_manager->GetProfile(dest_path);
  ASSERT_TRUE(other_profile);

  // Use a couple arbitrary URLs.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title2.html"))));

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
  BrowserInit browser_init;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(default_profile);
  last_opened_profiles.push_back(other_profile);
  browser_init.Start(dummy, profile_manager->user_data_dir(), default_profile,
                     last_opened_profiles, &return_code);

  // urls1 were opened in a browser for default_profile, and urls2 were opened
  // in a browser for other_profile.
  Browser* new_browser = NULL;
  // |browser()| is still around at this point, even though we've closed it's
  // window. Thus the browser count for default_profile is 2.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(default_profile));
  new_browser = FindOneOtherBrowserForProfile(default_profile, browser());
  ASSERT_TRUE(new_browser);
  ASSERT_EQ(1, new_browser->tab_count());
  EXPECT_EQ(urls1[0], new_browser->GetWebContentsAt(0)->GetURL());

  ASSERT_EQ(1u, BrowserList::GetBrowserCount(other_profile));
  new_browser = FindOneOtherBrowserForProfile(other_profile, NULL);
  ASSERT_TRUE(new_browser);
  ASSERT_EQ(1, new_browser->tab_count());
  EXPECT_EQ(urls2[0], new_browser->GetWebContentsAt(0)->GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowserInitTest, UpdateWithTwoProfiles) {
  // Make BrowserInit::WasRestarted() return true.
  BrowserInit::was_restarted_read_ = false;
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kWasRestarted, true);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Create two profiles.
  FilePath dest_path = profile_manager->user_data_dir();

  Profile* profile1 = profile_manager->GetProfile(
      dest_path.Append(FILE_PATH_LITERAL("New Profile 1")));
  ASSERT_TRUE(profile1);

  Profile* profile2 = profile_manager->GetProfile(
      dest_path.Append(FILE_PATH_LITERAL("New Profile 2")));
  ASSERT_TRUE(profile2);

  // Use a couple arbitrary URLs.
  std::vector<GURL> urls1;
  urls1.push_back(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));
  std::vector<GURL> urls2;
  urls2.push_back(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title2.html"))));

  // Set different startup preferences for the 2 profiles.
  SessionStartupPref pref1(SessionStartupPref::URLS);
  pref1.urls = urls1;
  SessionStartupPref::SetStartupPref(profile1, pref1);
  SessionStartupPref pref2(SessionStartupPref::URLS);
  pref2.urls = urls2;
  SessionStartupPref::SetStartupPref(profile2, pref2);

  // Simulate a launch after a browser update.
  CommandLine dummy(CommandLine::NO_PROGRAM);
  int return_code;
  BrowserInit browser_init;
  std::vector<Profile*> last_opened_profiles;
  last_opened_profiles.push_back(profile1);
  last_opened_profiles.push_back(profile2);
  browser_init.Start(dummy, profile_manager->user_data_dir(), profile1,
                     last_opened_profiles, &return_code);

  while (SessionRestore::IsRestoring())
    MessageLoop::current()->RunAllPending();

  // The startup URLs are ignored, and instead the last open sessions are
  // restored.
  EXPECT_TRUE(profile1->restored_last_session());
  EXPECT_TRUE(profile2->restored_last_session());

  Browser* new_browser = NULL;
  ASSERT_EQ(1u, BrowserList::GetBrowserCount(profile1));
  new_browser = FindOneOtherBrowserForProfile(profile1, NULL);
  ASSERT_TRUE(new_browser);
  ASSERT_EQ(1, new_browser->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            new_browser->GetWebContentsAt(0)->GetURL());

  ASSERT_EQ(1u, BrowserList::GetBrowserCount(profile2));
  new_browser = FindOneOtherBrowserForProfile(profile2, NULL);
  ASSERT_TRUE(new_browser);
  ASSERT_EQ(1, new_browser->tab_count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            new_browser->GetWebContentsAt(0)->GetURL());
}
