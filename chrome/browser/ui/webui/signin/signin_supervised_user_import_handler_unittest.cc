// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_supervised_user_import_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kTestGaiaId[] = "test-gaia-id";
const char kTestEmail[] = "foo@bar.com";

const char kTestWebUIResponse[] = "cr.webUIResponse";
const char kTestCallbackId[] = "test-callback-id";

}  // namespace

class TestSigninSupervisedUserImportHandler :
    public SigninSupervisedUserImportHandler {
 public:
  explicit TestSigninSupervisedUserImportHandler(content::WebUI* web_ui) {
    set_web_ui(web_ui);
  }
};

class SigninSupervisedUserImportHandlerTest : public BrowserWithTestWindowTest {
 public:
  SigninSupervisedUserImportHandlerTest() : web_ui_(new content::TestWebUI) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    handler_.reset(new TestSigninSupervisedUserImportHandler(web_ui()));

    // Authenticate the test profile.
    fake_signin_manager_ = static_cast<FakeSigninManagerForTesting*>(
        SigninManagerFactory::GetForProfile(profile()));
    fake_signin_manager_->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  }

  void TearDown() override {
    handler_.reset();
    web_ui_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{SigninManagerFactory::GetInstance(), BuildFakeSigninManagerBase}};
  }

  content::TestWebUI* web_ui() {
    return web_ui_.get();
  }

  TestSigninSupervisedUserImportHandler* handler() {
    return handler_.get();
  }

  FakeSigninManagerForTesting* signin_manager() {
    return fake_signin_manager_;
  }

  void VerifyResponse(size_t expected_total_calls,
                      const std::string& expected_callback_id,
                      bool expected_fulfilled) {
    EXPECT_EQ(expected_total_calls, web_ui()->call_data().size());

    EXPECT_EQ(kTestWebUIResponse, web_ui()->call_data()[0]->function_name());

    std::string callback_id;
    ASSERT_TRUE(web_ui()->call_data()[0]->arg1()->GetAsString(&callback_id));
    EXPECT_EQ(expected_callback_id, callback_id);

    bool fulfilled;
    ASSERT_TRUE(web_ui()->call_data()[0]->arg2()->GetAsBoolean(&fulfilled));
    EXPECT_EQ(expected_fulfilled, fulfilled);
  }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<TestSigninSupervisedUserImportHandler> handler_;
  FakeSigninManagerForTesting* fake_signin_manager_;
};

TEST_F(SigninSupervisedUserImportHandlerTest, NotAuthenticated) {
  // Sign out the user.
  signin_manager()->ForceSignOut();

  // Test the JS -> C++ -> JS callback path.
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(profile()->GetPath().AsUTF16Unsafe());
  handler()->GetExistingSupervisedUsers(&list_args);

  // Expect an error response.
  VerifyResponse(1U, kTestCallbackId, false);

  base::string16 expected_error_message = l10n_util::GetStringFUTF16(
      IDS_PROFILES_CREATE_CUSTODIAN_ACCOUNT_DETAILS_OUT_OF_DATE_ERROR,
      base::ASCIIToUTF16(profile()->GetProfileUserName()));
  base::string16 error_message;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg3()->GetAsString(&error_message));
  EXPECT_EQ(expected_error_message, error_message);
}

TEST_F(SigninSupervisedUserImportHandlerTest, AuthError) {
  // Set Auth Error.
  const GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile());
  token_service->UpdateCredentials(kTestGaiaId, "refresh_token");
  // TODO(https://crbug.com/836212): Do not use the delegate directly, because
  // it is internal API.
  token_service->GetDelegate()->UpdateAuthError(kTestGaiaId, error);

  // Test the JS -> C++ -> JS callback path.
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(profile()->GetPath().AsUTF16Unsafe());
  handler()->GetExistingSupervisedUsers(&list_args);

  // Expect an error response.
  VerifyResponse(1U, kTestCallbackId, false);

  base::string16 expected_error_message = l10n_util::GetStringFUTF16(
      IDS_PROFILES_CREATE_CUSTODIAN_ACCOUNT_DETAILS_OUT_OF_DATE_ERROR,
      base::ASCIIToUTF16(profile()->GetProfileUserName()));
  base::string16 error_message;
  ASSERT_TRUE(web_ui()->call_data()[0]->arg3()->GetAsString(&error_message));
  EXPECT_EQ(expected_error_message, error_message);
}

TEST_F(SigninSupervisedUserImportHandlerTest, CustodianIsSupervised) {
  // Build a supervised test profile.
  TestingProfile* supervised_profile = profile_manager()->CreateTestingProfile(
      "supervised-test-profile", nullptr,
      base::UTF8ToUTF16("supervised-test-profile"), 0,
      "12345",  // supervised_user_id
      TestingProfile::TestingFactories());

  // Test the JS -> C++ -> JS callback path.
  base::ListValue list_args;
  list_args.AppendString(kTestCallbackId);
  list_args.AppendString(supervised_profile->GetPath().AsUTF16Unsafe());
  handler()->GetExistingSupervisedUsers(&list_args);

  // Expect to do nothing.
  EXPECT_EQ(0U, web_ui()->call_data().size());
}
