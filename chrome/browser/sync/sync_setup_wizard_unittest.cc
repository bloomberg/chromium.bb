// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_setup_wizard.h"

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/testing_pref_store.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/options/options_sync_setup_handler.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

static const char kTestUser[] = "chrome.p13n.test@gmail.com";
static const char kTestPassword[] = "passwd";
static const char kTestCaptcha[] = "pizzamyheart";
static const char kTestCaptchaUrl[] = "http://pizzamyheart/";

typedef GoogleServiceAuthError AuthError;

class MockSyncSetupHandler : public OptionsSyncSetupHandler {
 public:
  MockSyncSetupHandler() : OptionsSyncSetupHandler(NULL) {}

  // SyncSetupFlowHandler implementation.
  virtual void ShowGaiaLogin(const DictionaryValue& args) OVERRIDE {}
  virtual void ShowGaiaSuccessAndClose() OVERRIDE {
    flow()->OnDialogClosed("");
  }
  virtual void ShowGaiaSuccessAndSettingUp() OVERRIDE {}
  virtual void ShowConfigure(const DictionaryValue& args) OVERRIDE {}
  virtual void ShowPassphraseEntry(const DictionaryValue& args) OVERRIDE {}
  virtual void ShowSettingUp() OVERRIDE {}
  virtual void ShowSetupDone(const string16& user) OVERRIDE {
    flow()->OnDialogClosed("");
  }

  void CloseSetupUI() {
    ShowSetupDone(string16());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSyncSetupHandler);
};

class SigninManagerMock : public FakeSigninManager {
 public:
  SigninManagerMock() : auth_error_(AuthError::None()) {}

  virtual void StartSignIn(const std::string& username,
                           const std::string& password,
                           const std::string& token,
                           const std::string& captcha) OVERRIDE {
    FakeSigninManager::StartSignIn(username, password, token, captcha);
    username_ = username;
    password_ = password;
    captcha_ = captcha;
  }

  virtual const AuthError& GetLoginAuthError() const OVERRIDE {
    return auth_error_;
  }

  void ResetTestStats() {
    username_.clear();
    password_.clear();
    captcha_.clear();
  }

  AuthError auth_error_;
  std::string username_;
  std::string password_;
  std::string captcha_;
};

// A PSS subtype to inject.
class ProfileSyncServiceForWizardTest : public ProfileSyncService {
 public:
  ProfileSyncServiceForWizardTest(ProfileSyncComponentsFactory* factory,
                                  Profile* profile,
                                  ProfileSyncService::StartBehavior behavior)
      : ProfileSyncService(factory, profile, NULL, behavior),
        user_cancelled_dialog_(false),
        is_using_secondary_passphrase_(false),
        encrypt_everything_(false) {
    signin_ = &mock_signin_;
    ResetTestStats();
  }

  virtual ~ProfileSyncServiceForWizardTest() {}

  virtual void OnUserChoseDatatypes(
      bool sync_everything, syncable::ModelTypeSet chosen_types) OVERRIDE {
    user_chose_data_types_ = true;
    chosen_data_types_ = chosen_types;
  }

  virtual void OnUserCancelledDialog() OVERRIDE {
    user_cancelled_dialog_ = true;
  }

  virtual void SetPassphrase(const std::string& passphrase,
                             bool is_explicit) OVERRIDE {
    passphrase_ = passphrase;
  }

  virtual bool IsUsingSecondaryPassphrase() const OVERRIDE {
    return is_using_secondary_passphrase_;
  }

  void set_last_auth_error(const AuthError& error) {
    // Set the cached auth error in the SigninManager and ProfileSyncService.
    last_auth_error_ = error;
    mock_signin_.auth_error_ = error;
  }

  void set_is_using_secondary_passphrase(bool secondary) {
    is_using_secondary_passphrase_ = secondary;
  }

  void set_passphrase_required_reason(
      sync_api::PassphraseRequiredReason reason) {
    passphrase_required_reason_ = reason;
  }

  virtual bool sync_initialized() const OVERRIDE {
    return true;
  }

  virtual bool EncryptEverythingEnabled() const OVERRIDE {
    return encrypt_everything_;
  }

  void set_encrypt_everything(bool encrypt_everything) {
    encrypt_everything_ = encrypt_everything;
  }

  void ResetTestStats() {
    mock_signin_.ResetTestStats();
    user_cancelled_dialog_ = false;
    user_chose_data_types_ = false;
    keep_everything_synced_ = false;
    chosen_data_types_.Clear();
  }

  virtual void ShowSyncSetup(const std::string& sub_page) OVERRIDE {
    // Do Nothing.
  }

  void ClearObservers() {
    observers_.Clear();
  }

  SyncSetupWizard* GetWizard() {
    return &wizard_;
  }

  SigninManagerMock mock_signin_;
  bool user_cancelled_dialog_;
  bool user_chose_data_types_;
  bool keep_everything_synced_;
  bool is_using_secondary_passphrase_;
  bool encrypt_everything_;
  syncable::ModelTypeSet chosen_data_types_;
  std::string passphrase_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceForWizardTest);
};

class TestingProfileWithSyncService : public TestingProfile {
 public:
  explicit TestingProfileWithSyncService(
      ProfileSyncService::StartBehavior behavior) {
    sync_service_.reset(new ProfileSyncServiceForWizardTest(
        &factory_, this, behavior));
  }

  virtual ProfileSyncService* GetProfileSyncService() {
    return sync_service_.get();
  }
 private:
  ProfileSyncComponentsFactoryMock factory_;
  scoped_ptr<ProfileSyncService> sync_service_;
};

// TODO(jhawkins): Subclass Browser (specifically, ShowOptionsTab) and inject it
// here to test the visibility of the Sync UI.
class SyncSetupWizardTest : public BrowserWithTestWindowTest {
 public:
  SyncSetupWizardTest()
      : wizard_(NULL),
        service_(NULL),
        flow_(NULL) {}
  virtual ~SyncSetupWizardTest() {}
  virtual TestingProfile* BuildProfile() {
    return new TestingProfileWithSyncService(
        ProfileSyncService::MANUAL_START);
  }
  virtual void SetUp() {
    set_profile(BuildProfile());
    profile()->CreateBookmarkModel(false);
    // Wait for the bookmarks model to load.
    profile()->BlockUntilBookmarkModelLoaded();
    PrefService* prefs = profile()->GetPrefs();
    prefs->SetString(prefs::kGoogleServicesUsername, kTestUser);

    set_browser(new Browser(Browser::TYPE_TABBED, profile()));
    browser()->SetWindowForTesting(window());
    BrowserList::SetLastActive(browser());
    service_ = static_cast<ProfileSyncServiceForWizardTest*>(
        profile()->GetProfileSyncService());
    wizard_ = service_->GetWizard();
  }

  virtual void TearDown() {
    wizard_ = NULL;
    service_ = NULL;
    flow_ = NULL;
  }

 protected:
  void AttachSyncSetupHandler() {
    flow_ = wizard_->AttachSyncSetupHandler(&handler_);
  }

  void CompleteSetup() {
    // For a discrete run, we need to have ran through setup once.
    wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
    AttachSyncSetupHandler();
    wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
    wizard_->Step(SyncSetupWizard::SYNC_EVERYTHING);
    wizard_->Step(SyncSetupWizard::SETTING_UP);
    wizard_->Step(SyncSetupWizard::DONE);
  }

  void CloseSetupUI() {
    handler_.CloseSetupUI();
  }

  // This pointer is owned by the |Service_|.
  SyncSetupWizard* wizard_;
  ProfileSyncServiceForWizardTest* service_;
  SyncSetupFlow* flow_;
  MockSyncSetupHandler handler_;
};

TEST_F(SyncSetupWizardTest, InitialStepLogin) {
  ListValue credentials;
  std::string auth = "{\"user\":\"";
  auth += std::string(kTestUser) + "\",\"pass\":\"";
  auth += std::string(kTestPassword) + "\",\"captcha\":\"";
  auth += std::string(kTestCaptcha) + "\",\"access_code\":\"";
  auth += std::string() + "\"}";
  credentials.Append(new StringValue(auth));

  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_EQ(static_cast<SyncSetupFlow*>(NULL), flow_);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  AttachSyncSetupHandler();

  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, flow_->end_state_);

  // Simulate the user submitting credentials.
  handler_.HandleSubmitAuth(&credentials);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  EXPECT_EQ(kTestUser, service_->mock_signin_.username_);
  EXPECT_EQ(kTestPassword, service_->mock_signin_.password_);
  EXPECT_EQ(kTestCaptcha, service_->mock_signin_.captcha_);
  EXPECT_FALSE(service_->user_cancelled_dialog_);
  service_->ResetTestStats();

  // Simulate failed credentials.
  AuthError invalid_gaia(AuthError::INVALID_GAIA_CREDENTIALS);
  flow_->last_attempted_user_email_ = kTestUser;
  service_->set_last_auth_error(invalid_gaia);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  DictionaryValue dialog_args;
  flow_->GetArgsForGaiaLogin(&dialog_args);
  EXPECT_EQ(4U, dialog_args.size());
  std::string actual_user;
  dialog_args.GetString("user", &actual_user);
  EXPECT_EQ(kTestUser, actual_user);
  int error = -1;
  dialog_args.GetInteger("error", &error);
  EXPECT_EQ(static_cast<int>(AuthError::INVALID_GAIA_CREDENTIALS), error);
  flow_->last_attempted_user_email_ = kTestUser;
  service_->set_last_auth_error(AuthError::None());

  // Simulate captcha.
  AuthError captcha_error(AuthError::FromCaptchaChallenge(
      std::string(), GURL(kTestCaptchaUrl), GURL()));
  flow_->last_attempted_user_email_ = kTestUser;
  service_->set_last_auth_error(captcha_error);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  flow_->GetArgsForGaiaLogin(&dialog_args);
  EXPECT_EQ(4U, dialog_args.size());
  std::string captcha_url;
  dialog_args.GetString("captchaUrl", &captcha_url);
  EXPECT_EQ(kTestCaptchaUrl, GURL(captcha_url).spec());
  error = -1;
  dialog_args.GetInteger("error", &error);
  EXPECT_EQ(static_cast<int>(AuthError::CAPTCHA_REQUIRED), error);
  flow_->last_attempted_user_email_ = kTestUser;
  service_->set_last_auth_error(AuthError::None());

  // Simulate success.
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_TRUE(wizard_->IsVisible());
  wizard_->Step(SyncSetupWizard::SYNC_EVERYTHING);
  EXPECT_EQ(SyncSetupWizard::SYNC_EVERYTHING, flow_->current_state_);

  // That's all we're testing here, just move on to DONE.  We'll test the
  // "choose data types" scenarios elsewhere.
  wizard_->Step(SyncSetupWizard::SETTING_UP);  // No merge and sync.
  wizard_->Step(SyncSetupWizard::DONE);  // No merge and sync.
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, ChooseDataTypesSetsPrefs) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  AttachSyncSetupHandler();
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::CONFIGURE);

  ListValue data_type_choices_value;
  std::string data_type_choices =
      "{\"syncAllDataTypes\":false,\"syncBookmarks\":true,"
      "\"syncPreferences\":true,\"syncThemes\":false,\"syncPasswords\":false,"
      "\"syncAutofill\":false,\"syncExtensions\":false,\"syncTypedUrls\":true,"
      "\"syncApps\":true,\"syncSessions\":false,\"usePassphrase\":false,"
      "\"encryptAllData\":false}";
  data_type_choices_value.Append(new StringValue(data_type_choices));

  // Simulate the user choosing data types; bookmarks, prefs, typed URLS, and
  // apps are on, the rest are off.
  handler_.HandleConfigure(&data_type_choices_value);
  // Since we don't need a passphrase, wizard should have transitioned to
  // DONE state and closed the UI.
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_FALSE(service_->keep_everything_synced_);
  EXPECT_TRUE(service_->chosen_data_types_.Has(syncable::BOOKMARKS));
  EXPECT_TRUE(service_->chosen_data_types_.Has(syncable::PREFERENCES));
  EXPECT_FALSE(service_->chosen_data_types_.Has(syncable::THEMES));
  EXPECT_FALSE(service_->chosen_data_types_.Has(syncable::PASSWORDS));
  EXPECT_FALSE(service_->chosen_data_types_.Has(syncable::AUTOFILL));
  EXPECT_FALSE(service_->chosen_data_types_.Has(syncable::EXTENSIONS));
  EXPECT_TRUE(service_->chosen_data_types_.Has(syncable::TYPED_URLS));
  EXPECT_TRUE(service_->chosen_data_types_.Has(syncable::APPS));
  EXPECT_FALSE(service_->chosen_data_types_.Has(
      syncable::APP_NOTIFICATIONS));
}

TEST_F(SyncSetupWizardTest, ShowErrorUIForPasswordTest) {
  service_->ClearObservers();
  CompleteSetup();

  // Simulate an auth error and make sure the start and end state are set
  // right.
  service_->set_last_auth_error(
      AuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  service_->ShowErrorUI();
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  EXPECT_EQ(SyncSetupWizard::GAIA_SUCCESS, flow_->end_state_);
  ASSERT_TRUE(wizard_->IsVisible());

  // Make sure the wizard is dismissed.
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  ASSERT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, ShowErrorUIForPassphraseTest) {
  service_->ClearObservers();
  CompleteSetup();

  // Simulate a passphrase error and make sure the start and end state are set
  // right and wizard is shown.
  service_->set_passphrase_required_reason(sync_api::REASON_ENCRYPTION);
  service_->set_is_using_secondary_passphrase(true);
  service_->ShowErrorUI();
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::ENTER_PASSPHRASE, flow_->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, flow_->end_state_);
  ASSERT_TRUE(wizard_->IsVisible());

  // Make sure the wizard is dismissed.
  wizard_->Step(SyncSetupWizard::DONE);
  ASSERT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, EnterPassphraseRequired) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  AttachSyncSetupHandler();
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::CONFIGURE);
  wizard_->Step(SyncSetupWizard::SETTING_UP);
  service_->set_passphrase_required_reason(sync_api::REASON_ENCRYPTION);
  wizard_->Step(SyncSetupWizard::ENTER_PASSPHRASE);
  EXPECT_EQ(SyncSetupWizard::ENTER_PASSPHRASE, flow_->current_state_);

  ListValue value;
  value.Append(new StringValue("{\"passphrase\":\"myPassphrase\","
                                "\"mode\":\"gaia\"}"));
  handler_.HandlePassphraseEntry(&value);
  EXPECT_EQ("myPassphrase", service_->passphrase_);
  CloseSetupUI();
}

TEST_F(SyncSetupWizardTest, DialogCancelled) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  AttachSyncSetupHandler();
  // Simulate the user closing the dialog.
  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->user_cancelled_dialog_);
  EXPECT_EQ(std::string(), service_->mock_signin_.username_);
  EXPECT_EQ(std::string(), service_->mock_signin_.password_);

  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  AttachSyncSetupHandler();
  EXPECT_TRUE(wizard_->IsVisible());
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);

  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->user_cancelled_dialog_);
  EXPECT_EQ(std::string(), service_->mock_signin_.username_);
  EXPECT_EQ(std::string(), service_->mock_signin_.password_);
}

TEST_F(SyncSetupWizardTest, InvalidTransitions) {
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_FALSE(wizard_->IsVisible());

  wizard_->Step(SyncSetupWizard::DONE);
  EXPECT_FALSE(wizard_->IsVisible());

  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  AttachSyncSetupHandler();

  wizard_->Step(SyncSetupWizard::DONE);
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::SYNC_EVERYTHING);
  EXPECT_EQ(SyncSetupWizard::SYNC_EVERYTHING, flow_->current_state_);

  wizard_->Step(SyncSetupWizard::FATAL_ERROR);
  // Stepping to FATAL_ERROR sends us back to GAIA_LOGIN.
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  CloseSetupUI();
}

TEST_F(SyncSetupWizardTest, FullSuccessfulRunSetsPref) {
  CompleteSetup();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->profile()->GetPrefs()->GetBoolean(
      prefs::kSyncHasSetupCompleted));
}

TEST_F(SyncSetupWizardTest, AbortedByPendingClear) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  AttachSyncSetupHandler();
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::SYNC_EVERYTHING);
  wizard_->Step(SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR);
  // Stepping to SETUP_ABORTED should redirect us to GAIA_LOGIN state, since
  // that's where we display the error.
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, DiscreteRunChooseDataTypes) {
  CompleteSetup();

  wizard_->Step(SyncSetupWizard::CONFIGURE);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::DONE, flow_->end_state_);

  wizard_->Step(SyncSetupWizard::SETTING_UP);
  wizard_->Step(SyncSetupWizard::DONE);
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, DiscreteRunChooseDataTypesAbortedByPendingClear) {
  CompleteSetup();

  wizard_->Step(SyncSetupWizard::CONFIGURE);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::DONE, flow_->end_state_);
  wizard_->Step(SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR);
  // Stepping to SETUP_ABORTED should redirect us to GAIA_LOGIN state, since
  // that's where we display the error.
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);

  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, DiscreteRunGaiaLogin) {
  CompleteSetup();

  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::GAIA_SUCCESS, flow_->end_state_);

  AuthError invalid_gaia(AuthError::INVALID_GAIA_CREDENTIALS);
  flow_->last_attempted_user_email_ = kTestUser;
  service_->set_last_auth_error(invalid_gaia);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_TRUE(wizard_->IsVisible());

  DictionaryValue dialog_args;
  flow_->GetArgsForGaiaLogin(&dialog_args);
  EXPECT_EQ(4U, dialog_args.size());
  std::string actual_user;
  dialog_args.GetString("user", &actual_user);
  EXPECT_EQ(kTestUser, actual_user);
  int error = -1;
  dialog_args.GetInteger("error", &error);
  EXPECT_EQ(static_cast<int>(AuthError::INVALID_GAIA_CREDENTIALS), error);
  flow_->last_attempted_user_email_ = kTestUser;
  service_->set_last_auth_error(AuthError::None());

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
}

TEST_F(SyncSetupWizardTest, NonFatalError) {
  CompleteSetup();

  // Set up the ENTER_PASSPHRASE case.
  service_->set_passphrase_required_reason(sync_api::REASON_ENCRYPTION);
  service_->set_is_using_secondary_passphrase(true);
  wizard_->Step(SyncSetupWizard::NONFATAL_ERROR);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::ENTER_PASSPHRASE, flow_->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, flow_->end_state_);
  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());

  // Reset.
  service_->set_passphrase_required_reason(
      sync_api::REASON_PASSPHRASE_NOT_REQUIRED);
  service_->set_is_using_secondary_passphrase(false);

  // Test the various auth error states that lead to GAIA_LOGIN.

  service_->set_last_auth_error(
      AuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  wizard_->Step(SyncSetupWizard::NONFATAL_ERROR);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, flow_->end_state_);
  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());

  service_->set_last_auth_error(
      AuthError(GoogleServiceAuthError::CAPTCHA_REQUIRED));
  wizard_->Step(SyncSetupWizard::NONFATAL_ERROR);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, flow_->end_state_);
  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());

  service_->set_last_auth_error(
      AuthError(GoogleServiceAuthError::ACCOUNT_DELETED));
  wizard_->Step(SyncSetupWizard::NONFATAL_ERROR);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, flow_->end_state_);
  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());

  service_->set_last_auth_error(
      AuthError(GoogleServiceAuthError::ACCOUNT_DISABLED));
  wizard_->Step(SyncSetupWizard::NONFATAL_ERROR);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, flow_->end_state_);
  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());

  service_->set_last_auth_error(
      AuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  wizard_->Step(SyncSetupWizard::NONFATAL_ERROR);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, flow_->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, flow_->end_state_);
  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());
}

class SyncSetupWizardCrosTest : public SyncSetupWizardTest {
 public:
  virtual TestingProfile* BuildProfile() {
    TestingProfile* profile =
        new TestingProfileWithSyncService(ProfileSyncService::AUTO_START);
    profile->GetProfileSyncService()->signin()->SetAuthenticatedUsername(
        kTestUser);
    return profile;
  }
};

// Tests a scenario where sync is disabled on chrome os on startup due to
// an auth error (application specific password is needed).
TEST_F(SyncSetupWizardCrosTest, CrosAuthSetup) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);

  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::GAIA_SUCCESS, flow_->end_state_);

  DictionaryValue dialog_args;
  flow_->GetArgsForGaiaLogin(&dialog_args);
  EXPECT_EQ(4U, dialog_args.size());
  std::string actual_user;
  dialog_args.GetString("user", &actual_user);
  EXPECT_EQ(kTestUser, actual_user);
  int error = -1;
  dialog_args.GetInteger("error", &error);
  EXPECT_EQ(0, error);
  bool editable = true;
  dialog_args.GetBoolean("editable_user", &editable);
  EXPECT_FALSE(editable);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_TRUE(service_->user_cancelled_dialog_);
  EXPECT_TRUE(service_->profile()->GetPrefs()->GetBoolean(
      prefs::kSyncHasSetupCompleted));
}
