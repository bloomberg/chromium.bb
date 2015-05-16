// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
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

const char kTestAuthCode[] = "fake-auth-code";
const char kTestGaiaUberToken[] = "fake-uber-token";
const char kTestAuthLoginAccessToken[] = "fake-access-token";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestAuthSIDCookie[] = "fake-auth-SID-cookie";
const char kTestAuthLSIDCookie[] = "fake-auth-LSID-cookie";
const char kTestSessionSIDCookie[] = "fake-session-SID-cookie";
const char kTestSessionLSIDCookie[] = "fake-session-LSID-cookie";
const char kTestUserinfoToken[] = "fake-userinfo-token";

std::string GetPolicy(scoped_ptr<base::DictionaryValue> mandatory,
                      scoped_ptr<base::DictionaryValue> recommended,
                      const std::string& policyType,
                      const std::string& account) {
  scoped_ptr<base::DictionaryValue> policy_type_dict(new base::DictionaryValue);
  policy_type_dict->Set("mandatory", mandatory.Pass());
  policy_type_dict->Set("recommended", recommended.Pass());

  scoped_ptr<base::ListValue> managed_users_list(new base::ListValue);
  managed_users_list->AppendString("*");

  base::DictionaryValue root_dict;
  root_dict.Set(policyType, policy_type_dict.Pass());
  root_dict.Set("managed_users", managed_users_list.Pass());
  root_dict.SetString("policy_user", account);
  root_dict.SetInteger("current_key_index", 0);

  std::string json_policy;
  base::JSONWriter::WriteWithOptions(
      root_dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_policy);
  return json_policy;
}

}  // namespace

const char LoginPolicyTestBase::kAccountPassword[] = "letmein";
const char LoginPolicyTestBase::kAccountId[] = "user@example.com";

LoginPolicyTestBase::LoginPolicyTestBase() {
  // TODO(nkostylev): Fix this test harness for webview. http://crbug.com/477402
  set_use_webview(false);
  set_open_about_blank_on_browser_launch(false);
}

LoginPolicyTestBase::~LoginPolicyTestBase() {
}

void LoginPolicyTestBase::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  SetServerPolicy();

  test_server_.reset(new LocalPolicyTestServer(PolicyFilePath()));
  ASSERT_TRUE(test_server_->Start());

  OobeBaseTest::SetUp();
}

void LoginPolicyTestBase::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                  test_server_->GetServiceURL().spec());
  OobeBaseTest::SetUpCommandLine(command_line);
}

void LoginPolicyTestBase::SetUpOnMainThread() {
  SetMergeSessionParams(kAccountId);
  SetUpGaiaServerWithAccessTokens();
  OobeBaseTest::SetUpOnMainThread();
}

scoped_ptr<base::DictionaryValue>
LoginPolicyTestBase::GetMandatoryPoliciesValue() const {
  return make_scoped_ptr(new base::DictionaryValue);
}

scoped_ptr<base::DictionaryValue>
LoginPolicyTestBase::GetRecommendedPoliciesValue() const {
  return make_scoped_ptr(new base::DictionaryValue);
}

void LoginPolicyTestBase::SetUpGaiaServerWithAccessTokens() {
  FakeGaia::AccessTokenInfo token_info;
  token_info.token = kTestUserinfoToken;
  token_info.scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  token_info.scopes.insert(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  token_info.audience = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  token_info.email = kAccountId;
  fake_gaia_->IssueOAuthToken(kTestRefreshToken, token_info);
}

void LoginPolicyTestBase::SetMergeSessionParams(const std::string& email) {
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

void LoginPolicyTestBase::SkipToLoginScreen() {
  chromeos::WizardController::SkipPostLoginScreensForTesting();
  chromeos::WizardController* const wizard_controller =
      chromeos::WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  wizard_controller->SkipToLoginForTesting(chromeos::LoginScreenContext());

  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
}

void LoginPolicyTestBase::LogIn(const std::string& user_id,
                                const std::string& password) {
  GetLoginDisplay()->ShowSigninScreenForCreds(user_id, password);

  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_SESSION_STARTED,
      content::NotificationService::AllSources()).Wait();
}

void LoginPolicyTestBase::SetServerPolicy() {
  const std::string policy =
      GetPolicy(GetMandatoryPoliciesValue(), GetRecommendedPoliciesValue(),
                dm_protocol::kChromeUserPolicyType, kAccountId);

  const int bytes_written =
      base::WriteFile(PolicyFilePath(), policy.data(), policy.size());
  ASSERT_EQ(static_cast<int>(policy.size()), bytes_written);
}

base::FilePath LoginPolicyTestBase::PolicyFilePath() const {
  return temp_dir_.path().AppendASCII("policy.json");
}

}  // namespace policy
