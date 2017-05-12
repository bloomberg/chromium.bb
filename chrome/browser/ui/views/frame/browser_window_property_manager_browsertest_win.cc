// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>
#include <shlobj.h>  // Must be before propkey.
#include <propkey.h>
#include <shellapi.h>
#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager_win.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/views/win/hwnd_util.h"

typedef ExtensionBrowserTest BrowserWindowPropertyManagerTest;

namespace {

// An observer that resumes test code after a new profile is initialized by
// quitting the message loop it's blocked on.
void UnblockOnProfileCreation(Profile* profile,
                              Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    base::MessageLoop::current()->QuitWhenIdle();
}

// Checks that the relaunch name, relaunch command and app icon for the given
// |browser| are correct.
void ValidateBrowserWindowProperties(
    const Browser* browser,
    const base::string16& expected_profile_name) {
  HWND hwnd = views::HWNDForNativeWindow(browser->window()->GetNativeWindow());

  base::win::ScopedComPtr<IPropertyStore> pps;
  HRESULT result = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps));
  EXPECT_TRUE(SUCCEEDED(result));

  base::win::ScopedPropVariant prop_var;
  // The relaunch name should be of the form "Chromium" if there is only 1
  // profile and "First User - Chromium" if there are more. The expected value
  // is given by |expected_profile_name|.
  EXPECT_EQ(S_OK, pps->GetValue(PKEY_AppUserModel_RelaunchDisplayNameResource,
                                prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  EXPECT_EQ(
      base::FilePath(profiles::internal::GetShortcutFilenameForProfile(
          expected_profile_name,
          BrowserDistribution::GetDistribution())).RemoveExtension().value(),
      prop_var.get().pwszVal);
  prop_var.Reset();

  // The relaunch command should specify the profile.
  EXPECT_EQ(S_OK, pps->GetValue(PKEY_AppUserModel_RelaunchCommand,
                                prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  base::CommandLine cmd_line(
      base::CommandLine::FromString(prop_var.get().pwszVal));
  EXPECT_EQ(browser->profile()->GetPath().BaseName().value(),
            cmd_line.GetSwitchValueNative(switches::kProfileDirectory));
  prop_var.Reset();

  // The app icon should be set to the profile icon.
  EXPECT_EQ(S_OK, pps->GetValue(PKEY_AppUserModel_RelaunchIconResource,
                                prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  EXPECT_EQ(profiles::internal::GetProfileIconPath(
                browser->profile()->GetPath()).value(),
            prop_var.get().pwszVal);
  prop_var.Reset();
  base::MessageLoop::current()->QuitWhenIdle();
}

void ValidateHostedAppWindowProperties(const Browser* browser,
                                       const extensions::Extension* extension) {
  HWND hwnd = views::HWNDForNativeWindow(browser->window()->GetNativeWindow());

  base::win::ScopedComPtr<IPropertyStore> pps;
  HRESULT result = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps));
  EXPECT_TRUE(SUCCEEDED(result));

  base::win::ScopedPropVariant prop_var;
  // The relaunch name should be the extension name.
  EXPECT_EQ(S_OK,
            pps->GetValue(PKEY_AppUserModel_RelaunchDisplayNameResource,
                          prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  EXPECT_EQ(base::UTF8ToWide(extension->name()), prop_var.get().pwszVal);
  prop_var.Reset();

  // The relaunch command should specify the profile and the app id.
  EXPECT_EQ(
      S_OK,
      pps->GetValue(PKEY_AppUserModel_RelaunchCommand, prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  base::CommandLine cmd_line(
      base::CommandLine::FromString(prop_var.get().pwszVal));
  EXPECT_EQ(browser->profile()->GetPath().BaseName().value(),
            cmd_line.GetSwitchValueNative(switches::kProfileDirectory));
  EXPECT_EQ(base::UTF8ToWide(extension->id()),
            cmd_line.GetSwitchValueNative(switches::kAppId));
  prop_var.Reset();

  // The app icon should be set to the extension app icon.
  base::FilePath web_app_dir = web_app::GetWebAppDataDirectory(
      browser->profile()->GetPath(), extension->id(), GURL());
  EXPECT_EQ(S_OK,
            pps->GetValue(PKEY_AppUserModel_RelaunchIconResource,
                          prop_var.Receive()));
  EXPECT_EQ(VT_LPWSTR, prop_var.get().vt);
  EXPECT_EQ(web_app::internals::GetIconFilePath(
                web_app_dir, base::UTF8ToUTF16(extension->name())).value(),
            prop_var.get().pwszVal);
  prop_var.Reset();
  base::MessageLoop::current()->QuitWhenIdle();
}

void PostValidationTaskToUIThread(const base::Closure& validation_task) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, validation_task);
}

// Posts a validation task to the FILE thread which bounces back to the UI
// thread and then does validation. This is necessary because the icon profile
// pref only gets set at the end of icon creation (which happens on the FILE
// thread) and is set on the UI thread.
void WaitAndValidateBrowserWindowProperties(
    const base::Closure& validation_task) {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&PostValidationTaskToUIThread, validation_task));
  content::RunMessageLoop();
}

}  // namespace

// Tests that require the profile shortcut manager to be instantiated despite
// having --user-data-dir specified.
class BrowserTestWithProfileShortcutManager : public InProcessBrowserTest {
 public:
  BrowserTestWithProfileShortcutManager() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableProfileShortcutManager);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserTestWithProfileShortcutManager);
};

// Check that the window properties on Windows are properly set.
// http://crbug.com/396344
IN_PROC_BROWSER_TEST_F(BrowserTestWithProfileShortcutManager,
                       DISABLED_WindowProperties) {
  // This test checks HWND properties that are only available on Win7+.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  // Single profile case. The profile name should not be shown.
  WaitAndValidateBrowserWindowProperties(base::Bind(
      &ValidateBrowserWindowProperties, browser(), base::string16()));

  // If multiprofile mode is not enabled, we can't test the behavior when there
  // are multiple profiles.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // Two profile case. Both profile names should be shown.
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath path_profile2 =
      profile_manager->GenerateNextProfileDirectoryPath();
  profile_manager->CreateProfileAsync(path_profile2,
                                      base::Bind(&UnblockOnProfileCreation),
                                      base::string16(), std::string(),
                                      std::string());

  // Spin to allow profile creation to take place, loop is terminated
  // by UnblockOnProfileCreation when the profile is created.
  content::RunMessageLoop();

  // The default profile's name should be part of the relaunch name.
  WaitAndValidateBrowserWindowProperties(base::Bind(
      &ValidateBrowserWindowProperties,
      browser(),
      base::UTF8ToUTF16(browser()->profile()->GetProfileUserName())));

  // The second profile's name should be part of the relaunch name.
  Browser* profile2_browser =
      CreateBrowser(profile_manager->GetProfileByPath(path_profile2));
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_manager->GetProfileAttributesStorage().
              GetProfileAttributesWithPath(path_profile2, &entry));
  WaitAndValidateBrowserWindowProperties(
      base::Bind(&ValidateBrowserWindowProperties,
                 profile2_browser,
                 entry->GetName()));
}

// http://crbug.com/396344
IN_PROC_BROWSER_TEST_F(BrowserWindowPropertyManagerTest, DISABLED_HostedApp) {
  // This test checks HWND properties that are only available on Win7+.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  // Load an app.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app/"));
  EXPECT_TRUE(extension);

  OpenApplication(AppLaunchParams(
      browser()->profile(), extension, extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::NEW_FOREGROUND_TAB, extensions::SOURCE_TEST));

  // Check that the new browser has an app name.
  // The launch should have created a new browser.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  Browser* app_browser = nullptr;
  for (auto* b : *BrowserList::GetInstance()) {
    if (b != browser())
      app_browser = b;
  }
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(app_browser != browser());

  WaitAndValidateBrowserWindowProperties(
      base::Bind(&ValidateHostedAppWindowProperties, app_browser, extension));
}
