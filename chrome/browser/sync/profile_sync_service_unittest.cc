// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/invalidation/fake_invalidation_service.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/pref_names.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class BrowserContext;
}

namespace browser_sync {

namespace {

class FakeDataTypeManager : public sync_driver::DataTypeManager {
 public:
  explicit FakeDataTypeManager(sync_driver::DataTypeManagerObserver* observer)
     : observer_(observer) {}
  virtual ~FakeDataTypeManager() {};

  virtual void Configure(syncer::ModelTypeSet desired_types,
                         syncer::ConfigureReason reason) OVERRIDE {
    sync_driver::DataTypeManager::ConfigureResult result;
    result.status = sync_driver::DataTypeManager::OK;
    observer_->OnConfigureDone(result);
  }

  virtual void PurgeForMigration(syncer::ModelTypeSet undesired_types,
                                 syncer::ConfigureReason reason) OVERRIDE {}
  virtual void Stop() OVERRIDE {};
  virtual State state() const OVERRIDE {
    return sync_driver::DataTypeManager::CONFIGURED;
  };

 private:
  sync_driver::DataTypeManagerObserver* observer_;
};

ACTION(ReturnNewDataTypeManager) {
  return new FakeDataTypeManager(arg4);
}

using testing::Return;
using testing::StrictMock;
using testing::_;

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
      sync_driver::SyncFrontend* frontend,
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

class SyncBackendHostMockCollectDeleteDirParam : public SyncBackendHostMock {
 public:
  explicit SyncBackendHostMockCollectDeleteDirParam(
      std::vector<bool>* delete_dir_param)
     : delete_dir_param_(delete_dir_param) {}

  virtual void Initialize(
      sync_driver::SyncFrontend* frontend,
      scoped_ptr<base::Thread> sync_thread,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const GURL& service_url,
      const syncer::SyncCredentials& credentials,
      bool delete_sync_data_folder,
      scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory,
      scoped_ptr<syncer::UnrecoverableErrorHandler> unrecoverable_error_handler,
      syncer::ReportUnrecoverableErrorFunction
          report_unrecoverable_error_function,
      syncer::NetworkResources* network_resources) OVERRIDE {
    delete_dir_param_->push_back(delete_sync_data_folder);
    SyncBackendHostMock::Initialize(frontend, sync_thread.Pass(),
                                    event_handler, service_url, credentials,
                                    delete_sync_data_folder,
                                    sync_manager_factory.Pass(),
                                    unrecoverable_error_handler.Pass(),
                                    report_unrecoverable_error_function,
                                    network_resources);
  }

 private:
  std::vector<bool>* delete_dir_param_;
};

ACTION(ReturnNewSyncBackendHostMock) {
  return new browser_sync::SyncBackendHostMock();
}

ACTION(ReturnNewSyncBackendHostNoReturn) {
  return new browser_sync::SyncBackendHostNoReturn();
}

ACTION_P(ReturnNewMockHostCollectDeleteDirParam, delete_dir_param) {
  return new browser_sync::SyncBackendHostMockCollectDeleteDirParam(
      delete_dir_param);
}

KeyedService* BuildFakeProfileInvalidationProvider(
    content::BrowserContext* context) {
  return new invalidation::ProfileInvalidationProvider(
      scoped_ptr<invalidation::InvalidationService>(
          new invalidation::FakeInvalidationService));
}

// A test harness that uses a real ProfileSyncService and in most cases a
// MockSyncBackendHost.
//
// This is useful if we want to test the ProfileSyncService and don't care about
// testing the SyncBackendHost.
class ProfileSyncServiceTest : public ::testing::Test {
 protected:
  ProfileSyncServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  virtual ~ProfileSyncServiceTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSyncDeferredStartupTimeoutSeconds, "0");

    CHECK(profile_manager_.SetUp());

    TestingProfile::TestingFactories testing_facotries;
    testing_facotries.push_back(
            std::make_pair(ProfileOAuth2TokenServiceFactory::GetInstance(),
                           BuildAutoIssuingFakeProfileOAuth2TokenService));
    testing_facotries.push_back(
            std::make_pair(
                invalidation::ProfileInvalidationProviderFactory::GetInstance(),
                BuildFakeProfileInvalidationProvider));

    profile_ = profile_manager_.CreateTestingProfile(
        "sync-service-test", scoped_ptr<PrefServiceSyncable>(),
        base::UTF8ToUTF16("sync-service-test"), 0, std::string(),
        testing_facotries);
  }

  virtual void TearDown() OVERRIDE {
    // Kill the service before the profile.
    if (service_)
      service_->Shutdown();

    service_.reset();
  }

  void IssueTestTokens() {
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
        ->UpdateCredentials("test", "oauth2_login_token");
  }

  void CreateService(ProfileSyncServiceStartBehavior behavior) {
    SigninManagerBase* signin =
        SigninManagerFactory::GetForProfile(profile_);
    signin->SetAuthenticatedUsername("test");
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
    components_factory_ = new ProfileSyncComponentsFactoryMock();
    service_.reset(new ProfileSyncService(
        scoped_ptr<ProfileSyncComponentsFactory>(components_factory_),
        profile_,
        make_scoped_ptr(new SupervisedUserSigninManagerWrapper(profile_,
                                                               signin)),
        oauth2_token_service,
        behavior));
    service_->SetClearingBrowseringDataForTesting(
        base::Bind(&ProfileSyncServiceTest::ClearBrowsingDataCallback,
                   base::Unretained(this)));
  }

#if defined(OS_WIN) || defined(OS_MACOSX) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  void CreateServiceWithoutSignIn() {
    CreateService(browser_sync::AUTO_START);
    SigninManagerFactory::GetForProfile(profile())->SignOut(
        signin_metrics::SIGNOUT_TEST);
  }
#endif

  void ShutdownAndDeleteService() {
    if (service_)
      service_->Shutdown();
    service_.reset();
  }

  void InitializeForNthSync() {
    // Set first sync time before initialize to disable backup.
    sync_driver::SyncPrefs sync_prefs(service()->profile()->GetPrefs());
    sync_prefs.SetFirstSyncTime(base::Time::Now());
    service_->Initialize();
  }

  void InitializeForFirstSync() {
    service_->Initialize();
  }

  void ExpectDataTypeManagerCreation(int times) {
    EXPECT_CALL(*components_factory_, CreateDataTypeManager(_, _, _, _, _, _))
        .Times(times)
        .WillRepeatedly(ReturnNewDataTypeManager());
  }

  void ExpectSyncBackendHostCreation(int times) {
    EXPECT_CALL(*components_factory_, CreateSyncBackendHost(_, _, _, _, _))
        .Times(times)
        .WillRepeatedly(ReturnNewSyncBackendHostMock());
  }

  void ExpectSyncBackendHostCreationCollectDeleteDir(
      int times, std::vector<bool> *delete_dir_param) {
    EXPECT_CALL(*components_factory_, CreateSyncBackendHost(_, _, _, _, _))
        .Times(times)
        .WillRepeatedly(ReturnNewMockHostCollectDeleteDirParam(
            delete_dir_param));
  }

  void PrepareDelayedInitSyncBackendHost() {
    EXPECT_CALL(*components_factory_, CreateSyncBackendHost(_, _, _, _, _)).
        WillOnce(ReturnNewSyncBackendHostNoReturn());
  }

  TestingProfile* profile() {
    return profile_;
  }

  ProfileSyncService* service() {
    return service_.get();
  }

  ProfileSyncComponentsFactoryMock* components_factory() {
    return components_factory_;
  }

  void ClearBrowsingDataCallback(Profile* profile, base::Time start,
                                 base::Time end) {
    EXPECT_EQ(profile_, profile);
    clear_browsing_date_start_ = start;
  }

 protected:
  void PumpLoop() {
    base::RunLoop run_loop;
    base::MessageLoop::current()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // The requested start time when ClearBrowsingDataCallback is called.
  base::Time clear_browsing_date_start_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  scoped_ptr<ProfileSyncService> service_;

  // Pointer to the components factory.  Not owned.  May be null.
  ProfileSyncComponentsFactoryMock* components_factory_;
};

// Verify that the server URLs are sane.
TEST_F(ProfileSyncServiceTest, InitialState) {
  CreateService(browser_sync::AUTO_START);
  InitializeForNthSync();
  const std::string& url = service()->sync_service_url().spec();
  EXPECT_TRUE(url == ProfileSyncService::kSyncServerUrl ||
              url == ProfileSyncService::kDevServerUrl);
}

// Verify a successful initialization.
TEST_F(ProfileSyncServiceTest, SuccessfulInitialization) {
  profile()->GetTestingPrefService()->SetManagedPref(
      sync_driver::prefs::kSyncManaged, new base::FundamentalValue(false));
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);
  ExpectDataTypeManagerCreation(1);
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();
  EXPECT_FALSE(service()->IsManaged());
  EXPECT_TRUE(service()->sync_initialized());
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());
}


// Verify that the SetSetupInProgress function call updates state
// and notifies observers.
TEST_F(ProfileSyncServiceTest, SetupInProgress) {
  CreateService(browser_sync::AUTO_START);
  InitializeForNthSync();

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
      sync_driver::prefs::kSyncManaged, new base::FundamentalValue(true));
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);
  InitializeForNthSync();
  EXPECT_TRUE(service()->IsManaged());
  EXPECT_FALSE(service()->sync_initialized());
}

// Verify that disable by enterprise policy works even after the backend has
// been initialized.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyAfterInit) {
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);
  ExpectDataTypeManagerCreation(1);
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();

  EXPECT_FALSE(service()->IsManaged());
  EXPECT_TRUE(service()->sync_initialized());

  profile()->GetTestingPrefService()->SetManagedPref(
      sync_driver::prefs::kSyncManaged, new base::FundamentalValue(true));

  EXPECT_TRUE(service()->IsManaged());
  EXPECT_FALSE(service()->sync_initialized());
}

// Exercies the ProfileSyncService's code paths related to getting shut down
// before the backend initialize call returns.
TEST_F(ProfileSyncServiceTest, AbortedByShutdown) {
  CreateService(browser_sync::AUTO_START);
  PrepareDelayedInitSyncBackendHost();

  IssueTestTokens();
  InitializeForNthSync();
  EXPECT_FALSE(service()->sync_initialized());

  ShutdownAndDeleteService();
}

// Test StopAndSuppress() before we've initialized the backend.
TEST_F(ProfileSyncServiceTest, EarlyStopAndSuppress) {
  CreateService(browser_sync::AUTO_START);
  IssueTestTokens();

  service()->StopAndSuppress();
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      sync_driver::prefs::kSyncSuppressStart));

  // Because of supression, this should fail.
  InitializeForNthSync();
  EXPECT_FALSE(service()->sync_initialized());

  // Remove suppression.  This should be enough to allow init to happen.
  ExpectDataTypeManagerCreation(1);
  ExpectSyncBackendHostCreation(1);
  service()->UnsuppressAndStart();
  EXPECT_TRUE(service()->sync_initialized());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      sync_driver::prefs::kSyncSuppressStart));
}

// Test StopAndSuppress() after we've initialized the backend.
TEST_F(ProfileSyncServiceTest, DisableAndEnableSyncTemporarily) {
  CreateService(browser_sync::AUTO_START);
  IssueTestTokens();
  ExpectDataTypeManagerCreation(1);
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();

  EXPECT_TRUE(service()->sync_initialized());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      sync_driver::prefs::kSyncSuppressStart));

  testing::Mock::VerifyAndClearExpectations(components_factory());

  service()->StopAndSuppress();
  EXPECT_FALSE(service()->sync_initialized());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      sync_driver::prefs::kSyncSuppressStart));

  ExpectDataTypeManagerCreation(1);
  ExpectSyncBackendHostCreation(1);

  service()->UnsuppressAndStart();
  EXPECT_TRUE(service()->sync_initialized());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      sync_driver::prefs::kSyncSuppressStart));
}

// Certain ProfileSyncService tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out" and policy.
#if !defined (OS_CHROMEOS)
TEST_F(ProfileSyncServiceTest, EnableSyncAndSignOut) {
  CreateService(browser_sync::AUTO_START);
  ExpectDataTypeManagerCreation(1);
  ExpectSyncBackendHostCreation(1);
  IssueTestTokens();
  InitializeForNthSync();

  EXPECT_TRUE(service()->sync_initialized());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      sync_driver::prefs::kSyncSuppressStart));

  SigninManagerFactory::GetForProfile(profile())->SignOut(
      signin_metrics::SIGNOUT_TEST);
  EXPECT_FALSE(service()->sync_initialized());
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(ProfileSyncServiceTest, GetSyncTokenStatus) {
  CreateService(browser_sync::AUTO_START);
  IssueTestTokens();
  ExpectDataTypeManagerCreation(1);
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();

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

#if defined(ENABLE_PRE_SYNC_BACKUP)
TEST_F(ProfileSyncServiceTest, DontStartBackupOnBrowserStart) {
  CreateServiceWithoutSignIn();
  InitializeForFirstSync();
  PumpLoop();
  EXPECT_EQ(ProfileSyncService::IDLE, service()->backend_mode());
}

TEST_F(ProfileSyncServiceTest, BackupBeforeFirstSync) {
  CreateServiceWithoutSignIn();
  ExpectDataTypeManagerCreation(2);
  std::vector<bool> delete_dir_param;
  ExpectSyncBackendHostCreationCollectDeleteDir(2, &delete_dir_param);
  InitializeForFirstSync();

  SigninManagerFactory::GetForProfile(profile())
      ->SetAuthenticatedUsername("test");
  IssueTestTokens();
  PumpLoop();

  // At this time, backup is finished. Task is posted to start sync again.
  EXPECT_EQ(ProfileSyncService::BACKUP, service()->backend_mode());
  EXPECT_TRUE(service()->ShouldPushChanges());
  EXPECT_EQ(1u, delete_dir_param.size());
  EXPECT_TRUE(delete_dir_param[0]);

  // Pump loop to start sync.
  PumpLoop();
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());
  EXPECT_EQ(2u, delete_dir_param.size());
  EXPECT_TRUE(delete_dir_param[0]);
}

// Test backup is done again on browser start if user signed in last session
// but backup didn't finish when last session was closed.
TEST_F(ProfileSyncServiceTest, ResumeBackupIfAborted) {
  IssueTestTokens();
  CreateService(AUTO_START);
  ExpectDataTypeManagerCreation(2);
  std::vector<bool> delete_dir_param;
  ExpectSyncBackendHostCreationCollectDeleteDir(2, &delete_dir_param);
  InitializeForFirstSync();
  PumpLoop();

  // At this time, backup is finished. Task is posted to start sync again.
  EXPECT_EQ(ProfileSyncService::BACKUP, service()->backend_mode());
  EXPECT_TRUE(service()->ShouldPushChanges());
  EXPECT_EQ(1u, delete_dir_param.size());
  EXPECT_TRUE(delete_dir_param[0]);

  // Pump loop to start sync.
  PumpLoop();
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());
  EXPECT_EQ(2u, delete_dir_param.size());
  EXPECT_TRUE(delete_dir_param[0]);
}

TEST_F(ProfileSyncServiceTest, Rollback) {
  CreateService(browser_sync::MANUAL_START);
  service()->SetSyncSetupCompleted();
  ExpectDataTypeManagerCreation(2);
  std::vector<bool> delete_dir_param;
  ExpectSyncBackendHostCreationCollectDeleteDir(2, &delete_dir_param);
  IssueTestTokens();
  InitializeForNthSync();
  EXPECT_TRUE(service()->sync_initialized());
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());

  // First sync time should be recorded.
  sync_driver::SyncPrefs sync_prefs(service()->profile()->GetPrefs());
  base::Time first_sync_time = sync_prefs.GetFirstSyncTime();
  EXPECT_FALSE(first_sync_time.is_null());

  syncer::SyncProtocolError client_cmd;
  client_cmd.action = syncer::DISABLE_SYNC_AND_ROLLBACK;
  service()->OnActionableError(client_cmd);
  EXPECT_EQ(ProfileSyncService::IDLE, service()->backend_mode());

  // Pump loop to run rollback.
  PumpLoop();
  EXPECT_EQ(ProfileSyncService::ROLLBACK, service()->backend_mode());

  // Browser data should be cleared during rollback.
  EXPECT_EQ(first_sync_time, clear_browsing_date_start_);

  client_cmd.action = syncer::ROLLBACK_DONE;
  service()->OnActionableError(client_cmd);
  EXPECT_EQ(ProfileSyncService::IDLE, service()->backend_mode());

  // First sync time is erased after rollback is done.
  EXPECT_TRUE(sync_prefs.GetFirstSyncTime().is_null());

  EXPECT_EQ(2u, delete_dir_param.size());
  EXPECT_FALSE(delete_dir_param[0]);
  EXPECT_FALSE(delete_dir_param[1]);
}

#endif

TEST_F(ProfileSyncServiceTest, GetSyncServiceURL) {
  // See that we can override the URL with a flag.
  CommandLine command_line(
      base::FilePath(base::FilePath(FILE_PATH_LITERAL("chrome.exe"))));
  command_line.AppendSwitchASCII(switches::kSyncServiceURL, "https://foo/bar");
  EXPECT_EQ("https://foo/bar",
            ProfileSyncService::GetSyncServiceURL(command_line).spec());
}

}  // namespace
}  // namespace browser_sync
