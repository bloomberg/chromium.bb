// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/login_screen_tester.h"
#include "chrome/browser/chromeos/login/test/oobe_auth_page_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/sync_consent_screen_handler.h"
#include "net/dns/mock_host_resolver.h"

namespace chromeos {
namespace {

constexpr char kExistingUserEmail[] = "existing@gmail.com";
constexpr char kExistingUserGaiaId[] = "9876543210";

constexpr char kNewUserEmail[] = "new@gmail.com";
constexpr char kNewUserGaiaId[] = "0123456789";

class LoginUIShelfVisibilityTest : public MixinBasedInProcessBrowserTest {
 public:
  LoginUIShelfVisibilityTest() = default;
  ~LoginUIShelfVisibilityTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(kExistingUserEmail, kExistingUserGaiaId)};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {test_user_}};
  EmbeddedTestServerSetupMixin test_server_mixin_{&mixin_host_,
                                                  embedded_test_server()};
  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_, embedded_test_server()};

  DISALLOW_COPY_AND_ASSIGN(LoginUIShelfVisibilityTest);
};

}  // namespace

// Verifies that shelf buttons are shown by default on login screen.
IN_PROC_BROWSER_TEST_F(LoginUIShelfVisibilityTest, DefaultVisibility) {
  EXPECT_TRUE(test::LoginScreenTester().IsGuestButtonShown());
  EXPECT_TRUE(test::LoginScreenTester().IsAddUserButtonShown());
}

// Verifies that guest button and add user button are hidden when Gaia
// dialog is shown.
IN_PROC_BROWSER_TEST_F(LoginUIShelfVisibilityTest, GaiaDialogOpen) {
  EXPECT_TRUE(test::LoginScreenTester().ClickAddUserButton());
  test::OobeGaiaPageWaiter().WaitUntilReady();
  EXPECT_FALSE(test::LoginScreenTester().IsGuestButtonShown());
  EXPECT_FALSE(test::LoginScreenTester().IsAddUserButtonShown());
}

// Verifies that guest button and add user button are hidden on post-login
// screens, after a user session is started.
IN_PROC_BROWSER_TEST_F(LoginUIShelfVisibilityTest, PostLoginScreen) {
  auto override = WizardController::ForceOfficialBuildForTesting();
  EXPECT_TRUE(test::LoginScreenTester().ClickAddUserButton());
  test::OobeGaiaPageWaiter().WaitUntilReady();
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(kNewUserEmail, kNewUserGaiaId,
                                FakeGaiaMixin::kEmptyUserServices);

  // Sync consent is the first post-login screen shown when a new user signs in.
  OobeScreenWaiter(SyncConsentScreenView::kScreenId).Wait();

  EXPECT_FALSE(test::LoginScreenTester().IsGuestButtonShown());
  EXPECT_FALSE(test::LoginScreenTester().IsAddUserButtonShown());
}

}  // namespace chromeos
