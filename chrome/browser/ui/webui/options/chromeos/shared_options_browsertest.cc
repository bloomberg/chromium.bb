// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

const char* kTestUsers[] = { "test-user1@gmail.com", "test-user2@gmail.com" };

}  // namespace

class SharedOptionsTest : public LoginManagerTest {
 public:
  SharedOptionsTest()
    : LoginManagerTest(false),
      device_settings_provider_(NULL) {
    stub_settings_provider_.Set(kDeviceOwner, base::StringValue(kTestUsers[0]));
  }

  virtual ~SharedOptionsTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    LoginManagerTest::SetUpOnMainThread();
    CrosSettings* settings = CrosSettings::Get();
    device_settings_provider_ = settings->GetProvider(kDeviceOwner);
    settings->RemoveSettingsProvider(device_settings_provider_);
    settings->AddSettingsProvider(&stub_settings_provider_);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    CrosSettings* settings = CrosSettings::Get();
    settings->RemoveSettingsProvider(&stub_settings_provider_);
    settings->AddSettingsProvider(device_settings_provider_);
    LoginManagerTest::CleanUpOnMainThread();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kMultiProfiles);
  }

 protected:
  void CheckOptionsUI(const User* user, bool is_primary) {
    Profile* profile = UserManager::Get()->GetProfileByUser(user);
    profile->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                   user->email());

    ui_test_utils::BrowserAddedObserver observer;
    Browser* browser = CreateBrowser(profile);
    observer.WaitForSingleNewBrowser();

    ui_test_utils::NavigateToURL(browser,
                                 GURL("chrome://settings-frame"));
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

    bool banner_visible;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "var e = document.getElementById('secondary-user-banner');"
        "var visible = e.offsetWidth > 0 && e.offsetHeight > 0;"
        "window.domAutomationController.send(visible);",
        &banner_visible));
    EXPECT_EQ(is_primary, !banner_visible);
  }

  StubCrosSettingsProvider stub_settings_provider_;
  CrosSettingsProvider* device_settings_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedOptionsTest);
};

IN_PROC_BROWSER_TEST_F(SharedOptionsTest, PRE_SharedOptions) {
  RegisterUser(kTestUsers[0]);
  RegisterUser(kTestUsers[1]);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(SharedOptionsTest, SharedOptions) {
  LoginUser(kTestUsers[0]);
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(kTestUsers[1]);

  UserManager* manager = UserManager::Get();
  ASSERT_EQ(2u, manager->GetLoggedInUsers().size());

  CheckOptionsUI(manager->FindUser(kTestUsers[0]), true /* is_primary */);
  CheckOptionsUI(manager->FindUser(kTestUsers[1]), false /* is_primary */);
}

}  // namespace chromeos
