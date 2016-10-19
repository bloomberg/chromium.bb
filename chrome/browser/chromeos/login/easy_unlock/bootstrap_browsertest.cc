// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/easy_unlock/bootstrap_user_context_initializer.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/signin/easy_unlock_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_switches.h"
#include "components/proximity_auth/switches.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace chromeos {

namespace {

const char kFakeGaiaId[] = "123456";
const char kFakeUser[] = "test_user@consumer.example.com";
const char kFakeSid[] = "fake-sid";
const char kFakeLsid[] = "fake-lsid";
const char kFakeRefreshToken[] = "fake-refresh-token";
const char kFakeUserInfoToken[] = "fake-user-info-token";

class ScopedCompleteCallbackForTesting {
 public:
  explicit ScopedCompleteCallbackForTesting(
      const BootstrapUserContextInitializer::CompleteCallback& callback);
  ~ScopedCompleteCallbackForTesting();

 private:
  BootstrapUserContextInitializer::CompleteCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(ScopedCompleteCallbackForTesting);
};

ScopedCompleteCallbackForTesting::ScopedCompleteCallbackForTesting(
    const BootstrapUserContextInitializer::CompleteCallback& callback)
    : callback_(callback) {
  BootstrapUserContextInitializer::SetCompleteCallbackForTesting(&callback_);
}

ScopedCompleteCallbackForTesting::~ScopedCompleteCallbackForTesting() {
  BootstrapUserContextInitializer::SetCompleteCallbackForTesting(NULL);
}

}  // namespace

class BootstrapTest : public OobeBaseTest {
 public:
  BootstrapTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        proximity_auth::switches::kForceLoadEasyUnlockAppInTests);

    const GURL& server_url = embedded_test_server()->base_url();
    GURL::Replacements replace_host;
    replace_host.SetHostStr("eafe");
    GURL eafe_url = server_url.ReplaceComponents(replace_host);

    command_line->AppendSwitchASCII(switches::kEafeUrl, eafe_url.spec());
    command_line->AppendSwitchASCII(switches::kEafePath,
                                    "login/easy_unlock/auth_code_login.html");
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

    fake_gaia_->SetFakeMergeSessionParams(kFakeUser, kFakeSid, kFakeLsid);
    fake_gaia_->MapEmailToGaiaId(kFakeUser, kFakeGaiaId);

    FakeGaia::AccessTokenInfo userinfo_token_info;
    userinfo_token_info.token = kFakeUserInfoToken;
    userinfo_token_info.scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
    userinfo_token_info.scopes.insert(GaiaConstants::kGoogleUserInfoProfile);
    userinfo_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    userinfo_token_info.email = kFakeUser;
    fake_gaia_->IssueOAuthToken(kFakeRefreshToken, userinfo_token_info);
  }

  void SetExpectedCredentials(const UserContext& user_context) {
    test::UserSessionManagerTestApi session_manager_test_api(
        UserSessionManager::GetInstance());
    session_manager_test_api.InjectStubUserContext(user_context);
  }

  bool JsExecute(const std::string& script) {
    return content::ExecuteScript(GetLoginUI()->GetWebContents(), script);
  }

  void SetTestEasyUnlockAppPath(const std::string& relative_path) {
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    EasyUnlockServiceFactory::GetInstance()->set_app_path_for_testing(
        test_data_dir.AppendASCII(relative_path));
  }

  void StartSignInScreen() {
    WizardController::SkipPostLoginScreensForTesting();
    WizardController* wizard_controller =
        WizardController::default_controller();
    ASSERT_TRUE(wizard_controller);
    wizard_controller->SkipToLoginForTesting(LoginScreenContext());
    OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
  }

  void OnBootstrapInitialized(bool success, const UserContext& user_context) {
    EXPECT_TRUE(success);
    SetExpectedCredentials(user_context);
  }

  void EafeLogin() {
    StartSignInScreen();
    JsExecute("cr.ui.Oobe.handleAccelerator('toggle_easy_bootstrap');");
  }
};

IN_PROC_BROWSER_TEST_F(BootstrapTest, Basic) {
  ScopedCompleteCallbackForTesting scoped_bootstrap_initialized(base::Bind(
      &BootstrapTest::OnBootstrapInitialized, base::Unretained(this)));

  SetTestEasyUnlockAppPath("login/easy_unlock/auto_pair_success");

  EafeLogin();

  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
}

IN_PROC_BROWSER_TEST_F(BootstrapTest, PRE_CleanUpFailedUser) {
  ScopedCompleteCallbackForTesting scoped_bootstrap_initialized(base::Bind(
      &BootstrapTest::OnBootstrapInitialized, base::Unretained(this)));

  SetTestEasyUnlockAppPath("login/easy_unlock/auto_pair_failure");

  EafeLogin();

  // Failed bootstrap should attempt exit and get out of this loop.
  content::RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(BootstrapTest, CleanUpFailedUser) {
  EXPECT_FALSE(user_manager::UserManager::Get()->IsKnownUser(
      AccountId::FromUserEmail(kFakeUser)));
}

}  // namespace chromeos
