// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_setup_handler.h"

#include <vector>

#include "base/json/json_writer.h"
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

typedef GoogleServiceAuthError AuthError;

namespace {

MATCHER_P(ModelTypeSetMatches, value, "") { return arg.Equals(value); }

const char kTestUser[] = "chrome.p13n.test@gmail.com";
const char kTestPassword[] = "passwd";
const char kTestCaptcha[] = "pizzamyheart";
const char kTestCaptchaImageUrl[] = "http://pizzamyheart/image";
const char kTestCaptchaUnlockUrl[] = "http://pizzamyheart/unlock";

// List of all the types a user can select in the sync config dialog.
const syncable::ModelType kUserSelectableTypes[] = {
  syncable::APPS,
  syncable::AUTOFILL,
  syncable::BOOKMARKS,
  syncable::EXTENSIONS,
  syncable::PASSWORDS,
  syncable::PREFERENCES,
  syncable::SESSIONS,
  syncable::THEMES,
  syncable::TYPED_URLS
};

// Returns a ModelTypeSet with all user selectable types set.
syncable::ModelTypeSet GetAllTypes() {
  syncable::ModelTypeSet types;
  for (size_t i = 0; i < arraysize(kUserSelectableTypes); ++i)
    types.Put(kUserSelectableTypes[i]);
  return types;
}

enum SyncAllDataConfig {
  SYNC_ALL_DATA,
  CHOOSE_WHAT_TO_SYNC
};

enum EncryptAllConfig {
  ENCRYPT_ALL_DATA,
  ENCRYPT_PASSWORDS
};

// Create a json-format string with the key/value pairs appropriate for a call
// to HandleConfigure(). If |extra_values| is non-null, then the values from
// the passed dictionary are added to the json.
std::string GetConfiguration(const DictionaryValue* extra_values,
                             SyncAllDataConfig sync_all,
                             syncable::ModelTypeSet types,
                             const std::string& passphrase,
                             EncryptAllConfig encrypt_all) {
  DictionaryValue result;
  if (extra_values)
    result.MergeDictionary(extra_values);
  result.SetBoolean("syncAllDataTypes", sync_all == SYNC_ALL_DATA);
  result.SetBoolean("encryptAllData", encrypt_all == ENCRYPT_ALL_DATA);
  result.SetBoolean("usePassphrase", !passphrase.empty());
  if (!passphrase.empty())
    result.SetString("passphrase", passphrase);
  // Add all of our data types.
  result.SetBoolean("appsSynced", types.Has(syncable::APPS));
  result.SetBoolean("autofillSynced", types.Has(syncable::AUTOFILL));
  result.SetBoolean("bookmarksSynced", types.Has(syncable::BOOKMARKS));
  result.SetBoolean("extensionsSynced", types.Has(syncable::EXTENSIONS));
  result.SetBoolean("passwordsSynced", types.Has(syncable::PASSWORDS));
  result.SetBoolean("preferencesSynced", types.Has(syncable::PREFERENCES));
  result.SetBoolean("sessionsSynced", types.Has(syncable::SESSIONS));
  result.SetBoolean("themesSynced", types.Has(syncable::THEMES));
  result.SetBoolean("typedUrlsSynced", types.Has(syncable::TYPED_URLS));
  std::string args;
  base::JSONWriter::Write(&result, &args);
  return args;
}

void CheckInt(const DictionaryValue* dictionary,
              const std::string& key,
              int expected_value) {
  int actual_value;
  EXPECT_TRUE(dictionary->GetInteger(key, &actual_value)) <<
      "Did not expect to find value for " << key;
  EXPECT_EQ(actual_value, expected_value) <<
      "Mismatch found for " << key;
}

// Checks whether the passed |dictionary| contains a |key| with the given
// |expected_value|. If |omit_if_false| is true, then the value should only
// be present if |expected_value| is true.
void CheckBool(const DictionaryValue* dictionary,
               const std::string& key,
               bool expected_value,
               bool omit_if_false) {
  if (omit_if_false && !expected_value) {
    EXPECT_FALSE(dictionary->HasKey(key)) <<
        "Did not expect to find value for " << key;
  } else {
    bool actual_value;
    EXPECT_TRUE(dictionary->GetBoolean(key, &actual_value)) <<
        "No value found for " << key;
    EXPECT_EQ(actual_value, expected_value) <<
        "Mismatch found for " << key;
  }
}

void CheckBool(const DictionaryValue* dictionary,
               const std::string& key,
               bool expected_value) {
  return CheckBool(dictionary, key, expected_value, false);
}

void CheckString(const DictionaryValue* dictionary,
                 const std::string& key,
                 const std::string& expected_value,
                 bool omit_if_empty) {
  if (omit_if_empty && expected_value.empty()) {
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
void CheckShowSyncSetupArgs(const DictionaryValue* dictionary,
                            std::string error_message,
                            bool fatal_error,
                            int error,
                            std::string user,
                            bool user_is_editable,
                            std::string captcha_url) {
  // showSyncSetupPage() expects to be passed a dictionary with the following
  // named values set:
  //   error_message: custom error message to display.
  //   fatalError: true if there was a fatal error while logging in.
  //   error: GoogleServiceAuthError from previous login attempt (0 if none).
  //   user: The email the user most recently entered.
  //   editable_user: Whether the username field should be editable.
  //   captchaUrl: The captcha image to display to the user (empty if none).
  //
  // The code below validates these arguments.

  CheckString(dictionary, "errorMessage", error_message, true);
  CheckString(dictionary, "user", user, false);
  CheckString(dictionary, "captchaUrl", captcha_url, false);
  CheckInt(dictionary, "error", error);
  CheckBool(dictionary, "fatalError", fatal_error, true);
  CheckBool(dictionary, "editableUser", user_is_editable);
}

// Checks to make sure that the values stored in |dictionary| match the values
// expected by the showSyncSetupPage() JS function for a given set of data
// types.
void CheckConfigDataTypeArguments(DictionaryValue* dictionary,
                                  SyncAllDataConfig config,
                                  syncable::ModelTypeSet types) {
  CheckBool(dictionary, "syncAllDataTypes", config == SYNC_ALL_DATA);
  CheckBool(dictionary, "appsSynced", types.Has(syncable::APPS));
  CheckBool(dictionary, "autofillSynced", types.Has(syncable::AUTOFILL));
  CheckBool(dictionary, "bookmarksSynced", types.Has(syncable::BOOKMARKS));
  CheckBool(dictionary, "extensionsSynced", types.Has(syncable::EXTENSIONS));
  CheckBool(dictionary, "passwordsSynced", types.Has(syncable::PASSWORDS));
  CheckBool(dictionary, "preferencesSynced", types.Has(syncable::PREFERENCES));
  CheckBool(dictionary, "sessionsSynced", types.Has(syncable::SESSIONS));
  CheckBool(dictionary, "themesSynced", types.Has(syncable::THEMES));
  CheckBool(dictionary, "typedUrlsSynced", types.Has(syncable::TYPED_URLS));
}


}  // namespace

// Test instance of MockWebUI that tracks the data passed to
// CallJavascriptFunction().
class TestWebUI : public content::MockWebUI {
 public:
  virtual ~TestWebUI() {
    ClearTrackedCalls();
  }

  void ClearTrackedCalls() {
    // Manually free the arguments stored in CallData, since there's no good
    // way to use a self-freeing reference like scoped_ptr in a std::vector.
    for (std::vector<CallData>::iterator i = call_data_.begin();
         i != call_data_.end();
         ++i) {
      delete i->arg1;
      delete i->arg2;
    }
    call_data_.clear();
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
  SyncSetupHandlerTest() : error_(GoogleServiceAuthError::NONE) {}
  virtual void SetUp() OVERRIDE {
    error_ = GoogleServiceAuthError::None();;
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
    EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error_));
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
  GoogleServiceAuthError error_;
  SigninManagerMock* mock_signin_;
  TestWebUI web_ui_;
  scoped_ptr<TestingSyncSetupHandler> handler_;
};


TEST_F(SyncSetupHandlerTest, Basic) {
}

#if !defined(OS_CHROMEOS)
TEST_F(SyncSetupHandlerTest, DisplayBasicLogin) {
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  handler_->OpenSyncSetup(false);
  EXPECT_EQ(handler_.get(),
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
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(true));
  // This should display the login UI even though sync setup has already
  // completed.
  handler_->OpenSyncSetup(true);
  EXPECT_EQ(handler_.get(),
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
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
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
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, unrecoverable_error_detected())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(false);
  // Fake a failed signin attempt that requires a captcha.
  handler_->TryLogin(kTestUser, kTestPassword, "", "");
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromClientLoginCaptchaChallenge(
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
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
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
#endif  // !OS_CHROMEOS

#if !defined(OS_CHROMEOS)
// TODO(kochi): We need equivalent tests for ChromeOS.
TEST_F(SyncSetupHandlerTest, UnrecoverableErrorInitializingSync) {
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
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
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
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
#endif  // !OS_CHROMEOS

TEST_F(SyncSetupHandlerTest, TestSyncEverything) {
  std::string args = GetConfiguration(
      NULL, SYNC_ALL_DATA, GetAllTypes(), "", ENCRYPT_PASSWORDS);
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
  std::string args = GetConfiguration(
      NULL, SYNC_ALL_DATA, GetAllTypes(), "", ENCRYPT_ALL_DATA);
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
  std::string args = GetConfiguration(
      NULL, SYNC_ALL_DATA, GetAllTypes(), "", ENCRYPT_PASSWORDS);
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
  DictionaryValue dict;
  dict.SetBoolean("isGooglePassphrase", true);
  std::string args = GetConfiguration(&dict,
                                      SYNC_ALL_DATA,
                                      GetAllTypes(),
                                      "gaiaPassphrase",
                                      ENCRYPT_PASSWORDS);
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
  EXPECT_CALL(*mock_pss_, SetDecryptionPassphrase("gaiaPassphrase")).
      WillOnce(Return(true));

  handler_->HandleConfigure(&list_args);
  // We should navigate to "done" page since we finished configuring.
  ExpectDone();
}

TEST_F(SyncSetupHandlerTest, SelectCustomEncryption) {
  DictionaryValue dict;
  dict.SetBoolean("isGooglePassphrase", false);
  std::string args = GetConfiguration(&dict,
                                      SYNC_ALL_DATA,
                                      GetAllTypes(),
                                      "custom_passphrase",
                                      ENCRYPT_PASSWORDS);
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
              SetEncryptionPassphrase("custom_passphrase",
                                      ProfileSyncService::EXPLICIT));

  handler_->HandleConfigure(&list_args);
  // We should navigate to "done" page since we finished configuring.
  ExpectDone();
}

TEST_F(SyncSetupHandlerTest, UnsuccessfullySetPassphrase) {
  DictionaryValue dict;
  dict.SetBoolean("isGooglePassphrase", true);
  std::string args = GetConfiguration(&dict,
                                      SYNC_ALL_DATA,
                                      GetAllTypes(),
                                      "invalid_passphrase",
                                      ENCRYPT_PASSWORDS);
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
  EXPECT_CALL(*mock_pss_, SetDecryptionPassphrase("invalid_passphrase")).
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
  CheckBool(dictionary, "passphraseFailed", true);
}

// Walks through each user selectable type, and tries to sync just that single
// data type.
TEST_F(SyncSetupHandlerTest, TestSyncIndividualTypes) {
  for (size_t i = 0; i < arraysize(kUserSelectableTypes); ++i) {
    syncable::ModelTypeSet type_to_set;
    type_to_set.Put(kUserSelectableTypes[i]);
    std::string args = GetConfiguration(
        NULL, CHOOSE_WHAT_TO_SYNC, type_to_set, "", ENCRYPT_PASSWORDS);
    ListValue list_args;
    list_args.Append(new StringValue(args));
    EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
        .WillRepeatedly(Return(false));
    SetupInitializedProfileSyncService();
    EXPECT_CALL(*mock_pss_,
                OnUserChoseDatatypes(false, ModelTypeSetMatches(type_to_set)));
    handler_->HandleConfigure(&list_args);

    ExpectDone();
    Mock::VerifyAndClearExpectations(mock_pss_);
    web_ui_.ClearTrackedCalls();
  }
}

TEST_F(SyncSetupHandlerTest, TestSyncAllManually) {
  std::string args = GetConfiguration(
      NULL, CHOOSE_WHAT_TO_SYNC, GetAllTypes(), "", ENCRYPT_PASSWORDS);
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

#if !defined(OS_CHROMEOS)
TEST_F(SyncSetupHandlerTest, ShowSyncSetupWithAuthError) {
  // Initialize the system to a signed in state, but with an auth error.
  error_ = GoogleServiceAuthError(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  SetupInitializedProfileSyncService();
  mock_signin_->SetAuthenticatedUsername(kTestUser);
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  // This should display the login dialog (not login).
  handler_->OpenSyncSetup(false);

  EXPECT_EQ(handler_.get(),
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
  ASSERT_EQ(1U, web_ui_.call_data().size());
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
  std::string page;
  ASSERT_TRUE(data.arg1->GetAsString(&page));
  EXPECT_EQ(page, "login");
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  // We should display a login screen with a non-editable username filled in.
  CheckShowSyncSetupArgs(dictionary,
                         "",
                         false,
                         GoogleServiceAuthError::NONE,
                         kTestUser,
                         false,
                         "");
}
#endif

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
  CheckBool(dictionary, "appsRegistered", true);
  CheckBool(dictionary, "autofillRegistered", true);
  CheckBool(dictionary, "bookmarksRegistered", true);
  CheckBool(dictionary, "extensionsRegistered", true);
  CheckBool(dictionary, "passwordsRegistered", true);
  CheckBool(dictionary, "preferencesRegistered", true);
  CheckBool(dictionary, "sessionsRegistered", true);
  CheckBool(dictionary, "themesRegistered", true);
  CheckBool(dictionary, "typedUrlsRegistered", true);
  CheckBool(dictionary, "showPassphrase", false);
  CheckBool(dictionary, "usePassphrase", false);
  CheckBool(dictionary, "passphraseFailed", false);
  CheckBool(dictionary, "encryptAllData", false);
  CheckConfigDataTypeArguments(dictionary, SYNC_ALL_DATA, GetAllTypes());
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
  CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, GetAllTypes());
}

TEST_F(SyncSetupHandlerTest, ShowSetupSyncForAllTypesIndividually) {
  for (size_t i = 0; i < arraysize(kUserSelectableTypes); ++i) {
    EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
        .WillRepeatedly(Return(false));
    SetupInitializedProfileSyncService();
    browser_sync::SyncPrefs sync_prefs(profile_->GetPrefs());
    sync_prefs.SetKeepEverythingSynced(false);
    SetDefaultExpectationsForConfigPage();
    syncable::ModelTypeSet types;
    types.Put(kUserSelectableTypes[i]);
    EXPECT_CALL(*mock_pss_, GetPreferredDataTypes()).
        WillRepeatedly(Return(types));

    // This should display the sync setup dialog (not login).
    handler_->OpenSyncSetup(false);

    ExpectConfig();
    // Close the config overlay.
    LoginUIServiceFactory::GetForProfile(profile_.get())->LoginUIClosed(
        handler_.get());
    const TestWebUI::CallData& data = web_ui_.call_data()[0];
    DictionaryValue* dictionary;
    ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
    CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, types);
    Mock::VerifyAndClearExpectations(mock_pss_);
    // Clean up so we can loop back to display the dialog again.
    web_ui_.ClearTrackedCalls();
  }
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
  CheckBool(dictionary, "showPassphrase", true);
  CheckBool(dictionary, "usePassphrase", false);
  CheckBool(dictionary, "passphraseFailed", false);
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
  CheckBool(dictionary, "showPassphrase", true);
  CheckBool(dictionary, "usePassphrase", true);
  CheckBool(dictionary, "passphraseFailed", false);
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
