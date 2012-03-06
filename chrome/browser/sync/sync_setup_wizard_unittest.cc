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
#include "chrome/browser/sync/profile_sync_service_factory.h"
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

typedef GoogleServiceAuthError AuthError;

class MockSyncSetupHandler : public OptionsSyncSetupHandler {
 public:
  explicit MockSyncSetupHandler(Profile* profile)
      : OptionsSyncSetupHandler(NULL),
        profile_(profile) {}

  // SyncSetupFlowHandler implementation.
  virtual void ShowFatalError() OVERRIDE {
    ShowSetupDone(string16());
  }
  virtual void ShowConfigure(const DictionaryValue& args) OVERRIDE {}
  virtual void ShowPassphraseEntry(const DictionaryValue& args) OVERRIDE {}
  virtual void ShowSettingUp() OVERRIDE {}
  virtual void ShowSetupDone(const string16& user) OVERRIDE {
    if (flow())
      flow()->OnDialogClosed("");
  }

  SyncSetupFlow* GetFlow() {
    return flow();
  }

  void CloseSetupUI() {
    ShowSetupDone(string16());
  }

  virtual Profile* GetProfile() const OVERRIDE {
    return profile_;
  }

 private:
  // Weak ptr to injected parent profile.
  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(MockSyncSetupHandler);
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

// A PSS subtype to inject.
class ProfileSyncServiceForWizardTest : public ProfileSyncService {
 public:
  virtual ~ProfileSyncServiceForWizardTest() {}

  static ProfileKeyedBase* BuildManual(Profile* profile) {
    return new ProfileSyncServiceForWizardTest(profile,
        ProfileSyncService::MANUAL_START);
  }

  static ProfileKeyedBase* BuildAuto(Profile* profile) {
    return new ProfileSyncServiceForWizardTest(profile,
        ProfileSyncService::AUTO_START);
  }

  virtual void OnUserChoseDatatypes(
      bool sync_everything, syncable::ModelTypeSet chosen_types) OVERRIDE {
    user_chose_data_types_ = true;
    chosen_data_types_ = chosen_types;
  }

  virtual void OnUserCancelledDialog() OVERRIDE {
    user_cancelled_dialog_ = true;
  }

  virtual void SetPassphrase(const std::string& passphrase,
                             PassphraseType type,
                             PassphraseSource source) OVERRIDE {
    passphrase_ = passphrase;
  }

  virtual bool IsUsingSecondaryPassphrase() const OVERRIDE {
    return is_using_secondary_passphrase_;
  }

  void set_last_auth_error(const AuthError& error) {
    // Set the cached auth error in ProfileSyncService.
    last_auth_error_ = error;
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
  ProfileSyncServiceForWizardTest(Profile* profile,
                                  ProfileSyncService::StartBehavior behavior)
      : ProfileSyncService(NULL, profile, NULL, behavior),
        user_cancelled_dialog_(false),
        is_using_secondary_passphrase_(false),
        encrypt_everything_(false) {
    signin_ = &mock_signin_;
    ResetTestStats();
  }

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceForWizardTest);
};

// TODO(jhawkins): Subclass Browser (specifically, ShowOptionsTab) and inject it
// here to test the visibility of the Sync UI.
class SyncSetupWizardTest : public BrowserWithTestWindowTest {
 public:
  SyncSetupWizardTest()
      : wizard_(NULL),
        service_(NULL) {}
  virtual ~SyncSetupWizardTest() {}
  virtual TestingProfile* BuildProfile() {
    TestingProfile* profile = new TestingProfile();
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(profile,
        ProfileSyncServiceForWizardTest::BuildManual);
    return profile;
  }
  virtual void SetUp() {
    set_profile(BuildProfile());
    profile()->CreateBookmarkModel(false);
    // Wait for the bookmarks model to load.
    profile()->BlockUntilBookmarkModelLoaded();
    PrefService* prefs = profile()->GetPrefs();
    prefs->SetString(prefs::kGoogleServicesUsername, kTestUser);

    handler_.reset(new MockSyncSetupHandler(profile()));
    set_browser(new Browser(Browser::TYPE_TABBED, profile()));
    browser()->SetWindowForTesting(window());
    BrowserList::SetLastActive(browser());
    service_ = static_cast<ProfileSyncServiceForWizardTest*>(
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile()));
    wizard_ = service_->GetWizard();
  }

  virtual void TearDown() {
    wizard_ = NULL;
    service_ = NULL;
    handler_.reset();
  }

 protected:
  void AttachSyncSetupHandler() {
    wizard_->AttachSyncSetupHandler(handler_.get());
  }

  void CompleteSetup() {
    wizard_->Step(SyncSetupWizard::SYNC_EVERYTHING);
    AttachSyncSetupHandler();
    wizard_->Step(SyncSetupWizard::SETTING_UP);
    wizard_->Step(SyncSetupWizard::DONE);
  }

  void CloseSetupUI() {
    handler_->CloseSetupUI();
  }

  // This pointer is owned by the |Service_|.
  SyncSetupWizard* wizard_;
  ProfileSyncServiceForWizardTest* service_;
  scoped_ptr<MockSyncSetupHandler> handler_;
};

TEST_F(SyncSetupWizardTest, ChooseDataTypesSetsPrefs) {
  wizard_->Step(SyncSetupWizard::CONFIGURE);
  AttachSyncSetupHandler();

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
  handler_->HandleConfigure(&data_type_choices_value);
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

TEST_F(SyncSetupWizardTest, ShowErrorUIForPassphraseTest) {
  service_->ClearObservers();
  CompleteSetup();

  // Simulate a passphrase error and make sure the start and end state are set
  // right and wizard is shown.
  service_->set_passphrase_required_reason(sync_api::REASON_ENCRYPTION);
  service_->set_is_using_secondary_passphrase(true);
  service_->ShowErrorUI();
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::ENTER_PASSPHRASE,
            handler_->GetFlow()->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, handler_->GetFlow()->end_state_);
  ASSERT_TRUE(wizard_->IsVisible());

  // Make sure the wizard is dismissed.
  wizard_->Step(SyncSetupWizard::DONE);
  ASSERT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, EnterPassphraseRequired) {
  wizard_->Step(SyncSetupWizard::CONFIGURE);
  AttachSyncSetupHandler();
  wizard_->Step(SyncSetupWizard::SETTING_UP);
  service_->set_passphrase_required_reason(sync_api::REASON_ENCRYPTION);
  wizard_->Step(SyncSetupWizard::ENTER_PASSPHRASE);
  EXPECT_EQ(SyncSetupWizard::ENTER_PASSPHRASE,
            handler_->GetFlow()->current_state_);

  ListValue value;
  value.Append(new StringValue("{\"passphrase\":\"myPassphrase\","
                                "\"mode\":\"gaia\"}"));
  handler_->HandlePassphraseEntry(&value);
  EXPECT_EQ("myPassphrase", service_->passphrase_);
  CloseSetupUI();
}

TEST_F(SyncSetupWizardTest, InvalidTransitions) {
  wizard_->Step(SyncSetupWizard::DONE);
  EXPECT_FALSE(wizard_->IsVisible());

  wizard_->Step(SyncSetupWizard::SYNC_EVERYTHING);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::SYNC_EVERYTHING,
            handler_->GetFlow()->current_state_);
  EXPECT_TRUE(wizard_->IsVisible());

  wizard_->Step(SyncSetupWizard::FATAL_ERROR);
  // Stepping to FATAL_ERROR leaves us in a FATAL_ERROR state and blows away
  // the SyncSetupFlow.
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(handler_->GetFlow() == NULL);
}

TEST_F(SyncSetupWizardTest, FullSuccessfulRunSetsPref) {
  CompleteSetup();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->profile()->GetPrefs()->GetBoolean(
      prefs::kSyncHasSetupCompleted));
}

TEST_F(SyncSetupWizardTest, AbortedByPendingClear) {
  wizard_->Step(SyncSetupWizard::SYNC_EVERYTHING);
  AttachSyncSetupHandler();
  EXPECT_TRUE(wizard_->IsVisible());
  wizard_->Step(SyncSetupWizard::ABORT);
  // Stepping to ABORT should leave us in ABORT state, and should close the
  // wizard.
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, DiscreteRunChooseDataTypes) {
  CompleteSetup();

  wizard_->Step(SyncSetupWizard::CONFIGURE);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::DONE, handler_->GetFlow()->end_state_);

  wizard_->Step(SyncSetupWizard::SETTING_UP);
  wizard_->Step(SyncSetupWizard::DONE);
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, DiscreteRunChooseDataTypesAbortedByPendingClear) {
  CompleteSetup();

  wizard_->Step(SyncSetupWizard::CONFIGURE);
  AttachSyncSetupHandler();
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_EQ(SyncSetupWizard::DONE, handler_->GetFlow()->end_state_);
  wizard_->Step(SyncSetupWizard::ABORT);
  // Stepping to ABORT should leave us in the ABORT state and close the dialog.
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, EnterPassphrase) {
  CompleteSetup();

  // Set up the ENTER_PASSPHRASE case.
  service_->set_passphrase_required_reason(sync_api::REASON_ENCRYPTION);
  service_->set_is_using_secondary_passphrase(true);
  wizard_->Step(SyncSetupWizard::ENTER_PASSPHRASE);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::ENTER_PASSPHRASE,
            handler_->GetFlow()->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, handler_->GetFlow()->end_state_);
  CloseSetupUI();
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, FatalErrorDuringConfigure) {
  CompleteSetup();

  wizard_->Step(SyncSetupWizard::CONFIGURE);
  AttachSyncSetupHandler();
  EXPECT_EQ(SyncSetupWizard::DONE, handler_->GetFlow()->end_state_);
  wizard_->Step(SyncSetupWizard::FATAL_ERROR);
  EXPECT_TRUE(handler_->GetFlow() == NULL);
  EXPECT_FALSE(wizard_->IsVisible());
}

class SyncSetupWizardCrosTest : public SyncSetupWizardTest {
 public:
  virtual TestingProfile* BuildProfile() {
    TestingProfile* profile = new TestingProfile();
    ProfileSyncServiceFactory* f = ProfileSyncServiceFactory::GetInstance();
    f->SetTestingFactory(profile, ProfileSyncServiceForWizardTest::BuildAuto);
    f->GetForProfile(profile)->signin()->SetAuthenticatedUsername(kTestUser);
    return profile;
  }
};
