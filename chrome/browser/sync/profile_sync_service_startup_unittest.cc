// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/glue/data_type_manager_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerMock;
using content::BrowserThread;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::InvokeArgument;
using testing::Mock;
using testing::Return;

ACTION_P(InvokeOnConfigureStart, pss) {
  TestProfileSyncService* service = static_cast<TestProfileSyncService*>(pss);
  service->OnConfigureStart();
}

ACTION_P2(InvokeOnConfigureDone, pss, result) {
  TestProfileSyncService* service = static_cast<TestProfileSyncService*>(pss);
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
};

class ProfileSyncServiceStartupTest : public testing::Test {
 public:
  ProfileSyncServiceStartupTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        db_thread_(BrowserThread::DB),
        file_thread_(BrowserThread::FILE),
        io_thread_(BrowserThread::IO),
        profile_(new TestingProfile),
        sync_(NULL) {}

  virtual ~ProfileSyncServiceStartupTest() {
  }

  virtual void SetUp() {
    file_thread_.Start();
    io_thread_.StartIOThread();
    profile_->CreateRequestContext();
    CreateSyncService();
    sync_->AddObserver(&observer_);
    sync_->set_synchronous_sync_configuration();
  }

  virtual void TearDown() {
    sync_->RemoveObserver(&observer_);
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), NULL);
    profile_.reset();

    // Pump messages posted by the sync core thread (which may end up
    // posting on the IO thread).
    ui_loop_.RunUntilIdle();
    io_thread_.Stop();
    file_thread_.Stop();
    ui_loop_.RunUntilIdle();
  }

  static ProfileKeyedService* BuildService(Profile* profile) {
    SigninManager* signin = static_cast<SigninManager*>(
        SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile, FakeSigninManager::Build));
    signin->SetAuthenticatedUsername("test_user");
    return new TestProfileSyncService(
        new ProfileSyncComponentsFactoryMock(),
        profile,
        signin,
        ProfileSyncService::MANUAL_START,
        true);
  }

 protected:
  // Overridden below by ProfileSyncServiceStartupCrosTest.
  virtual void CreateSyncService() {
    sync_ = static_cast<TestProfileSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), BuildService));
  }

  DataTypeManagerMock* SetUpDataTypeManager() {
    DataTypeManagerMock* data_type_manager = new DataTypeManagerMock();
    EXPECT_CALL(*sync_->components_factory_mock(),
                CreateDataTypeManager(_, _, _, _, _)).
        WillOnce(Return(data_type_manager));
    return data_type_manager;
  }

  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;
  TestProfileSyncService* sync_;
  ProfileSyncServiceObserverMock observer_;
};

class ProfileSyncServiceStartupCrosTest : public ProfileSyncServiceStartupTest {
 public:
    static ProfileKeyedService* BuildCrosService(Profile* profile) {
      SigninManager* signin = SigninManagerFactory::GetForProfile(profile);
      signin->SetAuthenticatedUsername("test_user");
      return new TestProfileSyncService(
          new ProfileSyncComponentsFactoryMock(),
          profile,
          signin,
          ProfileSyncService::AUTO_START,
          true);
    }
 protected:
  virtual void CreateSyncService() OVERRIDE {
    sync_ = static_cast<TestProfileSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), BuildCrosService));
  }
};

TEST_F(ProfileSyncServiceStartupTest, StartFirstTime) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);

  // We've never completed startup.
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  // Make sure SigninManager doesn't think we're signed in (undoes the call to
  // SetAuthenticatedUsername() in CreateSyncService()).
  SigninManagerFactory::GetForProfile(profile_.get())->SignOut();

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

  // Create some tokens in the token service; the service will startup when
  // it is notified that tokens are available.
  sync_->SetSetupInProgress(true);
  sync_->signin()->StartSignIn("test_user", "", "", "");
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth2_login_token");
  sync_->SetSetupInProgress(false);
  EXPECT_TRUE(sync_->ShouldPushChanges());
}

ProfileKeyedService* BuildFakeTokenService(Profile* profile) {
  return new FakeTokenService();
}

TEST_F(ProfileSyncServiceStartupTest, StartNoCredentials) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);
  TokenService* token_service = static_cast<TokenService*>(
      TokenServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_.get(), BuildFakeTokenService));

  // We've never completed startup.
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  // Make sure SigninManager doesn't think we're signed in (undoes the call to
  // SetAuthenticatedUsername() in CreateSyncService()).
  SigninManagerFactory::GetForProfile(profile_.get())->SignOut();

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->Initialize();
  EXPECT_FALSE(sync_->GetBackendForTest());

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
  sync_->signin()->StartSignIn("test_user", "", "", "");
  // NOTE: Unlike StartFirstTime, this test does not issue any auth tokens.
  token_service->LoadTokensFromDB();
  sync_->SetSetupInProgress(false);
  // Backend should initialize using a bogus GAIA token for credentials.
  EXPECT_TRUE(sync_->ShouldPushChanges());
}

TEST_F(ProfileSyncServiceStartupTest, StartInvalidCredentials) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);
  TokenService* token_service = static_cast<TokenService*>(
      TokenServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_.get(), BuildFakeTokenService));
  token_service->LoadTokensFromDB();

  // Tell the backend to stall while downloading control types (simulating an
  // auth error).
  sync_->fail_initial_download();

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->Initialize();
  EXPECT_TRUE(sync_->GetBackendForTest());
  EXPECT_FALSE(sync_->sync_initialized());
  EXPECT_FALSE(sync_->ShouldPushChanges());
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Update the credentials, unstalling the backend.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->SetSetupInProgress(true);
  sync_->signin()->StartSignIn("test_user", "", "", "");
  token_service->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  sync_->SetSetupInProgress(false);
  MessageLoop::current()->Run();

  // Verify we successfully finish startup and configuration.
  EXPECT_TRUE(sync_->ShouldPushChanges());
}

TEST_F(ProfileSyncServiceStartupCrosTest, StartCrosNoCredentials) {
  EXPECT_CALL(*sync_->components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _)).Times(0);
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  TokenService* token_service = static_cast<TokenService*>(
      TokenServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_.get(), BuildFakeTokenService));

  sync_->Initialize();
  // Sync should not start because there are no tokens yet.
  EXPECT_FALSE(sync_->ShouldPushChanges());
  EXPECT_FALSE(sync_->GetBackendForTest());
  token_service->LoadTokensFromDB();
  sync_->SetSetupInProgress(false);

  // Sync should not start because there are still no tokens.
  EXPECT_FALSE(sync_->ShouldPushChanges());
  EXPECT_FALSE(sync_->GetBackendForTest());
}

TEST_F(ProfileSyncServiceStartupCrosTest, StartFirstTime) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  sync_->Initialize();
  EXPECT_TRUE(sync_->ShouldPushChanges());
}

TEST_F(ProfileSyncServiceStartupTest, StartNormal) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Pre load the tokens
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
  sync_->Initialize();
}

// Test that we can recover from a case where a bug in the code resulted in
// OnUserChoseDatatypes not being properly called and datatype preferences
// therefore being left unset.
TEST_F(ProfileSyncServiceStartupTest, StartRecoverDatatypePrefs) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Clear the datatype preference fields (simulating bug 154940).
  profile_->GetPrefs()->ClearPref(prefs::kSyncKeepEverythingSynced);
  syncer::ModelTypeSet user_types = syncer::UserTypes();
  for (syncer::ModelTypeSet::Iterator iter = user_types.First();
       iter.Good(); iter.Inc()) {
    profile_->GetPrefs()->ClearPref(
        browser_sync::SyncPrefs::GetPrefNameForDataType(iter.Get()));
  }

  // Pre load the tokens
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
  sync_->Initialize();

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kSyncKeepEverythingSynced));
}

// Verify that the recovery of datatype preferences doesn't overwrite a valid
// case where only bookmarks are enabled.
TEST_F(ProfileSyncServiceStartupTest, StartDontRecoverDatatypePrefs) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Explicitly set Keep Everything Synced to false and have only bookmarks
  // enabled.
  profile_->GetPrefs()->SetBoolean(prefs::kSyncKeepEverythingSynced, false);

  // Pre load the tokens
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
  sync_->Initialize();

  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kSyncKeepEverythingSynced));
}

TEST_F(ProfileSyncServiceStartupTest, ManagedStartup) {
  // Disable sync through policy.
  profile_->GetPrefs()->SetBoolean(prefs::kSyncManaged, true);

  EXPECT_CALL(*sync_->components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Service should not be started by Initialize() since it's managed.
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
  sync_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SwitchManaged) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
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
  EXPECT_CALL(*sync_->components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  profile_->GetPrefs()->ClearPref(prefs::kSyncManaged);
}

TEST_F(ProfileSyncServiceStartupTest, StartFailure) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  DataTypeManager::ConfigureStatus status = DataTypeManager::ABORTED;
  syncer::SyncError error(
      FROM_HERE, "Association failed.", syncer::BOOKMARKS);
  std::list<syncer::SyncError> errors;
  errors.push_back(error);
  DataTypeManager::ConfigureResult result(
      status,
      syncer::ModelTypeSet(),
      errors,
      syncer::ModelTypeSet());
  EXPECT_CALL(*data_type_manager, Configure(_, _)).
      WillRepeatedly(
          DoAll(InvokeOnConfigureStart(sync_),
                InvokeOnConfigureDone(sync_, result)));
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::STOPPED));

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
  sync_->Initialize();
  EXPECT_TRUE(sync_->HasUnrecoverableError());
}

TEST_F(ProfileSyncServiceStartupTest, StartDownloadFailed) {
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Preload the tokens.
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  sync_->fail_initial_download();

  sync_->Initialize();
  EXPECT_FALSE(sync_->sync_initialized());
  EXPECT_FALSE(sync_->GetBackendForTest());
}
