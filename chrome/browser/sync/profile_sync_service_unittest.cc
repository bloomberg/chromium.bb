// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/fake_oauth2_token_service.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

ACTION(ReturnNewDataTypeManager) {
  return new browser_sync::DataTypeManagerImpl(arg0,
                                               arg1,
                                               arg2,
                                               arg3,
                                               arg4,
                                               arg5);
}

using testing::_;
using testing::StrictMock;

class TestProfileSyncServiceObserver : public ProfileSyncServiceObserver {
 public:
  explicit TestProfileSyncServiceObserver(ProfileSyncService* service)
      : service_(service), first_setup_in_progress_(false) {}
  virtual void OnStateChanged() OVERRIDE {
    first_setup_in_progress_ = service_->FirstSetupInProgress();
  }
  bool first_setup_in_progress() const { return first_setup_in_progress_; }
 private:
  ProfileSyncService* service_;
  bool first_setup_in_progress_;
};

// A variant of the SyncBackendHostMock that won't automatically
// call back when asked to initialized.  Allows us to test things
// that could happen while backend init is in progress.
class SyncBackendHostNoReturn : public SyncBackendHostMock {
  virtual void Initialize(
      SyncFrontend* frontend,
      scoped_ptr<base::Thread> sync_thread,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const GURL& service_url,
      const syncer::SyncCredentials& credentials,
      bool delete_sync_data_folder,
      scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory,
      scoped_ptr<syncer::UnrecoverableErrorHandler> unrecoverable_error_handler,
      syncer::ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function,
      syncer::NetworkResources* network_resources) OVERRIDE {}
};

ACTION(ReturnNewSyncBackendHostMock) {
  return new browser_sync::SyncBackendHostMock();
}

ACTION(ReturnNewSyncBackendHostNoReturn) {
  return new browser_sync::SyncBackendHostNoReturn();
}

// A test harness that uses a real ProfileSyncService and in most cases a
// MockSyncBackendHost.
//
// This is useful if we want to test the ProfileSyncService and don't care about
// testing the SyncBackendHost.
class ProfileSyncServiceTest : public ::testing::Test {
 protected:
  ProfileSyncServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~ProfileSyncServiceTest() {}

  virtual void SetUp() OVERRIDE {
    TestingProfile::Builder builder;

    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              FakeOAuth2TokenService::BuildTokenService);
    invalidation::InvalidationServiceFactory::GetInstance()->
        SetBuildOnlyFakeInvalidatorsForTest(true);

    profile_ = builder.Build().Pass();
  }

  virtual void TearDown() OVERRIDE {
    // Kill the service before the profile.
    if (service_)
      service_->Shutdown();

    service_.reset();
    profile_.reset();
  }

  void IssueTestTokens() {
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get())
        ->UpdateCredentials("test", "oauth2_login_token");
  }

  void CreateService(ProfileSyncService::StartBehavior behavior) {
    SigninManagerBase* signin =
        SigninManagerFactory::GetForProfile(profile_.get());
    signin->SetAuthenticatedUsername("test");
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get());
    components_factory_ = new StrictMock<ProfileSyncComponentsFactoryMock>();
    service_.reset(new ProfileSyncService(
        components_factory_,
        profile_.get(),
        signin,
        oauth2_token_service,
        behavior));
  }

  void ShutdownAndDeleteService() {
    if (service_)
      service_->Shutdown();
    service_.reset();
  }

  void Initialize() {
    service_->Initialize();
  }

  void ExpectDataTypeManagerCreation() {
    EXPECT_CALL(*components_factory_, CreateDataTypeManager(_, _, _, _, _, _)).
        WillOnce(ReturnNewDataTypeManager());
  }

  void ExpectSyncBackendHostCreation() {
    EXPECT_CALL(*components_factory_, CreateSyncBackendHost(_, _, _)).
        WillOnce(ReturnNewSyncBackendHostMock());
  }

  void PrepareDelayedInitSyncBackendHost() {
    EXPECT_CALL(*components_factory_, CreateSyncBackendHost(_, _, _)).
        WillOnce(ReturnNewSyncBackendHostNoReturn());
  }

  TestingProfile* profile() {
    return profile_.get();
  }

  ProfileSyncService* service() {
    return service_.get();
  }

  ProfileSyncComponentsFactoryMock* components_factory() {
    return components_factory_;
  }

 private:
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<ProfileSyncService> service_;

  // Pointer to the components factory.  Not owned.  May be null.
  ProfileSyncComponentsFactoryMock* components_factory_;

  content::TestBrowserThreadBundle thread_bundle_;
};

// Verify that the server URLs are sane.
TEST_F(ProfileSyncServiceTest, InitialState) {
  CreateService(ProfileSyncService::AUTO_START);
  Initialize();
  const std::string& url = service()->sync_service_url().spec();
  EXPECT_TRUE(url == ProfileSyncService::kSyncServerUrl ||
              url == ProfileSyncService::kDevServerUrl);
}

// Verify a successful initialization.
TEST_F(ProfileSyncServiceTest, SuccessfulInitialization) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSyncManaged,
      Value::CreateBooleanValue(false));
  IssueTestTokens();
  CreateService(ProfileSyncService::AUTO_START);
  ExpectDataTypeManagerCreation();
  ExpectSyncBackendHostCreation();
  Initialize();
  EXPECT_FALSE(service()->IsManaged());
  EXPECT_TRUE(service()->sync_initialized());
}


// Verify that the SetSetupInProgress function call updates state
// and notifies observers.
TEST_F(ProfileSyncServiceTest, SetupInProgress) {
  CreateService(ProfileSyncService::MANUAL_START);
  Initialize();

  TestProfileSyncServiceObserver observer(service());
  service()->AddObserver(&observer);

  service()->SetSetupInProgress(true);
  EXPECT_TRUE(observer.first_setup_in_progress());
  service()->SetSetupInProgress(false);
  EXPECT_FALSE(observer.first_setup_in_progress());

  service()->RemoveObserver(&observer);
}

// Verify that disable by enterprise policy works.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyBeforeInit) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSyncManaged,
      Value::CreateBooleanValue(true));
  IssueTestTokens();
  CreateService(ProfileSyncService::AUTO_START);
  Initialize();
  EXPECT_TRUE(service()->IsManaged());
  EXPECT_FALSE(service()->sync_initialized());
}

// Verify that disable by enterprise policy works even after the backend has
// been initialized.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyAfterInit) {
  IssueTestTokens();
  CreateService(ProfileSyncService::AUTO_START);
  ExpectDataTypeManagerCreation();
  ExpectSyncBackendHostCreation();
  Initialize();

  EXPECT_FALSE(service()->IsManaged());
  EXPECT_TRUE(service()->sync_initialized());

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSyncManaged,
      Value::CreateBooleanValue(true));

  EXPECT_TRUE(service()->IsManaged());
  EXPECT_FALSE(service()->sync_initialized());
}

// Exercies the ProfileSyncService's code paths related to getting shut down
// before the backend initialize call returns.
TEST_F(ProfileSyncServiceTest, AbortedByShutdown) {
  CreateService(ProfileSyncService::AUTO_START);
  PrepareDelayedInitSyncBackendHost();

  IssueTestTokens();
  Initialize();
  EXPECT_FALSE(service()->sync_initialized());

  ShutdownAndDeleteService();
}

// Test StopAndSuppress() before we've initialized the backend.
TEST_F(ProfileSyncServiceTest, EarlyStopAndSuppress) {
  CreateService(ProfileSyncService::AUTO_START);
  IssueTestTokens();

  service()->StopAndSuppress();
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));

  // Because of supression, this should fail.
  Initialize();
  EXPECT_FALSE(service()->sync_initialized());

  // Remove suppression.  This should be enough to allow init to happen.
  ExpectDataTypeManagerCreation();
  ExpectSyncBackendHostCreation();
  service()->UnsuppressAndStart();
  EXPECT_TRUE(service()->sync_initialized());
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));
}

// Test StopAndSuppress() after we've initialized the backend.
TEST_F(ProfileSyncServiceTest, DisableAndEnableSyncTemporarily) {
  CreateService(ProfileSyncService::AUTO_START);
  IssueTestTokens();
  ExpectDataTypeManagerCreation();
  ExpectSyncBackendHostCreation();
  Initialize();

  EXPECT_TRUE(service()->sync_initialized());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));

  testing::Mock::VerifyAndClearExpectations(components_factory());

  service()->StopAndSuppress();
  EXPECT_FALSE(service()->sync_initialized());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));

  ExpectDataTypeManagerCreation();
  ExpectSyncBackendHostCreation();

  service()->UnsuppressAndStart();
  EXPECT_TRUE(service()->sync_initialized());
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));
}

// Certain ProfileSyncService tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out" and policy.
#if !defined (OS_CHROMEOS)

TEST_F(ProfileSyncServiceTest, EnableSyncAndSignOut) {
  CreateService(ProfileSyncService::AUTO_START);
  ExpectDataTypeManagerCreation();
  ExpectSyncBackendHostCreation();
  IssueTestTokens();
  Initialize();

  EXPECT_TRUE(service()->sync_initialized());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));

  SigninManagerFactory::GetForProfile(profile())->SignOut();
  EXPECT_FALSE(service()->sync_initialized());
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(ProfileSyncServiceTest, GetSyncTokenStatus) {
  CreateService(ProfileSyncService::AUTO_START);
  IssueTestTokens();
  ExpectDataTypeManagerCreation();
  ExpectSyncBackendHostCreation();
  Initialize();

  // Initial status.
  ProfileSyncService::SyncTokenStatus token_status =
      service()->GetSyncTokenStatus();
  EXPECT_EQ(syncer::CONNECTION_NOT_ATTEMPTED, token_status.connection_status);
  EXPECT_TRUE(token_status.connection_status_update_time.is_null());
  EXPECT_TRUE(token_status.token_request_time.is_null());
  EXPECT_TRUE(token_status.token_receive_time.is_null());

  // Simulate an auth error.
  service()->OnConnectionStatusChange(syncer::CONNECTION_AUTH_ERROR);

  // The token request will take the form of a posted task.  Run it.
  base::RunLoop loop;
  loop.RunUntilIdle();

  token_status = service()->GetSyncTokenStatus();
  EXPECT_EQ(syncer::CONNECTION_AUTH_ERROR, token_status.connection_status);
  EXPECT_FALSE(token_status.connection_status_update_time.is_null());
  EXPECT_FALSE(token_status.token_request_time.is_null());
  EXPECT_FALSE(token_status.token_receive_time.is_null());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            token_status.last_get_token_error);
  EXPECT_TRUE(token_status.next_token_request_time.is_null());

  // Simulate successful connection.
  service()->OnConnectionStatusChange(syncer::CONNECTION_OK);
  token_status = service()->GetSyncTokenStatus();
  EXPECT_EQ(syncer::CONNECTION_OK, token_status.connection_status);
}

}  // namespace
}  // namespace browser_sync
