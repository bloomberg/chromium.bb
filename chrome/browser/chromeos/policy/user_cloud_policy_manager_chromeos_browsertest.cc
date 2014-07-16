// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

const char kAccountId[] = "dla1@example.com";
const char kAccountPassword[] = "letmein";
const char* kStartupURLs[] = {"chrome://policy", "chrome://about"};
const char kTestAuthCode[] = "fake-auth-code";
const char kTestGaiaUberToken[] = "fake-uber-token";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestAuthSIDCookie[] = "fake-auth-SID-cookie";
const char kTestAuthLSIDCookie[] = "fake-auth-LSID-cookie";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";
const char kTestUserinfoToken[] = "fake-userinfo-token";

}  // namespace

class UserCloudPolicyManagerTest : public chromeos::OobeBaseTest {
 protected:
  UserCloudPolicyManagerTest() {
    set_open_about_blank_on_browser_launch(false);
  }

  virtual ~UserCloudPolicyManagerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    SetServerPolicy();

    test_server_.reset(new LocalPolicyTestServer(policy_file_path()));
    ASSERT_TRUE(test_server_->Start());

    OobeBaseTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    test_server_->GetServiceURL().spec());
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    SetMergeSessionParams(kAccountId);
    SetupGaiaServerWithAccessTokens();
    OobeBaseTest::SetUpOnMainThread();
  }

  void SetupGaiaServerWithAccessTokens() {
    FakeGaia::AccessTokenInfo token_info;
    token_info.token = kTestUserinfoToken;
    token_info.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
    token_info.scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
    token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
    token_info.email = kAccountId;
    fake_gaia_->IssueOAuthToken(kTestRefreshToken, token_info);
  }

  void SetMergeSessionParams(const std::string& email) {
    FakeGaia::MergeSessionParams params;
    params.auth_sid_cookie = kTestAuthSIDCookie;
    params.auth_lsid_cookie = kTestAuthLSIDCookie;
    params.auth_code = kTestAuthCode;
    params.refresh_token = kTestRefreshToken;
    params.access_token = kTestAuthLoginAccessToken;
    params.gaia_uber_token = kTestGaiaUberToken;
    params.session_sid_cookie = kTestSessionSIDCookie;
    params.session_lsid_cookie = kTestSessionLSIDCookie;
    params.email = email;
    fake_gaia_->SetMergeSessionParams(params);
  }

  void SkipToLoginScreen() {
    chromeos::WizardController::SkipPostLoginScreensForTesting();
    chromeos::WizardController* wizard_controller =
        chromeos::WizardController::default_controller();
    ASSERT_TRUE(wizard_controller);
    wizard_controller->SkipToLoginForTesting(chromeos::LoginScreenContext());

    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources()).Wait();
  }

  void LogIn(const std::string& user_id, const std::string& password) {
    GetLoginDisplay()->ShowSigninScreenForCreds(user_id, password);

    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources()).Wait();
  }

  void SetServerPolicy() {
    const char kPolicy[] =
        "{"
        "  \"%s\": {"
        "    \"mandatory\": {"
        "      \"RestoreOnStartup\": 4,"
        "      \"RestoreOnStartupURLs\": ["
        "        \"chrome://policy\","
        "        \"chrome://about\""
        "      ]"
        "    },"
        "    \"recommended\": {}"
        "  },"
        "  \"managed_users\": [ \"*\" ],"
        "  \"policy_user\": \"%s\","
        "  \"current_key_index\": 0"
        "}";

    const std::string policy = base::StringPrintf(
        kPolicy, dm_protocol::kChromeUserPolicyType, kAccountId);

    const int bytes_written =
        base::WriteFile(policy_file_path(), policy.data(), policy.size());
    ASSERT_EQ(static_cast<int>(policy.size()), bytes_written);
  }

  base::FilePath policy_file_path() const {
    return temp_dir_.path().AppendASCII("policy.json");
  }

  scoped_ptr<LocalPolicyTestServer> test_server_;

  base::ScopedTempDir temp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerTest);
};

IN_PROC_BROWSER_TEST_F(UserCloudPolicyManagerTest, StartSession) {
  SkipToLoginScreen();

  LogIn(kAccountId, kAccountPassword);

  // Check that the startup pages specified in policy were opened.
  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);

  TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  const int expected_tab_count = static_cast<int>(arraysize(kStartupURLs));
  EXPECT_EQ(expected_tab_count, tabs->count());
  for (int i = 0; i < expected_tab_count && i < tabs->count(); ++i) {
    EXPECT_EQ(GURL(kStartupURLs[i]),
              tabs->GetWebContentsAt(i)->GetVisibleURL());
  }
}

}  // namespace policy
