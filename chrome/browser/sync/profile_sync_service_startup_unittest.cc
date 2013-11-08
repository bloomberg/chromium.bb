// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/fake_oauth2_token_service.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/glue/data_type_manager_mock.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerMock;
using browser_sync::SyncBackendHostMock;
using content::BrowserThread;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::InvokeArgument;
using testing::Mock;
using testing::Return;

ACTION_P(InvokeOnConfigureStart, pss) {
  ProfileSyncService* service =
      static_cast<ProfileSyncService*>(pss);
  service->OnConfigureStart();
}

ACTION_P2(InvokeOnConfigureDone, pss, result) {
  ProfileSyncService* service =
      static_cast<ProfileSyncService*>(pss);
  DataTypeManager::ConfigureResult configure_result =
      static_cast<DataTypeManager::ConfigureResult>(result);
  service->OnConfigureDone(configure_result);
}

class FakeTokenService : public TokenService {
 public:
  FakeTokenService() {}
  virtual ~FakeTokenService() {}

  virtual void LoadTokensFromDB() OVERRIDE {
    set_tokens_loaded(true);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_TOKEN_LOADING_FINISHED,
        content::Source<TokenService>(this),
        content::NotificationService::NoDetails());
  }

  static BrowserContextKeyedService* BuildFakeTokenService(
      content::BrowserContext* profile) {
    return new FakeTokenService();
  }
};

class ProfileSyncServiceStartupTest : public testing::Test {
 public:
  ProfileSyncServiceStartupTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_DB_THREAD |
                       content::TestBrowserThreadBundle::REAL_FILE_THREAD |
                       content::TestBrowserThreadBundle::REAL_IO_THREAD),
        sync_(NULL) {}

  virtual ~ProfileSyncServiceStartupTest() {
  }

  virtual void SetUp() {
    profile_ = CreateProfile();
  }

  virtual scoped_ptr<TestingProfile> CreateProfile() {
    TestingProfile::Builder builder;
#if defined(OS_CHROMEOS)
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              FakeSigninManagerBase::Build);
#else
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              FakeSigninManager::Build);
#endif
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              FakeOAuth2TokenService::BuildTokenService);
    builder.AddTestingFactory(ProfileSyncServiceFactory::GetInstance(),
                              ProfileSyncServiceStartupTest::BuildService);
    builder.AddTestingFactory(TokenServiceFactory::GetInstance(),
                              FakeTokenService::BuildFakeTokenService);
    return builder.Build();
  }

  virtual void TearDown() {
    sync_->RemoveObserver(&observer_);
    profile_.reset();
  }

  static BrowserContextKeyedService* BuildService(
      content::BrowserContext* browser_context) {
    Profile* profile = static_cast<Profile*>(browser_context);
    return new ProfileSyncService(
        new ProfileSyncComponentsFactoryMock(),
        profile,
        SigninManagerFactory::GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        ProfileSyncService::MANUAL_START);
  }

  void CreateSyncService() {
    sync_ = static_cast<ProfileSyncService*>(
        ProfileSyncServiceFactory::GetForProfile(profile_.get()));
    sync_->AddObserver(&observer_);
  }

  void IssueTestTokens() {
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get())
        ->UpdateCredentials("test_user@gmail.com", "oauth2_login_token");
  }

  ProfileSyncComponentsFactoryMock* components_factory_mock() {
    return static_cast<ProfileSyncComponentsFactoryMock*>(sync_->factory());
  }

 protected:
  DataTypeManagerMock* SetUpDataTypeManager() {
    DataTypeManagerMock* data_type_manager = new DataTypeManagerMock();
    EXPECT_CALL(*components_factory_mock(),
                CreateDataTypeManager(_, _, _, _, _, _)).
        WillOnce(Return(data_type_manager));
    return data_type_manager;
  }

  browser_sync::SyncBackendHostMock* SetUpSyncBackendHost() {
    browser_sync::SyncBackendHostMock* sync_backend_host =
        new browser_sync::SyncBackendHostMock();
    EXPECT_CALL(*components_factory_mock(),
                CreateSyncBackendHost(_, _, _)).
        WillOnce(Return(sync_backend_host));
    return sync_backend_host;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  ProfileSyncService* sync_;
  ProfileSyncServiceObserverMock observer_;
  ProfileSyncComponentsFactoryMock* mock_components_factory_;
};

class ProfileSyncServiceStartupCrosTest : public ProfileSyncServiceStartupTest {
 public:
  virtual void SetUp() {
    ProfileSyncServiceStartupTest::SetUp();
    sync_ = static_cast<ProfileSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), BuildCrosService));
    sync_->AddObserver(&observer_);
  }

  static BrowserContextKeyedService* BuildCrosService(
      content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);
    SigninManagerBase* signin =
        SigninManagerFactory::GetForProfile(profile);
    profile->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                   "test_user@gmail.com");
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
    signin->Initialize(profile, NULL);
    EXPECT_FALSE(signin->GetAuthenticatedUsername().empty());
    return new ProfileSyncService(
        new ProfileSyncComponentsFactoryMock(),
        profile,
        signin,
        oauth2_token_service,
        ProfileSyncService::AUTO_START);
  }
};

TEST_F(ProfileSyncServiceStartupTest, StartFirstTime) {
  // We've never completed startup.
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  SigninManagerFactory::GetForProfile(
      profile_.get())->Initialize(profile_.get(), NULL);
  CreateSyncService();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(0, profile_->GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted));
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Then start things up.
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(1);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED)).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  sync_->SetSetupInProgress(true);

  // Simulate successful signin as test_user.
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "test_user@gmail.com");
  sync_->signin()->SetAuthenticatedUsername("test_user@gmail.com");
  GoogleServiceSigninSuccessDetails details("test_user@gmail.com", "");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));

  // Create some tokens in the token service.
  IssueTestTokens();

  // Simulate the UI telling sync it has finished setting up.
  sync_->SetSetupInProgress(false);
  EXPECT_TRUE(sync_->ShouldPushChanges());
}

// TODO(pavely): Reenable test once android is switched to oauth2.
TEST_F(ProfileSyncServiceStartupTest, DISABLED_StartNoCredentials) {
  // We've never completed startup.
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  SigninManagerFactory::GetForProfile(
      profile_.get())->Initialize(profile_.get(), NULL);
  TokenService* token_service = static_cast<TokenService*>(
      TokenServiceFactory::GetForProfile(profile_.get()));
  CreateSyncService();

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  EXPECT_CALL(*components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(0, profile_->GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted));

  // Then start things up.
  sync_->SetSetupInProgress(true);

  // Simulate successful signin as test_user.
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "test_user@gmail.com");
  sync_->signin()->SetAuthenticatedUsername("test_user@gmail.com");
  GoogleServiceSigninSuccessDetails details("test_user@gmail.com", "");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
  // NOTE: Unlike StartFirstTime, this test does not issue any auth tokens.
  token_service->LoadTokensFromDB();

  sync_->SetSetupInProgress(false);
  // ProfileSyncService should try to start by requesting access token.
  // This request should fail as login token was not issued to TokenService.
  EXPECT_FALSE(sync_->ShouldPushChanges());
  EXPECT_EQ(GoogleServiceAuthError::USER_NOT_SIGNED_UP,
      sync_->GetAuthError().state());
}

// TODO(pavely): Reenable test once android is switched to oauth2.
TEST_F(ProfileSyncServiceStartupTest, DISABLED_StartInvalidCredentials) {
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "test_user@gmail.com");
  SigninManagerFactory::GetForProfile(
      profile_.get())->Initialize(profile_.get(), NULL);
  CreateSyncService();
  SyncBackendHostMock* mock_sbh = SetUpSyncBackendHost();

  // Tell the backend to stall while downloading control types (simulating an
  // auth error).
  mock_sbh->set_fail_initial_download(true);

  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);
  // Issue login token so that ProfileSyncServer tries to initialize backend.
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth2_login_token");

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->Initialize();
  EXPECT_FALSE(sync_->sync_initialized());
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Update the credentials, unstalling the backend.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->SetSetupInProgress(true);

  // Simulate successful signin.
  GoogleServiceSigninSuccessDetails details("test_user@gmail.com",
                                            std::string());
  content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
        content::Source<Profile>(profile_.get()),
        content::Details<const GoogleServiceSigninSuccessDetails>(&details));

  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth2_login_token");
  sync_->SetSetupInProgress(false);

  // Verify we successfully finish startup and configuration.
  EXPECT_TRUE(sync_->ShouldPushChanges());
}

TEST_F(ProfileSyncServiceStartupCrosTest, StartCrosNoCredentials) {
  EXPECT_CALL(*components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*components_factory_mock(),
              CreateSyncBackendHost(_, _, _)).Times(0);
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  TokenService* token_service = static_cast<TokenService*>(
      TokenServiceFactory::GetForProfile(profile_.get()));

  sync_->Initialize();
  // Sync should not start because there are no tokens yet.
  EXPECT_FALSE(sync_->ShouldPushChanges());
  token_service->LoadTokensFromDB();
  sync_->SetSetupInProgress(false);

  // Sync should not start because there are still no tokens.
  EXPECT_FALSE(sync_->ShouldPushChanges());
}

TEST_F(ProfileSyncServiceStartupCrosTest, StartFirstTime) {
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  IssueTestTokens();
  sync_->Initialize();
  EXPECT_TRUE(sync_->ShouldPushChanges());
}

TEST_F(ProfileSyncServiceStartupTest, StartNormal) {
  // Pre load the tokens
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "test_user@gmail.com");
  SigninManagerFactory::GetForProfile(profile_.get())->Initialize(
      profile_.get(), NULL);
  CreateSyncService();
  sync_->SetSyncSetupCompleted();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  IssueTestTokens();

  sync_->Initialize();
}

// Test that we can recover from a case where a bug in the code resulted in
// OnUserChoseDatatypes not being properly called and datatype preferences
// therefore being left unset.
TEST_F(ProfileSyncServiceStartupTest, StartRecoverDatatypePrefs) {
  // Clear the datatype preference fields (simulating bug 154940).
  profile_->GetPrefs()->ClearPref(prefs::kSyncKeepEverythingSynced);
  syncer::ModelTypeSet user_types = syncer::UserTypes();
  for (syncer::ModelTypeSet::Iterator iter = user_types.First();
       iter.Good(); iter.Inc()) {
    profile_->GetPrefs()->ClearPref(
        browser_sync::SyncPrefs::GetPrefNameForDataType(iter.Get()));
  }

  // Pre load the tokens
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "test_user@gmail.com");
  SigninManagerFactory::GetForProfile(profile_.get())->Initialize(
      profile_.get(), NULL);
  CreateSyncService();
  sync_->SetSyncSetupCompleted();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  IssueTestTokens();
  sync_->Initialize();

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kSyncKeepEverythingSynced));
}

// Verify that the recovery of datatype preferences doesn't overwrite a valid
// case where only bookmarks are enabled.
TEST_F(ProfileSyncServiceStartupTest, StartDontRecoverDatatypePrefs) {
  // Explicitly set Keep Everything Synced to false and have only bookmarks
  // enabled.
  profile_->GetPrefs()->SetBoolean(prefs::kSyncKeepEverythingSynced, false);

  // Pre load the tokens
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "test_user@gmail.com");
  SigninManagerFactory::GetForProfile(profile_.get())->Initialize(
      profile_.get(), NULL);
  CreateSyncService();
  sync_->SetSyncSetupCompleted();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  IssueTestTokens();
  sync_->Initialize();

  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kSyncKeepEverythingSynced));
}

TEST_F(ProfileSyncServiceStartupTest, ManagedStartup) {
  // Service should not be started by Initialize() since it's managed.
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "test_user@gmail.com");
  SigninManagerFactory::GetForProfile(profile_.get())->Initialize(
      profile_.get(), NULL);
  CreateSyncService();

  // Disable sync through policy.
  profile_->GetPrefs()->SetBoolean(prefs::kSyncManaged, true);
  EXPECT_CALL(*components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth2_login_token");
  sync_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SwitchManaged) {
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "test_user@gmail.com");
  SigninManagerFactory::GetForProfile(profile_.get())->Initialize(
      profile_.get(), NULL);
  CreateSyncService();
  sync_->SetSyncSetupCompleted();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  IssueTestTokens();
  sync_->Initialize();

  // The service should stop when switching to managed mode.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  profile_->GetPrefs()->SetBoolean(prefs::kSyncManaged, true);

  // When switching back to unmanaged, the state should change, but the service
  // should not start up automatically (kSyncSetupCompleted will be false).
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  profile_->GetPrefs()->ClearPref(prefs::kSyncManaged);
}

TEST_F(ProfileSyncServiceStartupTest, StartFailure) {
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "test_user@gmail.com");
  SigninManagerFactory::GetForProfile(profile_.get())->Initialize(
      profile_.get(), NULL);
  CreateSyncService();
  sync_->SetSyncSetupCompleted();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  DataTypeManager::ConfigureStatus status = DataTypeManager::ABORTED;
  syncer::SyncError error(
      FROM_HERE,
      syncer::SyncError::DATATYPE_ERROR,
      "Association failed.",
      syncer::BOOKMARKS);
  std::map<syncer::ModelType, syncer::SyncError> errors;
  errors[syncer::BOOKMARKS] = error;
  DataTypeManager::ConfigureResult result(
      status,
      syncer::ModelTypeSet(),
      errors,
      syncer::ModelTypeSet(),
      syncer::ModelTypeSet());
  EXPECT_CALL(*data_type_manager, Configure(_, _)).
      WillRepeatedly(
          DoAll(InvokeOnConfigureStart(sync_),
                InvokeOnConfigureDone(sync_, result)));
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::STOPPED));
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  IssueTestTokens();
  sync_->Initialize();
  EXPECT_TRUE(sync_->HasUnrecoverableError());
}

TEST_F(ProfileSyncServiceStartupTest, StartDownloadFailed) {
  // Pre load the tokens
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "test_user@gmail.com");
  SigninManagerFactory::GetForProfile(profile_.get())->Initialize(
      profile_.get(), NULL);
  CreateSyncService();
  SyncBackendHostMock* mock_sbh = SetUpSyncBackendHost();
  mock_sbh->set_fail_initial_download(true);

  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  IssueTestTokens();

  sync_->SetSetupInProgress(true);
  sync_->Initialize();
  sync_->SetSetupInProgress(false);
  EXPECT_FALSE(sync_->sync_initialized());
}
