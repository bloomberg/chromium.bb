// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_setup_handler.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "content/test/mock_web_ui.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::A;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;

static const char kTestUser[] = "chrome.p13n.test@gmail.com";
static const char kTestPassword[] = "passwd";
static const char kTestCaptcha[] = "pizzamyheart";
static const char kTestCaptchaImageUrl[] = "http://pizzamyheart/image";
static const char kTestCaptchaUnlockUrl[] = "http://pizzamyheart/unlock";

typedef GoogleServiceAuthError AuthError;

// Test instance of MockWebUI that tracks the data passed to
// CallJavascriptFunction().
class TestWebUI : public content::MockWebUI {
 public:
  virtual ~TestWebUI() {
    // Manually free the arguments stored in CallData, since there's no good
    // way to use a self-freeing reference like scoped_ptr in a std::vector.
    for (std::vector<CallData>::iterator i = call_data_.begin();
         i != call_data_.end();
         ++i) {
      delete i->arg1;
      delete i->arg2;
    }
  }

  virtual void CallJavascriptFunction(const std::string& function_name)
      OVERRIDE {
    call_data_.push_back(CallData());
    call_data_.back().function_name = function_name;
  }

  virtual void CallJavascriptFunction(const std::string& function_name,
                                      const base::Value& arg1,
                                      const base::Value& arg2) OVERRIDE {
    call_data_.push_back(CallData());
    call_data_.back().function_name = function_name;
    call_data_.back().arg1 = arg1.DeepCopy();
    call_data_.back().arg2 = arg2.DeepCopy();
  }
  class CallData {
   public:
    CallData() : arg1(NULL), arg2(NULL) {}
    std::string function_name;
    Value* arg1;
    Value* arg2;
  };
  const std::vector<CallData>& call_data() { return call_data_; }
 private:
  std::vector<CallData> call_data_;
};

class TestingSyncSetupHandler : public SyncSetupHandler {
 public:
  TestingSyncSetupHandler(content::WebUI* web_ui, Profile* profile)
      : SyncSetupHandler(NULL),
        profile_(profile) {
    set_web_ui(web_ui);
  }
  ~TestingSyncSetupHandler() {
    set_web_ui(NULL);
  }

  virtual void ShowSetupUI() OVERRIDE {}

  virtual Profile* GetProfile() const OVERRIDE { return profile_; }

 private:
  // Weak pointer to parent profile.
  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(TestingSyncSetupHandler);
};

class SigninManagerMock : public FakeSigninManager {
 public:
  SigninManagerMock() {}

  virtual void StartSignIn(const std::string& username,
                           const std::string& password,
                           const std::string& token,
                           const std::string& captcha) OVERRIDE {
    FakeSigninManager::StartSignIn(username, password, token, captcha);
    username_ = username;
    password_ = password;
    captcha_ = captcha;
  }

  void ResetTestStats() {
    username_.clear();
    password_.clear();
    captcha_.clear();
  }

  std::string username_;
  std::string password_;
  std::string captcha_;
};

static ProfileKeyedService* BuildSigninManagerMock(Profile* profile) {
  return new SigninManagerMock();
}

class SyncSetupHandlerTest : public testing::Test {
 public:
  SyncSetupHandlerTest() {}
  virtual void SetUp() OVERRIDE {
    profile_.reset(ProfileSyncServiceMock::MakeSignedInTestingProfile());
    mock_pss_ = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(),
            ProfileSyncServiceMock::BuildMockProfileSyncService));
    mock_signin_ = static_cast<SigninManagerMock*>(
        SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), BuildSigninManagerMock));
    handler_.reset(new TestingSyncSetupHandler(&web_ui_, profile_.get()));
  }

  scoped_ptr<Profile> profile_;
  ProfileSyncServiceMock* mock_pss_;
  SigninManagerMock* mock_signin_;
  TestWebUI web_ui_;
  scoped_ptr<TestingSyncSetupHandler> handler_;
};

static void CheckInt(const DictionaryValue* dictionary,
                     const std::string& key,
                     int expected_value) {
  int actual_value;
  EXPECT_TRUE(dictionary->GetInteger(key, &actual_value)) <<
      "Did not expect to find value for " << key;;
  EXPECT_EQ(actual_value, expected_value) <<
      "Mismatch found for " << key;;
}

static void CheckBool(const DictionaryValue* dictionary,
                      const std::string& key,
                      bool expected_value,
                      bool is_optional) {
  if (is_optional && !expected_value) {
    EXPECT_FALSE(dictionary->HasKey(key)) <<
        "Did not expect to find value for " << key;;
  } else {
    bool actual_value;
    EXPECT_TRUE(dictionary->GetBoolean(key, &actual_value)) <<
        "No value found for " << key;
    EXPECT_EQ(actual_value, expected_value) <<
        "Mismatch found for " << key;
  }
}

static void CheckString(const DictionaryValue* dictionary,
                        const std::string& key,
                        const std::string& expected_value,
                        bool is_optional) {
  if (is_optional && expected_value.empty()) {
    EXPECT_FALSE(dictionary->HasKey(key)) <<
        "Did not expect to find value for " << key;
  } else {
    std::string actual_value;
    EXPECT_TRUE(dictionary->GetString(key, &actual_value)) <<
        "No value found for " << key;
    EXPECT_EQ(actual_value, expected_value) <<
        "Mismatch found for " << key;
  }
}

// Validates that the expected args are being passed off to javascript.
static void CheckShowSyncSetupArgs(const DictionaryValue* dictionary,
                                   std::string error_message,
                                   bool fatal_error,
                                   int error,
                                   std::string user,
                                   bool user_is_editable,
                                   std::string captcha_url) {
  // showSyncSetupPage() expects to be passed a dictionary with the following
  // named values set:
  // error_message: custom error message to display.
  // fatalError: true if there was a fatal error while logging in.
  // error: GoogleServiceAuthError from previous login attempt (0 if none).
  // user: The email the user most recently entered.
  // editable_user: Whether the username field should be editable.
  // captchaUrl: The captcha image to display to the user (empty if none).
  //
  // The code below validates these arguments.

  CheckString(dictionary, "error_message", error_message, true);
  CheckString(dictionary, "user", user, false);
  CheckString(dictionary, "captchaUrl", captcha_url, false);
  CheckInt(dictionary, "error", error);
  CheckBool(dictionary, "fatalError", fatal_error, true);
  CheckBool(dictionary, "editable_user", user_is_editable, false);
}

TEST_F(SyncSetupHandlerTest, Basic) {
}

TEST_F(SyncSetupHandlerTest, DisplayBasicLogin) {
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable())
      .WillRepeatedly(Return(false));
  handler_->OpenSyncSetup();
  EXPECT_EQ(&web_ui_,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
  ASSERT_EQ(1U, web_ui_.call_data().size());
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
  std::string page;
  ASSERT_TRUE(data.arg1->GetAsString(&page));
  EXPECT_EQ(page, "login");
  // Now make sure that the appropriate params are being passed.
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckShowSyncSetupArgs(
      dictionary, "", false, GoogleServiceAuthError::NONE, "", true, "");
  handler_->CloseSyncSetup();
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}

TEST_F(SyncSetupHandlerTest, HandleGaiaAuthFailure) {
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, unrecoverable_error_detected())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup();
  // Fake a failed signin attempt.
  handler_->TryLogin(kTestUser, kTestPassword, "", "");
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  handler_->SigninFailed(error);
  ASSERT_EQ(2U, web_ui_.call_data().size());
  // Validate the second JS call (the first call was already tested by
  // the DisplayBasicLogin test).
  const TestWebUI::CallData& data = web_ui_.call_data()[1];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
  std::string page;
  ASSERT_TRUE(data.arg1->GetAsString(&page));
  EXPECT_EQ(page, "login");
  // Now make sure that the appropriate params are being passed.
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckShowSyncSetupArgs(
      dictionary, "", false, GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
      kTestUser, true, "");
}

TEST_F(SyncSetupHandlerTest, HandleCaptcha) {
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, unrecoverable_error_detected())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup();
  // Fake a failed signin attempt that requires a captcha.
  handler_->TryLogin(kTestUser, kTestPassword, "", "");
  GoogleServiceAuthError error = GoogleServiceAuthError::FromCaptchaChallenge(
      "token", GURL(kTestCaptchaImageUrl), GURL(kTestCaptchaUnlockUrl));
  handler_->SigninFailed(error);
  ASSERT_EQ(2U, web_ui_.call_data().size());
  // Validate the second JS call (the first call was already tested by
  // the DisplayBasicLogin test).
  const TestWebUI::CallData& data = web_ui_.call_data()[1];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
  std::string page;
  ASSERT_TRUE(data.arg1->GetAsString(&page));
  EXPECT_EQ(page, "login");
  // Now make sure that the appropriate params are being passed.
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckShowSyncSetupArgs(
      dictionary, "", false, GoogleServiceAuthError::CAPTCHA_REQUIRED,
      kTestUser, true, kTestCaptchaImageUrl);
}

TEST_F(SyncSetupHandlerTest, HandleFatalError) {
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable())
      .WillRepeatedly(Return(false));
  handler_->ShowFatalError();
  ASSERT_EQ(1U, web_ui_.call_data().size());
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
  std::string page;
  ASSERT_TRUE(data.arg1->GetAsString(&page));
  EXPECT_EQ(page, "login");
  // Now make sure that the appropriate params are being passed.
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckShowSyncSetupArgs(
      dictionary, "", true, GoogleServiceAuthError::NONE, "", true, "");
}

TEST_F(SyncSetupHandlerTest, UnrecoverableErrorInitializingSync) {
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup();
  ASSERT_EQ(1U, web_ui_.call_data().size());
  // Fake a successful GAIA request (gaia credentials valid, but signin not
  // complete yet).
  handler_->TryLogin(kTestUser, kTestPassword, "", "");
  handler_->GaiaCredentialsValid();
  ASSERT_EQ(2U, web_ui_.call_data().size());
  EXPECT_EQ("SyncSetupOverlay.showSuccessAndSettingUp",
            web_ui_.call_data()[1].function_name);
  // Now fake a sync error.
  GoogleServiceAuthError none(GoogleServiceAuthError::NONE);
  EXPECT_CALL(*mock_pss_, unrecoverable_error_detected())
      .WillRepeatedly(Return(true));
  mock_signin_->SignOut();
  handler_->SigninFailed(none);
  ASSERT_EQ(3U, web_ui_.call_data().size());
  // Validate the second JS call (the first call was already tested by
  // the DisplayBasicLogin test).
  const TestWebUI::CallData& data = web_ui_.call_data()[2];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
  std::string page;
  ASSERT_TRUE(data.arg1->GetAsString(&page));
  EXPECT_EQ(page, "login");
  // Now make sure that the appropriate params are being passed.
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckShowSyncSetupArgs(
      dictionary, "", true, GoogleServiceAuthError::NONE,
      kTestUser, true, "");
}

TEST_F(SyncSetupHandlerTest, GaiaErrorInitializingSync) {
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup();
  ASSERT_EQ(1U, web_ui_.call_data().size());
  // Fake a successful GAIA request (gaia credentials valid, but signin not
  // complete yet).
  handler_->TryLogin(kTestUser, kTestPassword, "", "");
  handler_->GaiaCredentialsValid();
  ASSERT_EQ(2U, web_ui_.call_data().size());
  EXPECT_EQ("SyncSetupOverlay.showSuccessAndSettingUp",
            web_ui_.call_data()[1].function_name);
  // Now fake a sync gaia error.
  GoogleServiceAuthError unavailable(
      GoogleServiceAuthError::SERVICE_UNAVAILABLE);
  EXPECT_CALL(*mock_pss_, unrecoverable_error_detected())
      .WillRepeatedly(Return(false));
  mock_signin_->SignOut();
  handler_->SigninFailed(unavailable);
  ASSERT_EQ(3U, web_ui_.call_data().size());
  // Validate the second JS call (the first call was already tested by
  // the DisplayBasicLogin test).
  const TestWebUI::CallData& data = web_ui_.call_data()[2];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
  std::string page;
  ASSERT_TRUE(data.arg1->GetAsString(&page));
  EXPECT_EQ(page, "login");
  // Now make sure that the appropriate params are being passed.
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckShowSyncSetupArgs(
      dictionary, "", false, GoogleServiceAuthError::SERVICE_UNAVAILABLE,
      kTestUser, true, "");
}
