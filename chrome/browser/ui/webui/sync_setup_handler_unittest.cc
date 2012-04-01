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
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::A;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;

MATCHER_P(ModelTypeSetMatches, value, "") { return arg.Equals(value); }

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
                                      const base::Value& arg1) OVERRIDE {
    call_data_.push_back(CallData());
    call_data_.back().function_name = function_name;
    call_data_.back().arg1 = arg1.DeepCopy();
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
    SyncPromoUI::RegisterUserPrefs(profile_->GetPrefs());
    mock_pss_ = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(),
            ProfileSyncServiceMock::BuildMockProfileSyncService));
    mock_signin_ = static_cast<SigninManagerMock*>(
        SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), BuildSigninManagerMock));
    handler_.reset(new TestingSyncSetupHandler(&web_ui_, profile_.get()));
  }

  // Returns a ModelTypeSet with all user selectable types set.
  syncable::ModelTypeSet GetAllTypes() {
    syncable::ModelTypeSet types;
    types.Put(syncable::APPS);
    types.Put(syncable::AUTOFILL);
    types.Put(syncable::BOOKMARKS);
    types.Put(syncable::EXTENSIONS);
    types.Put(syncable::PASSWORDS);
    types.Put(syncable::PREFERENCES);
    types.Put(syncable::SESSIONS);
    types.Put(syncable::THEMES);
    types.Put(syncable::TYPED_URLS);
    return types;
  }

  // Setup the expectations for calls made when displaying the config page.
  void SetDefaultExpectationsForConfigPage() {
    EXPECT_CALL(*mock_pss_, GetRegisteredDataTypes()).
        WillRepeatedly(Return(GetAllTypes()));
    EXPECT_CALL(*mock_pss_, GetPreferredDataTypes()).
        WillRepeatedly(Return(GetAllTypes()));
    EXPECT_CALL(*mock_pss_, EncryptEverythingEnabled()).
        WillRepeatedly(Return(false));
  }

  void SetupInitializedProfileSyncService() {
    // An initialized ProfileSyncService will have already completed sync setup
    // and will have an initialized sync backend.
    EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_pss_, sync_initialized()).WillRepeatedly(Return(true));
  }

  void ExpectConfig() {
    ASSERT_EQ(1U, web_ui_.call_data().size());
    const TestWebUI::CallData& data = web_ui_.call_data()[0];
    EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
    std::string page;
    ASSERT_TRUE(data.arg1->GetAsString(&page));
    EXPECT_EQ(page, "configure");
  }

  void ExpectDone() {
    ASSERT_EQ(1U, web_ui_.call_data().size());
    const TestWebUI::CallData& data = web_ui_.call_data()[0];
    EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
    std::string page;
    ASSERT_TRUE(data.arg1->GetAsString(&page));
    EXPECT_EQ(page, "done");
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

static void CheckBool(const DictionaryValue* dictionary,
                      const std::string& key,
                      bool expected_value) {
  return CheckBool(dictionary, key, expected_value, false);
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
  CheckBool(dictionary, "editable_user", user_is_editable);
}

TEST_F(SyncSetupHandlerTest, Basic) {
}

TEST_F(SyncSetupHandlerTest, DisplayBasicLogin) {
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  handler_->OpenSyncSetup(false);
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

TEST_F(SyncSetupHandlerTest, DisplayForceLogin) {
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(true));
  // This should display the login UI even though sync setup has already
  // completed.
  handler_->OpenSyncSetup(true);
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
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(false);
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
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(false);
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
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
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
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(false);
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
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(false);
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

TEST_F(SyncSetupHandlerTest, TestSyncEverything) {
  std::string args =
      "{\"syncAllDataTypes\":true,"
      "\"sync_apps\":true,"
      "\"sync_autofill\":true,"
      "\"sync_bookmarks\":true,"
      "\"sync_extensions\":true,"
      "\"sync_passwords\":true,"
      "\"sync_preferences\":true,"
      "\"sync_sessions\":true,"
      "\"sync_themes\":true,"
      "\"sync_typed_urls\":true,"
      "\"usePassphrase\":false,"
      "\"encryptAllData\":false}";
  ListValue list_args;
  list_args.Append(new StringValue(args));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  EXPECT_CALL(*mock_pss_, OnUserChoseDatatypes(true, _));
  handler_->HandleConfigure(&list_args);

  // Ensure that we navigated to the "done" state since we don't need a
  // passphrase.
  ExpectDone();
}

TEST_F(SyncSetupHandlerTest, TurnOnEncryptAll) {
  std::string args =
      "{\"syncAllDataTypes\":true,"
      "\"sync_apps\":true,"
      "\"sync_autofill\":true,"
      "\"sync_bookmarks\":true,"
      "\"sync_extensions\":true,"
      "\"sync_passwords\":true,"
      "\"sync_preferences\":true,"
      "\"sync_sessions\":true,"
      "\"sync_themes\":true,"
      "\"sync_typed_urls\":true,"
      "\"usePassphrase\":false,"
      "\"encryptAllData\":true}";
  ListValue list_args;
  list_args.Append(new StringValue(args));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  EXPECT_CALL(*mock_pss_, EnableEncryptEverything());
  EXPECT_CALL(*mock_pss_, OnUserChoseDatatypes(true, _));
  handler_->HandleConfigure(&list_args);

  // Ensure that we navigated to the "done" state since we don't need a
  // passphrase.
  ExpectDone();
}

TEST_F(SyncSetupHandlerTest, TestPassphraseStillRequired) {
  std::string args =
      "{\"syncAllDataTypes\":true,"
      "\"sync_apps\":true,"
      "\"sync_autofill\":true,"
      "\"sync_bookmarks\":true,"
      "\"sync_extensions\":true,"
      "\"sync_passwords\":true,"
      "\"sync_preferences\":true,"
      "\"sync_sessions\":true,"
      "\"sync_themes\":true,"
      "\"sync_typed_urls\":true,"
      "\"usePassphrase\":false,"
      "\"encryptAllData\":false}";
  ListValue list_args;
  list_args.Append(new StringValue(args));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  EXPECT_CALL(*mock_pss_, OnUserChoseDatatypes(_, _));
  SetDefaultExpectationsForConfigPage();

  // We should navigate back to the configure page since we need a passphrase.
  handler_->HandleConfigure(&list_args);

  ExpectConfig();
}

TEST_F(SyncSetupHandlerTest, SuccessfullySetPassphrase) {
  std::string args =
      "{\"syncAllDataTypes\":true,"
      "\"sync_apps\":true,"
      "\"sync_autofill\":true,"
      "\"sync_bookmarks\":true,"
      "\"sync_extensions\":true,"
      "\"sync_passwords\":true,"
      "\"sync_preferences\":true,"
      "\"sync_sessions\":true,"
      "\"sync_themes\":true,"
      "\"sync_typed_urls\":true,"
      "\"usePassphrase\":true,"
      "\"isGooglePassphrase\":true,"
      "\"passphrase\":\"whoopie\","
      "\"encryptAllData\":false}";
  ListValue list_args;
  list_args.Append(new StringValue(args));
  // Act as if an encryption passphrase is required the first time, then never
  // again after that.
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired()).WillOnce(Return(true));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  EXPECT_CALL(*mock_pss_, OnUserChoseDatatypes(_, _));
  EXPECT_CALL(*mock_pss_, SetDecryptionPassphrase("whoopie")).
      WillOnce(Return(true));

  handler_->HandleConfigure(&list_args);
  // We should navigate to "done" page since we finished configuring.
  ExpectDone();
}

TEST_F(SyncSetupHandlerTest, SelectCustomEncryption) {
  std::string args =
      "{\"syncAllDataTypes\":true,"
      "\"sync_apps\":true,"
      "\"sync_autofill\":true,"
      "\"sync_bookmarks\":true,"
      "\"sync_extensions\":true,"
      "\"sync_passwords\":true,"
      "\"sync_preferences\":true,"
      "\"sync_sessions\":true,"
      "\"sync_themes\":true,"
      "\"sync_typed_urls\":true,"
      "\"usePassphrase\":true,"
      "\"isGooglePassphrase\":false,"
      "\"passphrase\":\"whoopie\","
      "\"encryptAllData\":false}";
  ListValue list_args;
  list_args.Append(new StringValue(args));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  EXPECT_CALL(*mock_pss_, OnUserChoseDatatypes(_, _));
  EXPECT_CALL(*mock_pss_,
              SetEncryptionPassphrase("whoopie",
                                      ProfileSyncService::EXPLICIT));

  handler_->HandleConfigure(&list_args);
  // We should navigate to "done" page since we finished configuring.
  ExpectDone();
}

TEST_F(SyncSetupHandlerTest, UnsuccessfullySetPassphrase) {
  std::string args =
      "{\"syncAllDataTypes\":true,"
      "\"sync_apps\":true,"
      "\"sync_autofill\":true,"
      "\"sync_bookmarks\":true,"
      "\"sync_extensions\":true,"
      "\"sync_passwords\":true,"
      "\"sync_preferences\":true,"
      "\"sync_sessions\":true,"
      "\"sync_themes\":true,"
      "\"sync_typed_urls\":true,"
      "\"usePassphrase\":true,"
      "\"isGooglePassphrase\":true,"
      "\"passphrase\":\"whoopie\","
      "\"encryptAllData\":false}";
  ListValue list_args;
  list_args.Append(new StringValue(args));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  EXPECT_CALL(*mock_pss_, OnUserChoseDatatypes(_, _));
  EXPECT_CALL(*mock_pss_, SetDecryptionPassphrase("whoopie")).
      WillOnce(Return(false));

  SetDefaultExpectationsForConfigPage();
  // We should navigate back to the configure page since we need a passphrase.
  handler_->HandleConfigure(&list_args);

  ExpectConfig();

  // Make sure we display an error message to the user due to the failed
  // passphrase.
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "passphrase_failed", true);
}

TEST_F(SyncSetupHandlerTest, TestSyncOnlyBookmarks) {
  std::string args =
      "{\"syncAllDataTypes\":false,"
      "\"sync_apps\":false,"
      "\"sync_autofill\":false,"
      "\"sync_bookmarks\":true,"
      "\"sync_extensions\":false,"
      "\"sync_passwords\":false,"
      "\"sync_preferences\":false,"
      "\"sync_sessions\":false,"
      "\"sync_themes\":false,"
      "\"sync_typed_urls\":false,"
      "\"usePassphrase\":false,"
      "\"encryptAllData\":false}";
  ListValue list_args;
  list_args.Append(new StringValue(args));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  syncable::ModelTypeSet types;
  types.Put(syncable::BOOKMARKS);
  EXPECT_CALL(*mock_pss_,
              OnUserChoseDatatypes(false, ModelTypeSetMatches(types)));
  handler_->HandleConfigure(&list_args);

  ExpectDone();
}

TEST_F(SyncSetupHandlerTest, TestSyncAllManually) {
  std::string args =
      "{\"syncAllDataTypes\":false,"
      "\"sync_apps\":true,"
      "\"sync_autofill\":true,"
      "\"sync_bookmarks\":true,"
      "\"sync_extensions\":true,"
      "\"sync_passwords\":true,"
      "\"sync_preferences\":true,"
      "\"sync_sessions\":true,"
      "\"sync_themes\":true,"
      "\"sync_typed_urls\":true,"
      "\"usePassphrase\":false,"
      "\"encryptAllData\":false}";
  ListValue list_args;
  list_args.Append(new StringValue(args));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  EXPECT_CALL(*mock_pss_,
              OnUserChoseDatatypes(false, ModelTypeSetMatches(GetAllTypes())));
  handler_->HandleConfigure(&list_args);

  ExpectDone();
}

TEST_F(SyncSetupHandlerTest, ShowSyncSetup) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  // This should display the sync setup dialog (not login).
  SetDefaultExpectationsForConfigPage();
  handler_->OpenSyncSetup(false);

  ExpectConfig();
}

TEST_F(SyncSetupHandlerTest, ShowSetupSyncEverything) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  SetDefaultExpectationsForConfigPage();
  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(false);

  ExpectConfig();
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "showSyncEverythingPage", false);
  CheckBool(dictionary, "syncAllDataTypes", true);
  CheckBool(dictionary, "apps_registered", true);
  CheckBool(dictionary, "autofill_registered", true);
  CheckBool(dictionary, "bookmarks_registered", true);
  CheckBool(dictionary, "extensions_registered", true);
  CheckBool(dictionary, "passwords_registered", true);
  CheckBool(dictionary, "preferences_registered", true);
  CheckBool(dictionary, "sessions_registered", true);
  CheckBool(dictionary, "themes_registered", true);
  CheckBool(dictionary, "typed_urls_registered", true);
  CheckBool(dictionary, "show_passphrase", false);
  CheckBool(dictionary, "usePassphrase", false);
  CheckBool(dictionary, "passphrase_failed", false);
  CheckBool(dictionary, "encryptAllData", false);
}

TEST_F(SyncSetupHandlerTest, ShowSetupManuallySyncAll) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  browser_sync::SyncPrefs sync_prefs(profile_->GetPrefs());
  sync_prefs.SetKeepEverythingSynced(false);
  SetDefaultExpectationsForConfigPage();
  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(false);

  ExpectConfig();
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "syncAllDataTypes", false);
  CheckBool(dictionary, "sync_apps", true);
  CheckBool(dictionary, "sync_autofill", true);
  CheckBool(dictionary, "sync_bookmarks", true);
  CheckBool(dictionary, "sync_extensions", true);
  CheckBool(dictionary, "sync_passwords", true);
  CheckBool(dictionary, "sync_preferences", true);
  CheckBool(dictionary, "sync_sessions", true);
  CheckBool(dictionary, "sync_themes", true);
  CheckBool(dictionary, "sync_typed_urls", true);
}


// TODO(atwilson): Change this test to try individually syncing every data type
// not just bookmarks (http://crbug.com/119653).
TEST_F(SyncSetupHandlerTest, ShowSetupSyncOnlyBookmarks) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  browser_sync::SyncPrefs sync_prefs(profile_->GetPrefs());
  sync_prefs.SetKeepEverythingSynced(false);
  SetDefaultExpectationsForConfigPage();
  syncable::ModelTypeSet types;
  types.Put(syncable::BOOKMARKS);
  EXPECT_CALL(*mock_pss_, GetPreferredDataTypes()).
      WillRepeatedly(Return(types));

  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(false);

  ExpectConfig();
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "syncAllDataTypes", false);
  CheckBool(dictionary, "sync_apps", false);
  CheckBool(dictionary, "sync_autofill", false);
  CheckBool(dictionary, "sync_bookmarks", true);
  CheckBool(dictionary, "sync_extensions", false);
  CheckBool(dictionary, "sync_passwords", false);
  CheckBool(dictionary, "sync_preferences", false);
  CheckBool(dictionary, "sync_sessions", false);
  CheckBool(dictionary, "sync_themes", false);
  CheckBool(dictionary, "sync_typed_urls", false);
}

TEST_F(SyncSetupHandlerTest, ShowSetupGaiaPassphraseRequired) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  SetDefaultExpectationsForConfigPage();

  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(false);

  ExpectConfig();
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "show_passphrase", true);
  CheckBool(dictionary, "usePassphrase", false);
  CheckBool(dictionary, "passphrase_failed", false);
}

TEST_F(SyncSetupHandlerTest, ShowSetupCustomPassphraseRequired) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));
  SetupInitializedProfileSyncService();
  SetDefaultExpectationsForConfigPage();

  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(false);

  ExpectConfig();
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "show_passphrase", true);
  CheckBool(dictionary, "usePassphrase", true);
  CheckBool(dictionary, "passphrase_failed", false);
}

TEST_F(SyncSetupHandlerTest, ShowSetupEncryptAll) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  SetDefaultExpectationsForConfigPage();
  EXPECT_CALL(*mock_pss_, EncryptEverythingEnabled()).
      WillRepeatedly(Return(true));

  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(false);

  ExpectConfig();
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "encryptAllData", true);
}
