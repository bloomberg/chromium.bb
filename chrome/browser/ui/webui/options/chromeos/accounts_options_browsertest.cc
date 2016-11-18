// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

const char* kTestUsers[] = { "test-user1@gmail.com", "test-user2@gmail.com" };

}  // namespace

class AccountsOptionsTest : public LoginManagerTest {
 public:
  AccountsOptionsTest()
      : LoginManagerTest(false),
        stub_settings_provider_(base::MakeUnique<StubCrosSettingsProvider>()),
        stub_settings_provider_ptr_(static_cast<StubCrosSettingsProvider*>(
            stub_settings_provider_.get())) {
    stub_settings_provider_->Set(kDeviceOwner,
                                 base::StringValue(kTestUsers[0]));
    for (size_t i = 0; i < arraysize(kTestUsers); ++i) {
      test_users_.push_back(AccountId::FromUserEmail(kTestUsers[i]));
    }
  }

  ~AccountsOptionsTest() override {}

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    CrosSettings* settings = CrosSettings::Get();
    CrosSettingsProvider* device_settings_provider =
        settings->GetProvider(kDeviceOwner);
    device_settings_provider_ =
        settings->RemoveSettingsProvider(device_settings_provider);
    settings->AddSettingsProvider(std::move(stub_settings_provider_));

    // Notify ChromeUserManager of ownership change.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
        content::Source<AccountsOptionsTest>(this),
        content::NotificationService::NoDetails());
  }

  void TearDownOnMainThread() override {
    CrosSettings* settings = CrosSettings::Get();
    stub_settings_provider_ =
        settings->RemoveSettingsProvider(stub_settings_provider_ptr_);
    settings->AddSettingsProvider(std::move(device_settings_provider_));
    LoginManagerTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
  }

 protected:
  void CheckAccountsUI(const user_manager::User* user, bool is_owner) {
    Profile* profile = ProfileHelper::Get()->GetProfileByUserUnsafe(user);

    ui_test_utils::BrowserAddedObserver observer;
    Browser* browser = CreateBrowser(profile);
    observer.WaitForSingleNewBrowser();

    ui_test_utils::NavigateToURL(browser,
                                 GURL("chrome://settings-frame/accounts"));
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();

    bool warning_visible;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "var e = document.getElementById('ownerOnlyWarning');"
        "var visible = e.offsetWidth > 0 && e.offsetHeight > 0;"
        "window.domAutomationController.send(visible);",
        &warning_visible));
    EXPECT_EQ(is_owner, !warning_visible);

    bool guest_option_enabled;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "var e = document.getElementById('allowBwsiCheck');"
        "window.domAutomationController.send(!e.disabled);",
        &guest_option_enabled));
    EXPECT_EQ(is_owner, guest_option_enabled);

    bool supervised_users_enabled;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "var e = document.getElementById('allowSupervisedCheck');"
        "window.domAutomationController.send(!e.disabled);",
        &supervised_users_enabled));
    ASSERT_EQ(is_owner, supervised_users_enabled);

    bool user_pods_enabled;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "var e = document.getElementById('showUserNamesCheck');"
        "window.domAutomationController.send(!e.disabled);",
        &user_pods_enabled));
    EXPECT_EQ(is_owner, user_pods_enabled);

    bool whitelist_enabled;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        contents,
        "var e = document.getElementById('useWhitelistCheck');"
        "window.domAutomationController.send(!e.disabled);",
        &whitelist_enabled));
    EXPECT_EQ(is_owner, whitelist_enabled);
  }

  std::unique_ptr<CrosSettingsProvider> stub_settings_provider_;
  StubCrosSettingsProvider* stub_settings_provider_ptr_;
  std::unique_ptr<CrosSettingsProvider> device_settings_provider_;
  std::vector<AccountId> test_users_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountsOptionsTest);
};

IN_PROC_BROWSER_TEST_F(AccountsOptionsTest, PRE_MultiProfilesAccountsOptions) {
  RegisterUser(test_users_[0].GetUserEmail());
  RegisterUser(test_users_[1].GetUserEmail());
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(AccountsOptionsTest, MultiProfilesAccountsOptions) {
  LoginUser(test_users_[0].GetUserEmail());
  UserAddingScreen::Get()->Start();
  content::RunAllPendingInMessageLoop();
  AddUser(test_users_[1].GetUserEmail());

  user_manager::UserManager* manager = user_manager::UserManager::Get();
  ASSERT_EQ(2u, manager->GetLoggedInUsers().size());

  CheckAccountsUI(manager->FindUser(test_users_[0]), true /* is_owner */);
  CheckAccountsUI(manager->FindUser(test_users_[1]), false /* is_owner */);
}

}  // namespace chromeos
