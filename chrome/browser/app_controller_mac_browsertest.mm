// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "apps/app_window_registry.h"
#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/profiles/user_manager_mac.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"

namespace {

class AppControllerPlatformAppBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  AppControllerPlatformAppBrowserTest()
      : active_browser_list_(BrowserList::GetInstance(
                                chrome::GetActiveDesktop())) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppId,
                                    "1234");
  }

  const BrowserList* active_browser_list_;
};

// Test that if only a platform app window is open and no browser windows are
// open then a reopen event does nothing.
IN_PROC_BROWSER_TEST_F(AppControllerPlatformAppBrowserTest,
                       PlatformAppReopenWithWindows) {
  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  NSUInteger old_window_count = [[NSApp windows] count];
  EXPECT_EQ(1u, active_browser_list_->size());
  [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:YES];
  // We do not EXPECT_TRUE the result here because the method
  // deminiaturizes windows manually rather than return YES and have
  // AppKit do it.

  EXPECT_EQ(old_window_count, [[NSApp windows] count]);
  EXPECT_EQ(1u, active_browser_list_->size());
}

IN_PROC_BROWSER_TEST_F(AppControllerPlatformAppBrowserTest,
                       ActivationFocusesBrowserWindow) {
  base::scoped_nsobject<AppController> app_controller(
      [[AppController alloc] init]);

  ExtensionTestMessageListener listener("Launched", false);
  const extensions::Extension* app =
      InstallAndLaunchPlatformApp("minimal");
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  NSWindow* app_window = apps::AppWindowRegistry::Get(profile())
                             ->GetAppWindowsForApp(app->id())
                             .front()
                             ->GetNativeWindow();
  NSWindow* browser_window = browser()->window()->GetNativeWindow();

  EXPECT_LE([[NSApp orderedWindows] indexOfObject:app_window],
            [[NSApp orderedWindows] indexOfObject:browser_window]);
  [app_controller applicationShouldHandleReopen:NSApp
                              hasVisibleWindows:YES];
  EXPECT_LE([[NSApp orderedWindows] indexOfObject:browser_window],
            [[NSApp orderedWindows] indexOfObject:app_window]);
}

class AppControllerWebAppBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerWebAppBrowserTest()
      : active_browser_list_(BrowserList::GetInstance(
                                chrome::GetActiveDesktop())) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kApp, GetAppURL());
  }

  std::string GetAppURL() const {
    return "http://example.com/";
  }

  const BrowserList* active_browser_list_;
};

// Test that in web app mode a reopen event opens the app URL.
IN_PROC_BROWSER_TEST_F(AppControllerWebAppBrowserTest,
                       WebAppReopenWithNoWindows) {
  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  EXPECT_EQ(1u, active_browser_list_->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];

  EXPECT_FALSE(result);
  EXPECT_EQ(2u, active_browser_list_->size());

  Browser* browser = active_browser_list_->get(0);
  GURL current_url =
      browser->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_EQ(GetAppURL(), current_url.spec());
}

// Called when the ProfileManager has created a profile.
void CreateProfileCallback(const base::Closure& quit_closure,
                           Profile* profile,
                           Profile::CreateStatus status) {
  EXPECT_TRUE(profile);
  EXPECT_NE(Profile::CREATE_STATUS_LOCAL_FAIL, status);
  EXPECT_NE(Profile::CREATE_STATUS_REMOTE_FAIL, status);
  // This will be called multiple times. Wait until the profile is initialized
  // fully to quit the loop.
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    quit_closure.Run();
}

void CreateAndWaitForGuestProfile() {
  ProfileManager::CreateCallback create_callback =
      base::Bind(&CreateProfileCallback,
                 base::MessageLoop::current()->QuitClosure());
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileManager::GetGuestProfilePath(),
      create_callback,
      base::string16(),
      base::string16(),
      std::string());
  base::RunLoop().Run();
}

class AppControllerNewProfileManagementBrowserTest
    : public InProcessBrowserTest {
 protected:
  AppControllerNewProfileManagementBrowserTest()
      : active_browser_list_(BrowserList::GetInstance(
                                chrome::GetActiveDesktop())) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kNewProfileManagement);
  }

  const BrowserList* active_browser_list_;
};

// Test that for a regular last profile, a reopen event opens a browser.
IN_PROC_BROWSER_TEST_F(AppControllerNewProfileManagementBrowserTest,
                       RegularProfileReopenWithNoWindows) {
  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  EXPECT_EQ(1u, active_browser_list_->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];

  EXPECT_FALSE(result);
  EXPECT_EQ(2u, active_browser_list_->size());
  EXPECT_FALSE(UserManagerMac::IsShowing());
}

// Test that for a locked last profile, a reopen event opens the User Manager.
IN_PROC_BROWSER_TEST_F(AppControllerNewProfileManagementBrowserTest,
                       LockedProfileReopenWithNoWindows) {
  // The User Manager uses the guest profile as its underlying profile. To
  // minimize flakiness due to the scheduling/descheduling of tasks on the
  // different threads, pre-initialize the guest profile before it is needed.
  CreateAndWaitForGuestProfile();
  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);

  // Lock the active profile.
  Profile* profile = [ac lastProfile];
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  cache.SetProfileSigninRequiredAtIndex(profile_index, true);
  EXPECT_TRUE(cache.ProfileIsSigninRequiredAtIndex(profile_index));

  EXPECT_EQ(1u, active_browser_list_->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];
  EXPECT_FALSE(result);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, active_browser_list_->size());
  EXPECT_TRUE(UserManagerMac::IsShowing());
  UserManagerMac::Hide();
}

// Test that for a guest last profile, a reopen event opens the User Manager.
IN_PROC_BROWSER_TEST_F(AppControllerNewProfileManagementBrowserTest,
                       GuestProfileReopenWithNoWindows) {
  // Create the guest profile, and set it as the last used profile so the
  // app controller can use it on init.
  CreateAndWaitForGuestProfile();
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, chrome::kGuestProfileDir);

  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);

  Profile* profile = [ac lastProfile];
  EXPECT_EQ(ProfileManager::GetGuestProfilePath(), profile->GetPath());
  EXPECT_TRUE(profile->IsGuestSession());

  EXPECT_EQ(1u, active_browser_list_->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];
  EXPECT_FALSE(result);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, active_browser_list_->size());
  EXPECT_TRUE(UserManagerMac::IsShowing());
  UserManagerMac::Hide();
}

}  // namespace
