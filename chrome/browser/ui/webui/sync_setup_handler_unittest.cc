// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_setup_handler.h"

#include <vector>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Values;

typedef GoogleServiceAuthError AuthError;

namespace {

MATCHER_P(ModelTypeSetMatches, value, "") { return arg.Equals(value); }

const char kTestUser[] = "chrome.p13n.test@gmail.com";
const char kTestPassword[] = "passwd";
const char kTestCaptcha[] = "pizzamyheart";
const char kTestCaptchaImageUrl[] = "http://pizzamyheart/image";
const char kTestCaptchaUnlockUrl[] = "http://pizzamyheart/unlock";

// List of all the types a user can select in the sync config dialog.
const syncer::ModelType kUserSelectableTypes[] = {
  syncer::APPS,
  syncer::AUTOFILL,
  syncer::BOOKMARKS,
  syncer::EXTENSIONS,
  syncer::PASSWORDS,
  syncer::PREFERENCES,
  syncer::SESSIONS,
  syncer::THEMES,
  syncer::TYPED_URLS
};

// Returns a ModelTypeSet with all user selectable types set.
syncer::ModelTypeSet GetAllTypes() {
  syncer::ModelTypeSet types;
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
                             syncer::ModelTypeSet types,
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
  result.SetBoolean("appsSynced", types.Has(syncer::APPS));
  result.SetBoolean("autofillSynced", types.Has(syncer::AUTOFILL));
  result.SetBoolean("bookmarksSynced", types.Has(syncer::BOOKMARKS));
  result.SetBoolean("extensionsSynced", types.Has(syncer::EXTENSIONS));
  result.SetBoolean("passwordsSynced", types.Has(syncer::PASSWORDS));
  result.SetBoolean("preferencesSynced", types.Has(syncer::PREFERENCES));
  result.SetBoolean("sessionsSynced", types.Has(syncer::SESSIONS));
  result.SetBoolean("themesSynced", types.Has(syncer::THEMES));
  result.SetBoolean("typedUrlsSynced", types.Has(syncer::TYPED_URLS));
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
                                  syncer::ModelTypeSet types) {
  CheckBool(dictionary, "syncAllDataTypes", config == SYNC_ALL_DATA);
  CheckBool(dictionary, "appsSynced", types.Has(syncer::APPS));
  CheckBool(dictionary, "autofillSynced", types.Has(syncer::AUTOFILL));
  CheckBool(dictionary, "bookmarksSynced", types.Has(syncer::BOOKMARKS));
  CheckBool(dictionary, "extensionsSynced", types.Has(syncer::EXTENSIONS));
  CheckBool(dictionary, "passwordsSynced", types.Has(syncer::PASSWORDS));
  CheckBool(dictionary, "preferencesSynced", types.Has(syncer::PREFERENCES));
  CheckBool(dictionary, "sessionsSynced", types.Has(syncer::SESSIONS));
  CheckBool(dictionary, "themesSynced", types.Has(syncer::THEMES));
  CheckBool(dictionary, "typedUrlsSynced", types.Has(syncer::TYPED_URLS));
}


}  // namespace

// Test instance of WebUI that tracks the data passed to
// CallJavascriptFunction().
class TestWebUI : public content::WebUI {
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

  virtual content::WebContents* GetWebContents() const OVERRIDE {
    return NULL;
  }
  virtual content::WebUIController* GetController() const OVERRIDE {
    return NULL;
  }
  virtual void SetController(content::WebUIController* controller) OVERRIDE {}
  virtual ui::ScaleFactor GetDeviceScaleFactor() const OVERRIDE {
    return ui::SCALE_FACTOR_100P;
  }
  virtual bool ShouldHideFavicon() const OVERRIDE {
    return false;
  }
  virtual void HideFavicon() OVERRIDE {}
  virtual bool ShouldFocusLocationBarByDefault() const OVERRIDE {
    return false;
  }
  virtual void FocusLocationBarByDefault() OVERRIDE {}
  virtual bool ShouldHideURL() const OVERRIDE {
    return false;
  }
  virtual void HideURL() OVERRIDE {}
  virtual const string16& GetOverriddenTitle() const OVERRIDE {
    return temp_string_;
  }
  virtual void OverrideTitle(const string16& title) OVERRIDE {}
  virtual content::PageTransition GetLinkTransitionType() const OVERRIDE {
    return content::PAGE_TRANSITION_LINK;
  }
  virtual void SetLinkTransitionType(content::PageTransition type) OVERRIDE {}
  virtual int GetBindings() const OVERRIDE {
    return 0;
  }
  virtual void SetBindings(int bindings) OVERRIDE {}
  virtual void SetFrameXPath(const std::string& xpath) OVERRIDE {}
  virtual void AddMessageHandler(
      content::WebUIMessageHandler* handler) OVERRIDE {}
  virtual void RegisterMessageCallback(
      const std::string& message,
      const MessageCallback& callback) OVERRIDE {}
  virtual void ProcessWebUIMessage(const GURL& source_url,
                                   const std::string& message,
                                   const base::ListValue& args) OVERRIDE {}
  virtual void CallJavascriptFunction(const std::string& function_name,
                                      const base::Value& arg1,
                                      const base::Value& arg2,
                                      const base::Value& arg3) OVERRIDE {}
  virtual void CallJavascriptFunction(const std::string& function_name,
                                      const base::Value& arg1,
                                      const base::Value& arg2,
                                      const base::Value& arg3,
                                      const base::Value& arg4) OVERRIDE {}
  virtual void CallJavascriptFunction(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) OVERRIDE {}

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
  string16 temp_string_;
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
  virtual void FocusUI() OVERRIDE {}

  virtual Profile* GetProfile() const OVERRIDE { return profile_; }

  using SyncSetupHandler::is_configuring_sync;
  using SyncSetupHandler::have_signin_tracker;

 private:
  void DisplayGaiaLoginInNewTab() OVERRIDE {}

  // Weak pointer to parent profile.
  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(TestingSyncSetupHandler);
};

class SigninManagerMock : public FakeSigninManager {
 public:
  explicit SigninManagerMock(Profile* profile) : FakeSigninManager(profile) {}
  MOCK_CONST_METHOD1(IsAllowedUsername, bool(const std::string& username));
};

static ProfileKeyedService* BuildSigninManagerMock(Profile* profile) {
  return new SigninManagerMock(profile);
}

// The boolean parameter indicates whether the test is run with ClientOAuth
// or not.  The test parameter is a bool: whether or not to test with/
// /ClientLogin enabled or not.
class SyncSetupHandlerTest : public testing::TestWithParam<bool> {
 public:
  SyncSetupHandlerTest() : error_(GoogleServiceAuthError::NONE) {}
  virtual void SetUp() OVERRIDE {
    bool use_client_login_flow = GetParam();
    if (use_client_login_flow) {
      DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseWebBasedSigninFlow));
    } else {
      if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseWebBasedSigninFlow)) {
            CommandLine::ForCurrentProcess()->AppendSwitch(
                switches::kUseWebBasedSigninFlow);
      }
    }

    error_ = GoogleServiceAuthError::None();
    profile_.reset(ProfileSyncServiceMock::MakeSignedInTestingProfile());
    mock_pss_ = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(),
            ProfileSyncServiceMock::BuildMockProfileSyncService));
    mock_pss_->Initialize();

    ON_CALL(*mock_pss_, GetPassphraseType()).WillByDefault(
        Return(syncer::IMPLICIT_PASSPHRASE));
    ON_CALL(*mock_pss_, GetPassphraseTime()).WillByDefault(
        Return(base::Time()));
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
  // MessageLoop instance is required to work with OneShotTimer.
  MessageLoop message_loop_;
  SigninManagerMock* mock_signin_;
  TestWebUI web_ui_;
  scoped_ptr<TestingSyncSetupHandler> handler_;
};

TEST_P(SyncSetupHandlerTest, Basic) {
}

TEST_P(SyncSetupHandlerTest, DisplayBasicLogin) {
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

  if (!SyncPromoUI::UseWebBasedSigninFlow()) {
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
  } else {
    ASSERT_FALSE(handler_->is_configuring_sync());
    ASSERT_TRUE(handler_->have_signin_tracker());
  }

  handler_->CloseSyncSetup();
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}

TEST_P(SyncSetupHandlerTest, DisplayForceLogin) {
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

  if (!SyncPromoUI::UseWebBasedSigninFlow()) {
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
  } else {
    ASSERT_FALSE(handler_->is_configuring_sync());
    ASSERT_TRUE(handler_->have_signin_tracker());
  }

  handler_->CloseSyncSetup();
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}

// Verifies that the handler correctly handles a cancellation when
// it is displaying the spinner to the user.
TEST_P(SyncSetupHandlerTest, DisplayConfigureWithBackendDisabledAndCancel) {
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  error_ = GoogleServiceAuthError::None();
  EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error_));
  EXPECT_CALL(*mock_pss_, sync_initialized()).WillRepeatedly(Return(false));

  // We're simulating a user setting up sync, which would cause the backend to
  // kick off initialization, but not download user data types. The sync
  // backend will try to download control data types (e.g encryption info), but
  // that won't finish for this test as we're simulating cancelling while the
  // spinner is showing.
  handler_->OpenSyncSetup(false);

  // When the SigninTracker is initialized here, a signin failure is triggered
  // due to sync_initialized() returning false, causing the current login UI to
  // be dismissed.
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());

  // We expect a call to SyncSetupOverlay.showSyncSetupPage. Some variations of
  // this test also include a call to OptionsPage.closeOverlay, that we ignore.
  EXPECT_LE(1U, web_ui_.call_data().size());

  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
  std::string page;
  ASSERT_TRUE(data.arg1->GetAsString(&page));
  EXPECT_EQ(page, "spinner");
  // Cancelling the spinner dialog will cause CloseSyncSetup().
  handler_->CloseSyncSetup();
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}

// Verifies that the handler correctly transitions from showing the spinner
// to showing a configuration page when signin completes successfully.
TEST_P(SyncSetupHandlerTest,
       DisplayConfigureWithBackendDisabledAndSigninSuccess) {
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  error_ = GoogleServiceAuthError::None();
  EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error_));
  // Sync backend is stopped initially, and will start up.
  EXPECT_CALL(*mock_pss_, sync_initialized())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  SetDefaultExpectationsForConfigPage();

  handler_->OpenSyncSetup(false);

  // We expect a call to SyncSetupOverlay.showSyncSetupPage. Some variations of
  // this test also include a call to OptionsPage.closeOverlay, that we ignore.
  EXPECT_LE(1U, web_ui_.call_data().size());

  const TestWebUI::CallData& data0 = web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data0.function_name);
  std::string page;
  ASSERT_TRUE(data0.arg1->GetAsString(&page));
  EXPECT_EQ(page, "spinner");
  handler_->SigninSuccess();

  // On signin success, the dialog will proceed from spinner to configure sync
  // everything. There is no login UI once signin is successful.
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());

  // We expect a second call to SyncSetupOverlay.showSyncSetupPage. Some
  // variations of this test also include a call to OptionsPage.closeOverlay,
  // that we ignore.
  EXPECT_LE(2U, web_ui_.call_data().size());
  const TestWebUI::CallData& data1 = web_ui_.call_data().back();
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data1.function_name);
  ASSERT_TRUE(data1.arg1->GetAsString(&page));
  EXPECT_EQ(page, "configure");
  DictionaryValue* dictionary;
  ASSERT_TRUE(data1.arg2->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "passphraseFailed", false);
  CheckBool(dictionary, "showSyncEverythingPage", true);
  CheckBool(dictionary, "syncAllDataTypes", true);
  CheckBool(dictionary, "encryptAllData", false);
  CheckBool(dictionary, "usePassphrase", false);
}

// Verifies the case where the user cancels after the sync backend has
// initialized (meaning it already transitioned from the spinner to a proper
// configuration page, tested by
// DisplayConfigureWithBackendDisabledAndSigninSuccess), but before the user
// before the user has continued on.
TEST_P(SyncSetupHandlerTest,
       DisplayConfigureWithBackendDisabledAndCancelAfterSigninSuccess) {
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  error_ = GoogleServiceAuthError::None();
  EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error_));
  EXPECT_CALL(*mock_pss_, sync_initialized())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  SetDefaultExpectationsForConfigPage();
  handler_->OpenSyncSetup(false);
  handler_->SigninSuccess();

  // It's important to tell sync the user cancelled the setup flow before we
  // tell it we're through with the setup progress.
  testing::InSequence seq;
  EXPECT_CALL(*mock_pss_, DisableForUser());
  EXPECT_CALL(*mock_pss_, SetSetupInProgress(false));

  handler_->CloseSyncSetup();
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}

TEST_P(SyncSetupHandlerTest,
       DisplayConfigureWithBackendDisabledAndSigninFalied) {
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  error_ = GoogleServiceAuthError::None();
  EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error_));
  EXPECT_CALL(*mock_pss_, sync_initialized()).WillRepeatedly(Return(false));

  handler_->OpenSyncSetup(false);
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
  std::string page;
  ASSERT_TRUE(data.arg1->GetAsString(&page));
  EXPECT_EQ(page, "spinner");
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  handler_->SigninFailed(error);
  // On failure, the dialog will be closed.
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}

TEST_P(SyncSetupHandlerTest, HandleGaiaAuthFailure) {
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasUnrecoverableError())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(false);

  if (!SyncPromoUI::UseWebBasedSigninFlow()) {
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
  } else {
    ASSERT_FALSE(handler_->is_configuring_sync());
    ASSERT_TRUE(handler_->have_signin_tracker());
  }
}

TEST_P(SyncSetupHandlerTest, HandleCaptcha) {
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasUnrecoverableError())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(false);

  if (!SyncPromoUI::UseWebBasedSigninFlow()) {
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
  } else {
    ASSERT_FALSE(handler_->is_configuring_sync());
    ASSERT_TRUE(handler_->have_signin_tracker());
  }
}

// TODO(kochi): We need equivalent tests for ChromeOS.
TEST_P(SyncSetupHandlerTest, UnrecoverableErrorInitializingSync) {
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(false);

  if (!SyncPromoUI::UseWebBasedSigninFlow()) {
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
    EXPECT_CALL(*mock_pss_, HasUnrecoverableError())
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
  } else {
    ASSERT_FALSE(handler_->is_configuring_sync());
    ASSERT_TRUE(handler_->have_signin_tracker());
  }
}

TEST_P(SyncSetupHandlerTest, GaiaErrorInitializingSync) {
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasSyncSetupCompleted())
      .WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(false);

  if (!SyncPromoUI::UseWebBasedSigninFlow()) {
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
    EXPECT_CALL(*mock_pss_, HasUnrecoverableError())
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
  } else {
    ASSERT_FALSE(handler_->is_configuring_sync());
    ASSERT_TRUE(handler_->have_signin_tracker());
  }
}

TEST_P(SyncSetupHandlerTest, TestSyncEverything) {
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

TEST_P(SyncSetupHandlerTest, TurnOnEncryptAll) {
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

TEST_P(SyncSetupHandlerTest, TestPassphraseStillRequired) {
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

TEST_P(SyncSetupHandlerTest, SuccessfullySetPassphrase) {
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

TEST_P(SyncSetupHandlerTest, SelectCustomEncryption) {
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

TEST_P(SyncSetupHandlerTest, UnsuccessfullySetPassphrase) {
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
TEST_P(SyncSetupHandlerTest, TestSyncIndividualTypes) {
  for (size_t i = 0; i < arraysize(kUserSelectableTypes); ++i) {
    syncer::ModelTypeSet type_to_set;
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

TEST_P(SyncSetupHandlerTest, TestSyncAllManually) {
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

TEST_P(SyncSetupHandlerTest, ShowSyncSetup) {
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

TEST_P(SyncSetupHandlerTest, ShowSyncSetupWithAuthError) {
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

  if (!SyncPromoUI::UseWebBasedSigninFlow()) {
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
  } else {
    ASSERT_FALSE(handler_->is_configuring_sync());
    ASSERT_TRUE(handler_->have_signin_tracker());
  }
}

TEST_P(SyncSetupHandlerTest, ShowSetupSyncEverything) {
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

TEST_P(SyncSetupHandlerTest, ShowSetupManuallySyncAll) {
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

TEST_P(SyncSetupHandlerTest, ShowSetupSyncForAllTypesIndividually) {
  for (size_t i = 0; i < arraysize(kUserSelectableTypes); ++i) {
    EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
        .WillRepeatedly(Return(false));
    SetupInitializedProfileSyncService();
    browser_sync::SyncPrefs sync_prefs(profile_->GetPrefs());
    sync_prefs.SetKeepEverythingSynced(false);
    SetDefaultExpectationsForConfigPage();
    syncer::ModelTypeSet types;
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

TEST_P(SyncSetupHandlerTest, ShowSetupGaiaPassphraseRequired) {
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

TEST_P(SyncSetupHandlerTest, ShowSetupCustomPassphraseRequired) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, GetPassphraseType())
      .WillRepeatedly(Return(syncer::CUSTOM_PASSPHRASE));
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

TEST_P(SyncSetupHandlerTest, ShowSetupEncryptAll) {
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

// Tests that trying to log in with an invalid username results in an error
// displayed to the user.
TEST_P(SyncSetupHandlerTest, SubmitAuthWithInvalidUsername) {
  EXPECT_CALL(*mock_signin_, IsAllowedUsername(_)).
      WillRepeatedly(Return(false));

  // Generate a blob of json that matches what would be submitted by the login
  // javascript code.
  DictionaryValue args;
  args.SetString("user", "user@not_allowed.com");
  args.SetString("pass", "password");
  args.SetString("captcha", "");
  args.SetString("otp", "");
  args.SetString("accessCode", "");
  std::string json;
  base::JSONWriter::Write(&args, &json);
  ListValue list_args;
  list_args.Append(new StringValue(json));

  // Mimic a login attempt from the UI.
  handler_->HandleSubmitAuth(&list_args);

  // Should result in the login page being displayed again.
  ASSERT_EQ(1U, web_ui_.call_data().size());
  const TestWebUI::CallData& data = web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name);
  std::string page;
  ASSERT_TRUE(data.arg1->GetAsString(&page));
  EXPECT_EQ(page, "login");

  // Also make sure that the appropriate error message is being passed.
  DictionaryValue* dictionary;
  ASSERT_TRUE(data.arg2->GetAsDictionary(&dictionary));
  std::string err = l10n_util::GetStringUTF8(IDS_SYNC_LOGIN_NAME_PROHIBITED);
  CheckShowSyncSetupArgs(
      dictionary, err, false, GoogleServiceAuthError::NONE, "", true, "");
  handler_->CloseSyncSetup();
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}

INSTANTIATE_TEST_CASE_P(SyncSetupHandlerTestWithParam,
                        SyncSetupHandlerTest,
                        Values(true, false));
