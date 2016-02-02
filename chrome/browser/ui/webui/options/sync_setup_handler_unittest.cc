// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/sync_setup_handler.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/fake_auth_status_provider.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/layout.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Values;

typedef GoogleServiceAuthError AuthError;

namespace {

MATCHER_P(ModelTypeSetMatches, value, "") { return arg.Equals(value); }

const char kTestUser[] = "chrome.p13n.test@gmail.com";

// Returns a ModelTypeSet with all user selectable types set.
syncer::ModelTypeSet GetAllTypes() {
  return syncer::UserSelectableTypes();
}

enum SyncAllDataConfig {
  SYNC_ALL_DATA,
  CHOOSE_WHAT_TO_SYNC,
  SYNC_NOTHING
};

enum EncryptAllConfig {
  ENCRYPT_ALL_DATA,
  ENCRYPT_PASSWORDS
};

enum PaymentsIntegrationConfig {
  PAYMENTS_INTEGRATION_ENABLED,
  PAYMENTS_INTEGRATION_DISABLED
};

// Create a json-format string with the key/value pairs appropriate for a call
// to HandleConfigure(). If |extra_values| is non-null, then the values from
// the passed dictionary are added to the json.
std::string GetConfiguration(const base::DictionaryValue* extra_values,
                             SyncAllDataConfig sync_all,
                             syncer::ModelTypeSet types,
                             const std::string& passphrase,
                             EncryptAllConfig encrypt_all,
                             PaymentsIntegrationConfig payments_integration) {
  base::DictionaryValue result;
  if (extra_values)
    result.MergeDictionary(extra_values);
  result.SetBoolean("syncAllDataTypes", sync_all == SYNC_ALL_DATA);
  result.SetBoolean("syncNothing", sync_all == SYNC_NOTHING);
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
  result.SetBoolean("tabsSynced", types.Has(syncer::PROXY_TABS));
  result.SetBoolean("themesSynced", types.Has(syncer::THEMES));
  result.SetBoolean("typedUrlsSynced", types.Has(syncer::TYPED_URLS));
  result.SetBoolean("wifiCredentialsSynced",
                    types.Has(syncer::WIFI_CREDENTIALS));
  result.SetBoolean("paymentsIntegrationEnabled",
                    payments_integration == PAYMENTS_INTEGRATION_ENABLED);
  std::string args;
  base::JSONWriter::Write(result, &args);
  return args;
}

// Checks whether the passed |dictionary| contains a |key| with the given
// |expected_value|. If |omit_if_false| is true, then the value should only
// be present if |expected_value| is true.
void CheckBool(const base::DictionaryValue* dictionary,
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
    EXPECT_EQ(expected_value, actual_value) <<
        "Mismatch found for " << key;
  }
}

void CheckBool(const base::DictionaryValue* dictionary,
               const std::string& key,
               bool expected_value) {
  return CheckBool(dictionary, key, expected_value, false);
}

// Checks to make sure that the values stored in |dictionary| match the values
// expected by the showSyncSetupPage() JS function for a given set of data
// types.
void CheckConfigDataTypeArguments(const base::DictionaryValue* dictionary,
                                  SyncAllDataConfig config,
                                  syncer::ModelTypeSet types) {
  CheckBool(dictionary, "syncAllDataTypes", config == SYNC_ALL_DATA);
  CheckBool(dictionary, "syncNothing", config == SYNC_NOTHING);
  CheckBool(dictionary, "appsSynced", types.Has(syncer::APPS));
  CheckBool(dictionary, "autofillSynced", types.Has(syncer::AUTOFILL));
  CheckBool(dictionary, "bookmarksSynced", types.Has(syncer::BOOKMARKS));
  CheckBool(dictionary, "extensionsSynced", types.Has(syncer::EXTENSIONS));
  CheckBool(dictionary, "passwordsSynced", types.Has(syncer::PASSWORDS));
  CheckBool(dictionary, "preferencesSynced", types.Has(syncer::PREFERENCES));
  CheckBool(dictionary, "tabsSynced", types.Has(syncer::PROXY_TABS));
  CheckBool(dictionary, "themesSynced", types.Has(syncer::THEMES));
  CheckBool(dictionary, "typedUrlsSynced", types.Has(syncer::TYPED_URLS));
  CheckBool(dictionary, "wifiCredentialsSynced",
            types.Has(syncer::WIFI_CREDENTIALS));
}


}  // namespace

class TestingSyncSetupHandler : public SyncSetupHandler {
 public:
  TestingSyncSetupHandler(content::WebUI* web_ui, Profile* profile)
      : profile_(profile) {
    set_web_ui(web_ui);
  }
  ~TestingSyncSetupHandler() override { set_web_ui(NULL); }

  void FocusUI() override {}

  Profile* GetProfile() const override { return profile_; }

  using SyncSetupHandler::is_configuring_sync;

 private:
#if !defined(OS_CHROMEOS)
  void DisplayGaiaLoginInNewTabOrWindow(
      signin_metrics::AccessPoint access_point) override {}
#endif

  // Weak pointer to parent profile.
  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(TestingSyncSetupHandler);
};

// The boolean parameter indicates whether the test is run with ClientOAuth
// or not.  The test parameter is a bool: whether or not to test with/
// /ClientLogin enabled or not.
class SyncSetupHandlerTest : public testing::Test {
 public:
  SyncSetupHandlerTest() : error_(GoogleServiceAuthError::NONE) {}
  void SetUp() override {
    error_ = GoogleServiceAuthError::AuthErrorNone();

    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              BuildFakeSigninManagerBase);
    profile_ = builder.Build();

    // Sign in the user.
    mock_signin_ = static_cast<SigninManagerBase*>(
        SigninManagerFactory::GetForProfile(profile_.get()));
    std::string username = GetTestUser();
    if (!username.empty())
      mock_signin_->SetAuthenticatedAccountInfo(username, username);

    mock_pss_ = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), BuildMockProfileSyncService));
    EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error_));
    ON_CALL(*mock_pss_, GetPassphraseType()).WillByDefault(
        Return(syncer::IMPLICIT_PASSPHRASE));
    ON_CALL(*mock_pss_, GetExplicitPassphraseTime()).WillByDefault(
        Return(base::Time()));
    ON_CALL(*mock_pss_, GetRegisteredDataTypes())
        .WillByDefault(Return(syncer::ModelTypeSet()));

    mock_pss_->Initialize();

    handler_.reset(new TestingSyncSetupHandler(&web_ui_, profile_.get()));
  }

  // Setup the expectations for calls made when displaying the config page.
  void SetDefaultExpectationsForConfigPage() {
    EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_pss_, GetRegisteredDataTypes())
        .WillRepeatedly(Return(GetAllTypes()));
    EXPECT_CALL(*mock_pss_, GetPreferredDataTypes())
        .WillRepeatedly(Return(GetAllTypes()));
    EXPECT_CALL(*mock_pss_, GetActiveDataTypes())
        .WillRepeatedly(Return(GetAllTypes()));
    EXPECT_CALL(*mock_pss_, IsEncryptEverythingAllowed())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_pss_, IsEncryptEverythingEnabled())
        .WillRepeatedly(Return(false));
  }

  void SetupInitializedProfileSyncService() {
    // An initialized ProfileSyncService will have already completed sync setup
    // and will have an initialized sync backend.
    ASSERT_TRUE(mock_signin_->IsInitialized());
    EXPECT_CALL(*mock_pss_, IsBackendInitialized())
        .WillRepeatedly(Return(true));
  }

  void ExpectConfig() {
    ASSERT_EQ(1U, web_ui_.call_data().size());
    const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
    EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name());
    std::string page;
    ASSERT_TRUE(data.arg1()->GetAsString(&page));
    EXPECT_EQ(page, "configure");
  }

  void ExpectDone() {
    ASSERT_EQ(1U, web_ui_.call_data().size());
    const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
    EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name());
    std::string page;
    ASSERT_TRUE(data.arg1()->GetAsString(&page));
    EXPECT_EQ(page, "done");
  }

  void ExpectSpinnerAndClose() {
    // We expect a call to SyncSetupOverlay.showSyncSetupPage.
    EXPECT_EQ(1U, web_ui_.call_data().size());
    const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
    EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name());

    std::string page;
    ASSERT_TRUE(data.arg1()->GetAsString(&page));
    EXPECT_EQ(page, "spinner");
    // Cancelling the spinner dialog will cause CloseSyncSetup().
    handler_->CloseSyncSetup();
    EXPECT_EQ(NULL,
              LoginUIServiceFactory::GetForProfile(
                  profile_.get())->current_login_ui());
  }

  // It's difficult to notify sync listeners when using a ProfileSyncServiceMock
  // so this helper routine dispatches an OnStateChanged() notification to the
  // SyncStartupTracker.
  void NotifySyncListeners() {
    if (handler_->sync_startup_tracker_)
      handler_->sync_startup_tracker_->OnStateChanged();
  }

  virtual std::string GetTestUser() {
    return std::string(kTestUser);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<Profile> profile_;
  ProfileSyncServiceMock* mock_pss_;
  GoogleServiceAuthError error_;
  SigninManagerBase* mock_signin_;
  content::TestWebUI web_ui_;
  scoped_ptr<TestingSyncSetupHandler> handler_;
};

class SyncSetupHandlerFirstSigninTest : public SyncSetupHandlerTest {
  std::string GetTestUser() override { return std::string(); }
};

TEST_F(SyncSetupHandlerTest, Basic) {
}

#if !defined(OS_CHROMEOS)
TEST_F(SyncSetupHandlerFirstSigninTest, DisplayBasicLogin) {
  EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsFirstSetupComplete()).WillRepeatedly(Return(false));
  // Ensure that the user is not signed in before calling |HandleStartSignin()|.
  SigninManager* manager = static_cast<SigninManager*>(mock_signin_);
  manager->SignOut(signin_metrics::SIGNOUT_TEST,
                   signin_metrics::SignoutDelete::IGNORE_METRIC);
  handler_->HandleStartSignin(NULL);

  // Sync setup hands off control to the gaia login tab.
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());

  ASSERT_FALSE(handler_->is_configuring_sync());

  handler_->CloseSyncSetup();
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}

TEST_F(SyncSetupHandlerTest, ShowSyncSetupWhenNotSignedIn) {
  EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsFirstSetupComplete()).WillRepeatedly(Return(false));
  handler_->HandleShowSetupUI(NULL);

  // We expect a call to SyncSetupOverlay.showSyncSetupPage.
  ASSERT_EQ(1U, web_ui_.call_data().size());
  const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name());

  ASSERT_FALSE(handler_->is_configuring_sync());
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}
#endif  // !defined(OS_CHROMEOS)

// Verifies that the sync setup is terminated correctly when the
// sync is disabled.
TEST_F(SyncSetupHandlerTest, HandleSetupUIWhenSyncDisabled) {
  EXPECT_CALL(*mock_pss_, IsManaged()).WillRepeatedly(Return(true));
  handler_->HandleShowSetupUI(NULL);

  // Sync setup is closed when sync is disabled.
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
  ASSERT_FALSE(handler_->is_configuring_sync());
}

// Verifies that the handler correctly handles a cancellation when
// it is displaying the spinner to the user.
TEST_F(SyncSetupHandlerTest, DisplayConfigureWithBackendDisabledAndCancel) {
  EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsFirstSetupComplete()).WillRepeatedly(Return(false));
  error_ = GoogleServiceAuthError::AuthErrorNone();
  EXPECT_CALL(*mock_pss_, IsBackendInitialized()).WillRepeatedly(Return(false));

  // We're simulating a user setting up sync, which would cause the backend to
  // kick off initialization, but not download user data types. The sync
  // backend will try to download control data types (e.g encryption info), but
  // that won't finish for this test as we're simulating cancelling while the
  // spinner is showing.
  handler_->HandleShowSetupUI(NULL);

  EXPECT_EQ(handler_.get(),
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());

  ExpectSpinnerAndClose();
}

// Verifies that the handler correctly transitions from showing the spinner
// to showing a configuration page when sync setup completes successfully.
TEST_F(SyncSetupHandlerTest,
       DisplayConfigureWithBackendDisabledAndSyncStartupCompleted) {
  EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsFirstSetupComplete()).WillRepeatedly(Return(false));
  error_ = GoogleServiceAuthError::AuthErrorNone();
  // Sync backend is stopped initially, and will start up.
  EXPECT_CALL(*mock_pss_, IsBackendInitialized()).WillRepeatedly(Return(false));
  SetDefaultExpectationsForConfigPage();

  handler_->OpenSyncSetup(nullptr);

  // We expect a call to SyncSetupOverlay.showSyncSetupPage.
  EXPECT_EQ(1U, web_ui_.call_data().size());

  const content::TestWebUI::CallData& data0 = *web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data0.function_name());
  std::string page;
  ASSERT_TRUE(data0.arg1()->GetAsString(&page));
  EXPECT_EQ(page, "spinner");

  Mock::VerifyAndClearExpectations(mock_pss_);
  // Now, act as if the ProfileSyncService has started up.
  SetDefaultExpectationsForConfigPage();
  EXPECT_CALL(*mock_pss_, IsBackendInitialized()).WillRepeatedly(Return(true));
  error_ = GoogleServiceAuthError::AuthErrorNone();
  EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error_));
  NotifySyncListeners();

  // We expect a second call to SyncSetupOverlay.showSyncSetupPage.
  EXPECT_EQ(2U, web_ui_.call_data().size());
  const content::TestWebUI::CallData& data1 = *web_ui_.call_data().back();
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data1.function_name());
  ASSERT_TRUE(data1.arg1()->GetAsString(&page));
  EXPECT_EQ(page, "configure");
  const base::DictionaryValue* dictionary = nullptr;
  ASSERT_TRUE(data1.arg2()->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "passphraseFailed", false);
  CheckBool(dictionary, "syncAllDataTypes", true);
  CheckBool(dictionary, "encryptAllDataAllowed", true);
  CheckBool(dictionary, "encryptAllData", false);
  CheckBool(dictionary, "usePassphrase", false);
}

// Verifies the case where the user cancels after the sync backend has
// initialized (meaning it already transitioned from the spinner to a proper
// configuration page, tested by
// DisplayConfigureWithBackendDisabledAndSigninSuccess), but before the user
// before the user has continued on.
TEST_F(SyncSetupHandlerTest,
       DisplayConfigureWithBackendDisabledAndCancelAfterSigninSuccess) {
  EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsFirstSetupComplete()).WillRepeatedly(Return(false));
  error_ = GoogleServiceAuthError::AuthErrorNone();
  EXPECT_CALL(*mock_pss_, IsBackendInitialized())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  SetDefaultExpectationsForConfigPage();
  handler_->OpenSyncSetup(nullptr);

  // It's important to tell sync the user cancelled the setup flow before we
  // tell it we're through with the setup progress.
  testing::InSequence seq;
  EXPECT_CALL(*mock_pss_, RequestStop(ProfileSyncService::CLEAR_DATA));
  EXPECT_CALL(*mock_pss_, SetSetupInProgress(false));

  handler_->CloseSyncSetup();
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}

TEST_F(SyncSetupHandlerTest,
       DisplayConfigureWithBackendDisabledAndSigninFailed) {
  EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsFirstSetupComplete()).WillRepeatedly(Return(false));
  error_ = GoogleServiceAuthError::AuthErrorNone();
  EXPECT_CALL(*mock_pss_, IsBackendInitialized()).WillRepeatedly(Return(false));

  handler_->OpenSyncSetup(nullptr);
  const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name());
  std::string page;
  ASSERT_TRUE(data.arg1()->GetAsString(&page));
  EXPECT_EQ(page, "spinner");
  Mock::VerifyAndClearExpectations(mock_pss_);
  error_ = GoogleServiceAuthError(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error_));
  NotifySyncListeners();

  // On failure, the dialog will be closed.
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());
}

#if !defined(OS_CHROMEOS)

class SyncSetupHandlerNonCrosTest : public SyncSetupHandlerTest {
 public:
  SyncSetupHandlerNonCrosTest() {}
};

TEST_F(SyncSetupHandlerNonCrosTest, HandleGaiaAuthFailure) {
  EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, HasUnrecoverableError())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsFirstSetupComplete()).WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(nullptr);

  ASSERT_FALSE(handler_->is_configuring_sync());
}

// TODO(kochi): We need equivalent tests for ChromeOS.
TEST_F(SyncSetupHandlerNonCrosTest, UnrecoverableErrorInitializingSync) {
  EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsFirstSetupComplete()).WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(nullptr);

  ASSERT_FALSE(handler_->is_configuring_sync());
}

TEST_F(SyncSetupHandlerNonCrosTest, GaiaErrorInitializingSync) {
  EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsFirstSetupComplete()).WillRepeatedly(Return(false));
  // Open the web UI.
  handler_->OpenSyncSetup(nullptr);

  ASSERT_FALSE(handler_->is_configuring_sync());
}

#endif  // #if !defined(OS_CHROMEOS)

TEST_F(SyncSetupHandlerTest, TestSyncEverything) {
  std::string args =
      GetConfiguration(NULL, SYNC_ALL_DATA, GetAllTypes(), std::string(),
                       ENCRYPT_PASSWORDS, PAYMENTS_INTEGRATION_ENABLED);
  base::ListValue list_args;
  list_args.Append(new base::StringValue(args));
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

TEST_F(SyncSetupHandlerTest, TestSyncNothing) {
  std::string args =
      GetConfiguration(NULL, SYNC_NOTHING, GetAllTypes(), std::string(),
                       ENCRYPT_PASSWORDS, PAYMENTS_INTEGRATION_ENABLED);
  base::ListValue list_args;
  list_args.Append(new base::StringValue(args));
  EXPECT_CALL(*mock_pss_, RequestStop(ProfileSyncService::CLEAR_DATA));
  SetupInitializedProfileSyncService();
  handler_->HandleConfigure(&list_args);

  // We expect a call to SyncSetupOverlay.showSyncSetupPage.
  ASSERT_EQ(1U, web_ui_.call_data().size());
  const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
  EXPECT_EQ("SyncSetupOverlay.showSyncSetupPage", data.function_name());
}

TEST_F(SyncSetupHandlerTest, TurnOnEncryptAll) {
  std::string args =
      GetConfiguration(NULL, SYNC_ALL_DATA, GetAllTypes(), std::string(),
                       ENCRYPT_ALL_DATA, PAYMENTS_INTEGRATION_ENABLED);
  base::ListValue list_args;
  list_args.Append(new base::StringValue(args));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsEncryptEverythingAllowed())
      .WillRepeatedly(Return(true));
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
      GetConfiguration(NULL, SYNC_ALL_DATA, GetAllTypes(), std::string(),
                       ENCRYPT_PASSWORDS, PAYMENTS_INTEGRATION_ENABLED);
  base::ListValue list_args;
  list_args.Append(new base::StringValue(args));
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
  base::DictionaryValue dict;
  dict.SetBoolean("isGooglePassphrase", true);
  std::string args =
      GetConfiguration(&dict, SYNC_ALL_DATA, GetAllTypes(), "gaiaPassphrase",
                       ENCRYPT_PASSWORDS, PAYMENTS_INTEGRATION_ENABLED);
  base::ListValue list_args;
  list_args.Append(new base::StringValue(args));
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
  base::DictionaryValue dict;
  dict.SetBoolean("isGooglePassphrase", false);
  std::string args =
      GetConfiguration(&dict, SYNC_ALL_DATA, GetAllTypes(), "custom_passphrase",
                       ENCRYPT_PASSWORDS, PAYMENTS_INTEGRATION_ENABLED);
  base::ListValue list_args;
  list_args.Append(new base::StringValue(args));
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
  base::DictionaryValue dict;
  dict.SetBoolean("isGooglePassphrase", true);
  std::string args = GetConfiguration(&dict, SYNC_ALL_DATA, GetAllTypes(),
                                      "invalid_passphrase", ENCRYPT_PASSWORDS,
                                      PAYMENTS_INTEGRATION_ENABLED);
  base::ListValue list_args;
  list_args.Append(new base::StringValue(args));
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
  const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
  const base::DictionaryValue* dictionary = nullptr;
  ASSERT_TRUE(data.arg2()->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "passphraseFailed", true);
}

// Walks through each user selectable type, and tries to sync just that single
// data type.
TEST_F(SyncSetupHandlerTest, TestSyncIndividualTypes) {
  syncer::ModelTypeSet user_selectable_types = GetAllTypes();
  syncer::ModelTypeSet::Iterator it;
  for (it = user_selectable_types.First(); it.Good(); it.Inc()) {
    syncer::ModelTypeSet type_to_set;
    type_to_set.Put(it.Get());
    std::string args =
        GetConfiguration(NULL, CHOOSE_WHAT_TO_SYNC, type_to_set, std::string(),
                         ENCRYPT_PASSWORDS, PAYMENTS_INTEGRATION_ENABLED);
    base::ListValue list_args;
    list_args.Append(new base::StringValue(args));
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
  std::string args =
      GetConfiguration(NULL, CHOOSE_WHAT_TO_SYNC, GetAllTypes(), std::string(),
                       ENCRYPT_PASSWORDS, PAYMENTS_INTEGRATION_ENABLED);
  base::ListValue list_args;
  list_args.Append(new base::StringValue(args));
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
  handler_->OpenSyncSetup(nullptr);

  ExpectConfig();
}

// We do not display signin on chromeos in the case of auth error.
TEST_F(SyncSetupHandlerTest, ShowSigninOnAuthError) {
  // Initialize the system to a signed in state, but with an auth error.
  error_ = GoogleServiceAuthError(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  SetupInitializedProfileSyncService();
  mock_signin_->SetAuthenticatedAccountInfo(kTestUser, kTestUser);
  FakeAuthStatusProvider provider(
      SigninErrorControllerFactory::GetForProfile(profile_.get()));
  provider.SetAuthError(kTestUser, error_);
  EXPECT_CALL(*mock_pss_, CanSyncStart()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsBackendInitialized()).WillRepeatedly(Return(false));

#if defined(OS_CHROMEOS)
  // On ChromeOS, auth errors are ignored - instead we just try to start the
  // sync backend (which will fail due to the auth error). This should only
  // happen if the user manually navigates to chrome://settings/syncSetup -
  // clicking on the button in the UI will sign the user out rather than
  // displaying a spinner. Should be no visible UI on ChromeOS in this case.
  EXPECT_EQ(NULL, LoginUIServiceFactory::GetForProfile(
      profile_.get())->current_login_ui());
#else

  // On ChromeOS, this should display the spinner while we try to startup the
  // sync backend, and on desktop this displays the login dialog.
  handler_->OpenSyncSetup(nullptr);

  // Sync setup is closed when re-auth is in progress.
  EXPECT_EQ(NULL,
            LoginUIServiceFactory::GetForProfile(
                profile_.get())->current_login_ui());

  ASSERT_FALSE(handler_->is_configuring_sync());
#endif
}

TEST_F(SyncSetupHandlerTest, ShowSetupSyncEverything) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  SetDefaultExpectationsForConfigPage();
  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(nullptr);

  ExpectConfig();
  const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
  const base::DictionaryValue* dictionary = nullptr;
  ASSERT_TRUE(data.arg2()->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "syncAllDataTypes", true);
  CheckBool(dictionary, "appsRegistered", true);
  CheckBool(dictionary, "autofillRegistered", true);
  CheckBool(dictionary, "bookmarksRegistered", true);
  CheckBool(dictionary, "extensionsRegistered", true);
  CheckBool(dictionary, "passwordsRegistered", true);
  CheckBool(dictionary, "preferencesRegistered", true);
  CheckBool(dictionary, "wifiCredentialsRegistered", true);
  CheckBool(dictionary, "tabsRegistered", true);
  CheckBool(dictionary, "themesRegistered", true);
  CheckBool(dictionary, "typedUrlsRegistered", true);
  CheckBool(dictionary, "paymentsIntegrationEnabled", true);
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
  sync_driver::SyncPrefs sync_prefs(profile_->GetPrefs());
  sync_prefs.SetKeepEverythingSynced(false);
  SetDefaultExpectationsForConfigPage();
  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(nullptr);

  ExpectConfig();
  const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
  const base::DictionaryValue* dictionary = nullptr;
  ASSERT_TRUE(data.arg2()->GetAsDictionary(&dictionary));
  CheckConfigDataTypeArguments(dictionary, CHOOSE_WHAT_TO_SYNC, GetAllTypes());
}

TEST_F(SyncSetupHandlerTest, ShowSetupSyncForAllTypesIndividually) {
  syncer::ModelTypeSet user_selectable_types = GetAllTypes();
  syncer::ModelTypeSet::Iterator it;
  for (it = user_selectable_types.First(); it.Good(); it.Inc()) {
    EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
        .WillRepeatedly(Return(false));
    SetupInitializedProfileSyncService();
    sync_driver::SyncPrefs sync_prefs(profile_->GetPrefs());
    sync_prefs.SetKeepEverythingSynced(false);
    SetDefaultExpectationsForConfigPage();
    syncer::ModelTypeSet types;
    types.Put(it.Get());
    EXPECT_CALL(*mock_pss_, GetPreferredDataTypes()).
        WillRepeatedly(Return(types));

    // This should display the sync setup dialog (not login).
    handler_->OpenSyncSetup(nullptr);

    ExpectConfig();
    // Close the config overlay.
    LoginUIServiceFactory::GetForProfile(profile_.get())->LoginUIClosed(
        handler_.get());
    const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
    const base::DictionaryValue* dictionary = nullptr;
    ASSERT_TRUE(data.arg2()->GetAsDictionary(&dictionary));
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
  handler_->OpenSyncSetup(nullptr);

  ExpectConfig();
  const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
  const base::DictionaryValue* dictionary = nullptr;
  ASSERT_TRUE(data.arg2()->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "showPassphrase", true);
  CheckBool(dictionary, "usePassphrase", false);
  CheckBool(dictionary, "passphraseFailed", false);
}

TEST_F(SyncSetupHandlerTest, ShowSetupCustomPassphraseRequired) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, GetPassphraseType())
      .WillRepeatedly(Return(syncer::CUSTOM_PASSPHRASE));
  SetupInitializedProfileSyncService();
  SetDefaultExpectationsForConfigPage();

  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(nullptr);

  ExpectConfig();
  const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
  const base::DictionaryValue* dictionary = nullptr;
  ASSERT_TRUE(data.arg2()->GetAsDictionary(&dictionary));
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
  EXPECT_CALL(*mock_pss_, IsEncryptEverythingEnabled())
      .WillRepeatedly(Return(true));

  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(nullptr);

  ExpectConfig();
  const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
  const base::DictionaryValue* dictionary = nullptr;
  ASSERT_TRUE(data.arg2()->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "encryptAllData", true);
}

TEST_F(SyncSetupHandlerTest, ShowSetupEncryptAllDisallowed) {
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  SetDefaultExpectationsForConfigPage();
  EXPECT_CALL(*mock_pss_, IsEncryptEverythingAllowed())
      .WillRepeatedly(Return(false));

  // This should display the sync setup dialog (not login).
  handler_->OpenSyncSetup(nullptr);

  ExpectConfig();
  const content::TestWebUI::CallData& data = *web_ui_.call_data()[0];
  const base::DictionaryValue* dictionary = nullptr;
  ASSERT_TRUE(data.arg2()->GetAsDictionary(&dictionary));
  CheckBool(dictionary, "encryptAllData", false);
  CheckBool(dictionary, "encryptAllDataAllowed", false);
}

TEST_F(SyncSetupHandlerTest, TurnOnEncryptAllDisallowed) {
  std::string args =
      GetConfiguration(NULL, SYNC_ALL_DATA, GetAllTypes(), std::string(),
                       ENCRYPT_ALL_DATA, PAYMENTS_INTEGRATION_ENABLED);
  base::ListValue list_args;
  list_args.Append(new base::StringValue(args));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequiredForDecryption())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsPassphraseRequired())
      .WillRepeatedly(Return(false));
  SetupInitializedProfileSyncService();
  EXPECT_CALL(*mock_pss_, IsEncryptEverythingAllowed())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, EnableEncryptEverything()).Times(0);
  EXPECT_CALL(*mock_pss_, OnUserChoseDatatypes(true, _));
  handler_->HandleConfigure(&list_args);

  // Ensure that we navigated to the "done" state since we don't need a
  // passphrase.
  ExpectDone();
}

