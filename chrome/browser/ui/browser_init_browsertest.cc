// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserInitTest : public ExtensionBrowserTest {
 protected:
  // Helper functions return void so that we can ASSERT*().
  // Use ASSERT_FALSE(HasFatalFailure()) after calling these functions
  // to stop the test if an assert fails.
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
  ASSERT_EQ(popup->type(), Browser::TYPE_POPUP);
  ASSERT_EQ(popup, observer.added_browser_);

  CommandLine dummy(CommandLine::NO_PROGRAM);
  BrowserInit::LaunchWithProfile launch(FilePath(), dummy);
  // This should create a new window, but re-use the profile from |popup|. If
  // it used a NULL or invalid profile, it would crash.
  launch.OpenURLsInBrowser(popup, false, urls);
  ASSERT_NE(popup, observer.added_browser_);
  BrowserList::RemoveObserver(&observer);
}

// App shortcuts are not implemented on mac os.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserInitTest, OpenAppShortcutNoPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = NULL;
  LoadApp("app_with_tab_container", &extension_app);
  ASSERT_FALSE(HasFatalFailure());  // Check for ASSERT failures in LoadApp().

  // Add --app-id=<extension->id()> to the command line.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  BrowserInit::LaunchWithProfile launch(FilePath(), command_line);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // No pref was set, so the app should have opened in a window.
  // The launch should have created a new browser.
  Browser* new_browser = NULL;
  FindOneOtherBrowser(&new_browser);
  ASSERT_FALSE(HasFatalFailure());

  // Expect an app window.
  EXPECT_EQ(Browser::TYPE_APP, new_browser->type());

  // The browser's app_name should include the app's ID.
  EXPECT_NE(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

IN_PROC_BROWSER_TEST_F(BrowserInitTest, OpenAppShortcutWindowPref) {
  const Extension* extension_app = NULL;
  LoadApp("app_with_tab_container", &extension_app);
  ASSERT_FALSE(HasFatalFailure());  // Check for ASSERT failures in LoadApp().

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), ExtensionPrefs::LAUNCH_WINDOW);

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  BrowserInit::LaunchWithProfile launch(FilePath(), command_line);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // Pref was set to open in a window, so the app should have opened in a
  // window.  The launch should have created a new browser. Find the new
  // browser.
  Browser* new_browser = NULL;
  FindOneOtherBrowser(&new_browser);
  ASSERT_FALSE(HasFatalFailure());

  // Expect an app window.
  EXPECT_EQ(Browser::TYPE_APP, new_browser->type());

  // The browser's app_name should include the app's ID.
  EXPECT_NE(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

IN_PROC_BROWSER_TEST_F(BrowserInitTest, OpenAppShortcutTabPref) {
  // Load an app with launch.container = 'tab'.
  const Extension* extension_app = NULL;
  LoadApp("app_with_tab_container", &extension_app);
  ASSERT_FALSE(HasFatalFailure());  // Check for ASSERT failures in LoadApp().

  // Set a pref indicating that the user wants to open this app in a window.
  SetAppLaunchPref(extension_app->id(), ExtensionPrefs::LAUNCH_REGULAR);

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  BrowserInit::LaunchWithProfile launch(FilePath(), command_line);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // When an app shortcut is open and the pref indicates a tab should
  // open, the tab is open in a new browser window.  Expect a new window.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));

  Browser* new_browser = NULL;
  FindOneOtherBrowser(&new_browser);
  ASSERT_FALSE(HasFatalFailure());

  // The tab should be in a normal window.
  EXPECT_EQ(Browser::TYPE_NORMAL, new_browser->type());

  // The browser's app_name should not include the app's ID: It is in a
  // normal browser.
  EXPECT_EQ(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

IN_PROC_BROWSER_TEST_F(BrowserInitTest, OpenAppShortcutPanel) {
  // Load an app with launch.container = 'panel'.
  const Extension* extension_app = NULL;
  LoadApp("app_with_panel_container", &extension_app);
  ASSERT_FALSE(HasFatalFailure());  // Check for ASSERT failures in LoadApp().

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());
  BrowserInit::LaunchWithProfile launch(FilePath(), command_line);
  ASSERT_TRUE(launch.Launch(browser()->profile(), std::vector<GURL>(), false));

  // The launch should have created a new browser, with a panel type.
  Browser* new_browser = NULL;
  FindOneOtherBrowser(&new_browser);
  ASSERT_FALSE(HasFatalFailure());

  // Expect an app panel.
  EXPECT_EQ(Browser::TYPE_APP_POPUP, new_browser->type());

  // The new browser's app_name should include the app's ID.
  EXPECT_NE(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}

#endif  // !defined(OS_MACOSX)
