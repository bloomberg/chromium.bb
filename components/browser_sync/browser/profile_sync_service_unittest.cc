// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/browser_sync/common/browser_sync_switches.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/data_type_manager_observer.h"
#include "components/sync_driver/fake_data_type_controller.h"
#include "components/sync_driver/fake_sync_client.h"
#include "components/sync_driver/glue/sync_backend_host_mock.h"
#include "components/sync_driver/pref_names.h"
#include "components/sync_driver/signin_manager_wrapper.h"
#include "components/sync_driver/sync_api_component_factory_mock.h"
#include "components/sync_driver/sync_driver_features.h"
#include "components/sync_driver/sync_driver_switches.h"
#include "components/sync_driver/sync_prefs.h"
#include "components/sync_driver/sync_service_observer.h"
#include "components/sync_driver/sync_util.h"
#include "components/sync_sessions/fake_sync_sessions_client.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace browser_sync {

namespace {

const char kGaiaId[] = "12345";
const char kEmail[] = "test_user@gmail.com";

void EmptyNetworkTimeUpdate(const base::Time&,
                            const base::TimeDelta&,
                            const base::TimeDelta&) {}

class FakeDataTypeManager : public sync_driver::DataTypeManager {
 public:
  typedef base::Callback<void(syncer::ConfigureReason)> ConfigureCalled;

  explicit FakeDataTypeManager(const ConfigureCalled& configure_called)
      : configure_called_(configure_called) {}

  ~FakeDataTypeManager() override{};

  void Configure(syncer::ModelTypeSet desired_types,
                 syncer::ConfigureReason reason) override {
    DCHECK(!configure_called_.is_null());
    configure_called_.Run(reason);
  }

  void ReenableType(syncer::ModelType type) override {}
  void ResetDataTypeErrors() override {}
  void PurgeForMigration(syncer::ModelTypeSet undesired_types,
                         syncer::ConfigureReason reason) override {}
  void Stop() override{};
  State state() const override {
    return sync_driver::DataTypeManager::CONFIGURED;
  };

 private:
  ConfigureCalled configure_called_;
};

ACTION_P(ReturnNewDataTypeManager, configure_called) {
  return new FakeDataTypeManager(configure_called);
}

using testing::Return;
using testing::StrictMock;
using testing::_;

class TestSyncClient : public sync_driver::FakeSyncClient {
 public:
  TestSyncClient(
      scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory,
      PrefService* pref_service,
      sync_driver::ClearBrowsingDataCallback callback)
      : sync_driver::FakeSyncClient(),
        callback_(callback),
        pref_service_(pref_service),
        component_factory_(std::move(component_factory)) {}
  ~TestSyncClient() override {}

 private:
  // SyncClient:
  PrefService* GetPrefService() override { return pref_service_; }

  sync_sessions::SyncSessionsClient* GetSyncSessionsClient() override {
    return &sync_sessions_client_;
  }

  sync_driver::ClearBrowsingDataCallback GetClearBrowsingDataCallback()
      override {
    return callback_;
  }
  sync_driver::SyncApiComponentFactory* GetSyncApiComponentFactory() override {
    return component_factory_.get();
  }

  sync_driver::ClearBrowsingDataCallback callback_;
  sync_sessions::FakeSyncSessionsClient sync_sessions_client_;
  PrefService* pref_service_;
  scoped_ptr<sync_driver::SyncApiComponentFactory> component_factory_;
};

class TestSyncServiceObserver : public sync_driver::SyncServiceObserver {
 public:
  explicit TestSyncServiceObserver(ProfileSyncService* service)
      : service_(service), first_setup_in_progress_(false) {}
  void OnStateChanged() override {
    first_setup_in_progress_ = service_->IsFirstSetupInProgress();
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
  void Initialize(
      sync_driver::SyncFrontend* frontend,
      scoped_ptr<base::Thread> sync_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_thread,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const GURL& service_url,
      const std::string& sync_user_agent,
      const syncer::SyncCredentials& credentials,
      bool delete_sync_data_folder,
      scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory,
      const syncer::WeakHandle<syncer::UnrecoverableErrorHandler>&
          unrecoverable_error_handler,
      const base::Closure& report_unrecoverable_error_function,
      const HttpPostProviderFactoryGetter& http_post_provider_factory_getter,
      scoped_ptr<syncer::SyncEncryptionHandler::NigoriState> saved_nigori_state)
      override {}
};

class SyncBackendHostMockCollectDeleteDirParam : public SyncBackendHostMock {
 public:
  explicit SyncBackendHostMockCollectDeleteDirParam(
      std::vector<bool>* delete_dir_param)
     : delete_dir_param_(delete_dir_param) {}

  void Initialize(
      sync_driver::SyncFrontend* frontend,
      scoped_ptr<base::Thread> sync_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_thread,
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler,
      const GURL& service_url,
      const std::string& sync_user_agent,
      const syncer::SyncCredentials& credentials,
      bool delete_sync_data_folder,
      scoped_ptr<syncer::SyncManagerFactory> sync_manager_factory,
      const syncer::WeakHandle<syncer::UnrecoverableErrorHandler>&
          unrecoverable_error_handler,
      const base::Closure& report_unrecoverable_error_function,
      const HttpPostProviderFactoryGetter& http_post_provider_factory_getter,
      scoped_ptr<syncer::SyncEncryptionHandler::NigoriState> saved_nigori_state)
      override {
    delete_dir_param_->push_back(delete_sync_data_folder);
    SyncBackendHostMock::Initialize(
        frontend, std::move(sync_thread), db_thread, file_thread, event_handler,
        service_url, sync_user_agent, credentials, delete_sync_data_folder,
        std::move(sync_manager_factory), unrecoverable_error_handler,
        report_unrecoverable_error_function, http_post_provider_factory_getter,
        std::move(saved_nigori_state));
  }

 private:
  std::vector<bool>* delete_dir_param_;
};

// SyncBackendHostMock that calls an external callback when ClearServerData is
// called.
class SyncBackendHostCaptureClearServerData : public SyncBackendHostMock {
 public:
  typedef base::Callback<void(
      const syncer::SyncManager::ClearServerDataCallback&)>
      ClearServerDataCalled;
  explicit SyncBackendHostCaptureClearServerData(
      const ClearServerDataCalled& clear_server_data_called)
      : clear_server_data_called_(clear_server_data_called) {}

  void ClearServerData(
      const syncer::SyncManager::ClearServerDataCallback& callback) override {
    clear_server_data_called_.Run(callback);
  }

 private:
  ClearServerDataCalled clear_server_data_called_;
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

void OnClearServerDataCalled(
    syncer::SyncManager::ClearServerDataCallback* captured_callback,
    const syncer::SyncManager::ClearServerDataCallback& callback) {
  *captured_callback = callback;
}

ACTION_P(ReturnNewMockHostCaptureClearServerData, captured_callback) {
  return new SyncBackendHostCaptureClearServerData(base::Bind(
      &OnClearServerDataCalled, base::Unretained(captured_callback)));
}

// A test harness that uses a real ProfileSyncService and in most cases a
// MockSyncBackendHost.
//
// This is useful if we want to test the ProfileSyncService and don't care about
// testing the SyncBackendHost.
class ProfileSyncServiceTest : public ::testing::Test {
 protected:
  ProfileSyncServiceTest()
      : worker_pool_owner_(2, "sync test worker pool"),
        components_factory_(NULL) {}
  ~ProfileSyncServiceTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSyncDeferredStartupTimeoutSeconds, "0");

    // TODO(crbug.com/544972): Pull all this setup code out into helper classes
    // and/or utils as needed to be reused by other //components-level sync
    // tests.
    sync_driver::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());

    url_request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());

    auth_service_.reset(new FakeProfileOAuth2TokenService());
    auth_service_->set_auto_post_fetch_response_on_message_loop(true);

    signin_client_.reset(new TestSigninClient(prefs()));

    account_tracker_.reset(new AccountTrackerService());
    account_tracker_->Initialize(signin_client_.get());

#if defined(OS_CHROMEOS)
    signin_manager_.reset(new FakeSigninManagerBase(signin_client_.get(),
                                                    account_tracker_.get()));
#else
    signin_manager_.reset(
        new FakeSigninManager(signin_client_.get(), auth_service_.get(),
                              account_tracker_.get(), nullptr));
#endif
    signin_manager_->Initialize(prefs());
  }

  void TearDown() override {
    // Kill the service before the profile.
    if (service_)
      service_->Shutdown();

    service_.reset();
  }

  void IssueTestTokens() {
    std::string account_id = account_tracker_->SeedAccountInfo(kGaiaId, kEmail);
    auth_service_->UpdateCredentials(account_id, "oauth2_login_token");
  }

  void CreateService(ProfileSyncServiceStartBehavior behavior) {
    signin_manager_->SetAuthenticatedAccountInfo(kGaiaId, kEmail);
    scoped_ptr<SyncApiComponentFactoryMock> components_factory(
        new SyncApiComponentFactoryMock());
    components_factory_ = components_factory.get();
    scoped_ptr<sync_driver::SyncClient> sync_client(new TestSyncClient(
        std::move(components_factory), &pref_service_,
        base::Bind(&ProfileSyncServiceTest::ClearBrowsingDataCallback,
                   base::Unretained(this))));

    ProfileSyncService::InitParams init_params;
    init_params.signin_wrapper =
        make_scoped_ptr(new SigninManagerWrapper(signin_manager_.get()));
    init_params.oauth2_token_service = auth_service_.get();
    init_params.start_behavior = behavior;
    init_params.sync_client = std::move(sync_client);
    init_params.network_time_update_callback =
        base::Bind(&EmptyNetworkTimeUpdate);
    init_params.base_directory = base::FilePath(FILE_PATH_LITERAL("dummyPath"));
    init_params.url_request_context = url_request_context_;
    init_params.debug_identifier = "dummyDebugName";
    init_params.channel = version_info::Channel::UNKNOWN;
    init_params.db_thread = base::ThreadTaskRunnerHandle::Get();
    init_params.file_thread = base::ThreadTaskRunnerHandle::Get();
    init_params.blocking_pool = worker_pool_owner_.pool().get();

    service_.reset(new ProfileSyncService(std::move(init_params)));
    service_->RegisterDataTypeController(
        new sync_driver::FakeDataTypeController(syncer::BOOKMARKS));
  }

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  void CreateServiceWithoutSignIn() {
    CreateService(browser_sync::AUTO_START);
    signin_manager_->SignOut(signin_metrics::SIGNOUT_TEST,
                             signin_metrics::SignoutDelete::IGNORE_METRIC);
  }
#endif

  void ShutdownAndDeleteService() {
    if (service_)
      service_->Shutdown();
    service_.reset();
  }

  void InitializeForNthSync() {
    // Set first sync time before initialize to disable backup and simulate
    // a complete sync setup.
    sync_driver::SyncPrefs sync_prefs(
        service_->GetSyncClient()->GetPrefService());
    sync_prefs.SetFirstSyncTime(base::Time::Now());
    sync_prefs.SetFirstSetupComplete();
    sync_prefs.SetKeepEverythingSynced(true);
    service_->Initialize();
  }

  void InitializeForFirstSync() {
    service_->Initialize();
  }

  void TriggerPassphraseRequired() {
    service_->OnPassphraseRequired(syncer::REASON_DECRYPTION,
                                   sync_pb::EncryptedData());
  }

  void TriggerDataTypeStartRequest() {
    service_->OnDataTypeRequestsSyncStartup(syncer::BOOKMARKS);
  }

  void OnConfigureCalled(syncer::ConfigureReason configure_reason) {
    sync_driver::DataTypeManager::ConfigureResult result;
    result.status = sync_driver::DataTypeManager::OK;
    service()->OnConfigureDone(result);
  }

  FakeDataTypeManager::ConfigureCalled GetDefaultConfigureCalledCallback() {
    return base::Bind(&ProfileSyncServiceTest::OnConfigureCalled,
                      base::Unretained(this));
  }

  void OnConfigureCalledRecordReason(syncer::ConfigureReason* reason_dest,
                                     syncer::ConfigureReason reason) {
    DCHECK(reason_dest);
    *reason_dest = reason;
  }

  FakeDataTypeManager::ConfigureCalled GetRecordingConfigureCalledCallback(
      syncer::ConfigureReason* reason) {
    return base::Bind(&ProfileSyncServiceTest::OnConfigureCalledRecordReason,
                      base::Unretained(this), reason);
  }

  void ExpectDataTypeManagerCreation(
      int times,
      const FakeDataTypeManager::ConfigureCalled& callback) {
    EXPECT_CALL(*components_factory_, CreateDataTypeManager(_, _, _, _, _))
        .Times(times)
        .WillRepeatedly(ReturnNewDataTypeManager(callback));
  }

  void ExpectSyncBackendHostCreation(int times) {
    EXPECT_CALL(*components_factory_, CreateSyncBackendHost(_, _, _, _))
        .Times(times)
        .WillRepeatedly(ReturnNewSyncBackendHostMock());
  }

  void ExpectSyncBackendHostCreationCollectDeleteDir(
      int times, std::vector<bool> *delete_dir_param) {
    EXPECT_CALL(*components_factory_, CreateSyncBackendHost(_, _, _, _))
        .Times(times)
        .WillRepeatedly(
            ReturnNewMockHostCollectDeleteDirParam(delete_dir_param));
  }

  void ExpectSyncBackendHostCreationCaptureClearServerData(
      syncer::SyncManager::ClearServerDataCallback* captured_callback) {
    EXPECT_CALL(*components_factory_, CreateSyncBackendHost(_, _, _, _))
        .Times(1)
        .WillOnce(ReturnNewMockHostCaptureClearServerData(captured_callback));
  }

  void PrepareDelayedInitSyncBackendHost() {
    EXPECT_CALL(*components_factory_, CreateSyncBackendHost(_, _, _, _))
        .WillOnce(ReturnNewSyncBackendHostNoReturn());
  }

  AccountTrackerService* account_tracker() { return account_tracker_.get(); }

#if defined(OS_CHROMEOS)
  SigninManagerBase* signin_manager()
#else
  SigninManager* signin_manager()
#endif
  // Opening brace is outside of macro to avoid confusing lint.
  {
    return signin_manager_.get();
  }

  ProfileOAuth2TokenService* auth_service() { return auth_service_.get(); }

  ProfileSyncService* service() {
    return service_.get();
  }

  syncable_prefs::TestingPrefServiceSyncable* prefs() { return &pref_service_; }

  SyncApiComponentFactoryMock* components_factory() {
    return components_factory_;
  }

  void ClearBrowsingDataCallback(base::Time start, base::Time end) {
    clear_browsing_date_start_ = start;
  }

 protected:
  void PumpLoop() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

  // The requested start time when ClearBrowsingDataCallback is called.
  base::Time clear_browsing_date_start_;

 private:
  base::MessageLoop message_loop_;
  base::SequencedWorkerPoolOwner worker_pool_owner_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  syncable_prefs::TestingPrefServiceSyncable pref_service_;
  scoped_ptr<TestSigninClient> signin_client_;
  scoped_ptr<AccountTrackerService> account_tracker_;
#if defined(OS_CHROMEOS)
  scoped_ptr<SigninManagerBase> signin_manager_;
#else
  scoped_ptr<SigninManager> signin_manager_;
#endif
  scoped_ptr<FakeProfileOAuth2TokenService> auth_service_;
  scoped_ptr<ProfileSyncService> service_;

  // The current component factory used by sync. May be null if the server
  // hasn't been created yet.
  SyncApiComponentFactoryMock* components_factory_;
};

// Verify that the server URLs are sane.
TEST_F(ProfileSyncServiceTest, InitialState) {
  CreateService(browser_sync::AUTO_START);
  InitializeForNthSync();
  const std::string& url = service()->sync_service_url().spec();
  EXPECT_TRUE(url == internal::kSyncServerUrl ||
              url == internal::kSyncDevServerUrl);
}

// Verify a successful initialization.
TEST_F(ProfileSyncServiceTest, SuccessfulInitialization) {
  prefs()->SetManagedPref(sync_driver::prefs::kSyncManaged,
                          new base::FundamentalValue(false));
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();
  EXPECT_FALSE(service()->IsManaged());
  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());
}

// Verify that the SetSetupInProgress function call updates state
// and notifies observers.
TEST_F(ProfileSyncServiceTest, SetupInProgress) {
  CreateService(browser_sync::AUTO_START);
  InitializeForFirstSync();

  TestSyncServiceObserver observer(service());
  service()->AddObserver(&observer);

  service()->SetSetupInProgress(true);
  EXPECT_TRUE(observer.first_setup_in_progress());
  service()->SetSetupInProgress(false);
  EXPECT_FALSE(observer.first_setup_in_progress());

  service()->RemoveObserver(&observer);
}

// Verify that disable by enterprise policy works.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyBeforeInit) {
  prefs()->SetManagedPref(sync_driver::prefs::kSyncManaged,
                          new base::FundamentalValue(true));
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);
  InitializeForNthSync();
  EXPECT_TRUE(service()->IsManaged());
  EXPECT_FALSE(service()->IsSyncActive());
}

// Verify that disable by enterprise policy works even after the backend has
// been initialized.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyAfterInit) {
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();

  EXPECT_FALSE(service()->IsManaged());
  EXPECT_TRUE(service()->IsSyncActive());

  prefs()->SetManagedPref(sync_driver::prefs::kSyncManaged,
                          new base::FundamentalValue(true));

  EXPECT_TRUE(service()->IsManaged());
  EXPECT_FALSE(service()->IsSyncActive());
}

// Exercies the ProfileSyncService's code paths related to getting shut down
// before the backend initialize call returns.
TEST_F(ProfileSyncServiceTest, AbortedByShutdown) {
  CreateService(browser_sync::AUTO_START);
  PrepareDelayedInitSyncBackendHost();

  IssueTestTokens();
  InitializeForNthSync();
  EXPECT_FALSE(service()->IsSyncActive());

  ShutdownAndDeleteService();
}

// Test RequestStop() before we've initialized the backend.
TEST_F(ProfileSyncServiceTest, EarlyRequestStop) {
  CreateService(browser_sync::AUTO_START);
  IssueTestTokens();

  service()->RequestStop(ProfileSyncService::KEEP_DATA);
  EXPECT_TRUE(prefs()->GetBoolean(sync_driver::prefs::kSyncSuppressStart));

  // Because of suppression, this should fail.
  sync_driver::SyncPrefs sync_prefs(
      service()->GetSyncClient()->GetPrefService());
  sync_prefs.SetFirstSyncTime(base::Time::Now());
  service()->Initialize();
  EXPECT_FALSE(service()->IsSyncActive());

  // Request start.  This should be enough to allow init to happen.
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);
  service()->RequestStart();
  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_FALSE(prefs()->GetBoolean(sync_driver::prefs::kSyncSuppressStart));
}

// Test RequestStop() after we've initialized the backend.
TEST_F(ProfileSyncServiceTest, DisableAndEnableSyncTemporarily) {
  CreateService(browser_sync::AUTO_START);
  IssueTestTokens();
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();

  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_FALSE(prefs()->GetBoolean(sync_driver::prefs::kSyncSuppressStart));

  testing::Mock::VerifyAndClearExpectations(components_factory());

  service()->RequestStop(ProfileSyncService::KEEP_DATA);
  EXPECT_FALSE(service()->IsSyncActive());
  EXPECT_TRUE(prefs()->GetBoolean(sync_driver::prefs::kSyncSuppressStart));

  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);

  service()->RequestStart();
  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_FALSE(prefs()->GetBoolean(sync_driver::prefs::kSyncSuppressStart));
}

// Certain ProfileSyncService tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out" and policy.
#if !defined (OS_CHROMEOS)
TEST_F(ProfileSyncServiceTest, EnableSyncAndSignOut) {
  CreateService(browser_sync::AUTO_START);
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);
  IssueTestTokens();
  InitializeForNthSync();

  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_FALSE(prefs()->GetBoolean(sync_driver::prefs::kSyncSuppressStart));

  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_FALSE(service()->IsSyncActive());
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(ProfileSyncServiceTest, GetSyncTokenStatus) {
  CreateService(browser_sync::AUTO_START);
  IssueTestTokens();
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
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

TEST_F(ProfileSyncServiceTest, RevokeAccessTokenFromTokenService) {
  CreateService(browser_sync::AUTO_START);
  IssueTestTokens();
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();
  EXPECT_TRUE(service()->IsSyncActive());

  std::string primary_account_id =
      signin_manager()->GetAuthenticatedAccountId();
  auth_service()->LoadCredentials(primary_account_id);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  std::string secondary_account_gaiaid = "1234567";
  std::string secondary_account_name = "test_user2@gmail.com";
  std::string secondary_account_id = account_tracker()->SeedAccountInfo(
      secondary_account_gaiaid, secondary_account_name);
  auth_service()->UpdateCredentials(secondary_account_id,
                                    "second_account_refresh_token");
  auth_service()->RevokeCredentials(secondary_account_id);
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  auth_service()->RevokeCredentials(primary_account_id);
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}

// CrOS does not support signout.
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceTest, SignOutRevokeAccessToken) {
  CreateService(browser_sync::AUTO_START);
  IssueTestTokens();
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();
  EXPECT_TRUE(service()->IsSyncActive());

  std::string primary_account_id =
      signin_manager()->GetAuthenticatedAccountId();
  auth_service()->LoadCredentials(primary_account_id);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}
#endif

#if BUILDFLAG(ENABLE_PRE_SYNC_BACKUP)
TEST_F(ProfileSyncServiceTest, DontStartBackupOnBrowserStart) {
  CreateServiceWithoutSignIn();
  InitializeForFirstSync();
  PumpLoop();
  EXPECT_EQ(ProfileSyncService::IDLE, service()->backend_mode());
}

TEST_F(ProfileSyncServiceTest, BackupBeforeFirstSync) {
  CreateServiceWithoutSignIn();
  ExpectDataTypeManagerCreation(2, GetDefaultConfigureCalledCallback());
  std::vector<bool> delete_dir_param;
  ExpectSyncBackendHostCreationCollectDeleteDir(2, &delete_dir_param);
  InitializeForFirstSync();

  signin_manager()->SetAuthenticatedAccountInfo(kGaiaId, kEmail);
  IssueTestTokens();
  PumpLoop();

  // At this time, backup is finished. Task is posted to start sync again.
  EXPECT_EQ(ProfileSyncService::BACKUP, service()->backend_mode());
  EXPECT_FALSE(service()->IsSyncActive());
  EXPECT_EQ(1u, delete_dir_param.size());
  EXPECT_TRUE(delete_dir_param[0]);

  // Pump loop to start sync.
  PumpLoop();
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());
  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_EQ(2u, delete_dir_param.size());
  EXPECT_TRUE(delete_dir_param[0]);
}

// Test backup is done again on browser start if user signed in last session
// but backup didn't finish when last session was closed.
TEST_F(ProfileSyncServiceTest, ResumeBackupIfAborted) {
  IssueTestTokens();
  CreateService(AUTO_START);
  ExpectDataTypeManagerCreation(2, GetDefaultConfigureCalledCallback());
  std::vector<bool> delete_dir_param;
  ExpectSyncBackendHostCreationCollectDeleteDir(2, &delete_dir_param);
  InitializeForFirstSync();
  PumpLoop();

  // At this time, backup is finished. Task is posted to start sync again.
  EXPECT_EQ(ProfileSyncService::BACKUP, service()->backend_mode());
  EXPECT_FALSE(service()->IsSyncActive());
  EXPECT_EQ(1u, delete_dir_param.size());
  EXPECT_TRUE(delete_dir_param[0]);

  // Pump loop to start sync.
  PumpLoop();
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());
  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_EQ(2u, delete_dir_param.size());
  EXPECT_TRUE(delete_dir_param[0]);
}

TEST_F(ProfileSyncServiceTest, Rollback) {
  CreateService(browser_sync::MANUAL_START);
  service()->SetFirstSetupComplete();
  ExpectDataTypeManagerCreation(2, GetDefaultConfigureCalledCallback());
  std::vector<bool> delete_dir_param;
  ExpectSyncBackendHostCreationCollectDeleteDir(2, &delete_dir_param);
  IssueTestTokens();
  InitializeForNthSync();
  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());

  // First sync time should be recorded.
  sync_driver::SyncPrefs sync_prefs(
      service()->GetSyncClient()->GetPrefService());
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

#endif  // ENABLE_PRE_SYNC_BACKUP

// Verify that LastSyncedTime is cleared when the user signs out.
TEST_F(ProfileSyncServiceTest, ClearLastSyncedTimeOnSignOut) {
  IssueTestTokens();
  CreateService(AUTO_START);
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();
  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SYNC_TIME_JUST_NOW),
            service()->GetLastSyncedTimeString());

  // Sign out.
  service()->RequestStop(ProfileSyncService::CLEAR_DATA);
  PumpLoop();

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SYNC_TIME_NEVER),
            service()->GetLastSyncedTimeString());
}

// Verify that the disable sync flag disables sync.
TEST_F(ProfileSyncServiceTest, DisableSyncFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableSync);
  EXPECT_FALSE(ProfileSyncService::IsSyncAllowedByFlag());
}

// Verify that no disable sync flag enables sync.
TEST_F(ProfileSyncServiceTest, NoDisableSyncFlag) {
  EXPECT_TRUE(ProfileSyncService::IsSyncAllowedByFlag());
}

// Test Sync will stop after receive memory pressure
TEST_F(ProfileSyncServiceTest, MemoryPressureRecording) {
  CreateService(browser_sync::AUTO_START);
  IssueTestTokens();
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();

  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_FALSE(prefs()->GetBoolean(sync_driver::prefs::kSyncSuppressStart));

  testing::Mock::VerifyAndClearExpectations(components_factory());

  sync_driver::SyncPrefs sync_prefs(
      service()->GetSyncClient()->GetPrefService());

  EXPECT_EQ(
      prefs()->GetInteger(sync_driver::prefs::kSyncMemoryPressureWarningCount),
      0);
  EXPECT_FALSE(sync_prefs.DidSyncShutdownCleanly());

  // Simulate memory pressure notification.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();

  // Verify memory pressure recorded.
  EXPECT_EQ(
      prefs()->GetInteger(sync_driver::prefs::kSyncMemoryPressureWarningCount),
      1);
  EXPECT_FALSE(sync_prefs.DidSyncShutdownCleanly());

  // Simulate memory pressure notification.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  ShutdownAndDeleteService();

  // Verify memory pressure and shutdown recorded.
  EXPECT_EQ(
      prefs()->GetInteger(sync_driver::prefs::kSyncMemoryPressureWarningCount),
      2);
  EXPECT_TRUE(sync_prefs.DidSyncShutdownCleanly());
}

// Verify that OnLocalSetPassphraseEncryption triggers catch up configure sync
// cycle, calls ClearServerData, shuts down and restarts sync.
TEST_F(ProfileSyncServiceTest, OnLocalSetPassphraseEncryption) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSyncEnableClearDataOnPassphraseEncryption);
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);

  syncer::SyncManager::ClearServerDataCallback captured_callback;
  syncer::ConfigureReason configure_reason = syncer::CONFIGURE_REASON_UNKNOWN;

  // Initialize sync, ensure that both DataTypeManager and SyncBackendHost are
  // initialized and DTM::Configure is called with
  // CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE.
  ExpectSyncBackendHostCreationCaptureClearServerData(&captured_callback);
  ExpectDataTypeManagerCreation(
      1, GetRecordingConfigureCalledCallback(&configure_reason));
  InitializeForNthSync();
  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());
  testing::Mock::VerifyAndClearExpectations(components_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  sync_driver::DataTypeManager::ConfigureResult result;
  result.status = sync_driver::DataTypeManager::OK;
  service()->OnConfigureDone(result);

  // Simulate user entering encryption passphrase. Ensure that catch up
  // configure cycle is started (DTM::Configure is called with
  // CONFIGURE_REASON_CATCH_UP).
  const syncer::SyncEncryptionHandler::NigoriState nigori_state;
  service()->OnLocalSetPassphraseEncryption(nigori_state);
  EXPECT_EQ(syncer::CONFIGURE_REASON_CATCH_UP, configure_reason);
  EXPECT_TRUE(captured_callback.is_null());

  // Simulate configure successful. Ensure that SBH::ClearServerData is called.
  service()->OnConfigureDone(result);
  EXPECT_FALSE(captured_callback.is_null());

  // Once SBH::ClearServerData finishes successfully ensure that sync is
  // restarted.
  configure_reason = syncer::CONFIGURE_REASON_UNKNOWN;
  ExpectSyncBackendHostCreation(1);
  ExpectDataTypeManagerCreation(
      1, GetRecordingConfigureCalledCallback(&configure_reason));
  captured_callback.Run();
  testing::Mock::VerifyAndClearExpectations(components_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  service()->OnConfigureDone(result);
}

// Verify that if after OnLocalSetPassphraseEncryption catch up configure sync
// cycle gets interrupted, it starts again after browser restart.
TEST_F(ProfileSyncServiceTest,
       OnLocalSetPassphraseEncryption_RestartDuringCatchUp) {
  syncer::ConfigureReason configure_reason = syncer::CONFIGURE_REASON_UNKNOWN;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSyncEnableClearDataOnPassphraseEncryption);
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);
  ExpectSyncBackendHostCreation(1);
  ExpectDataTypeManagerCreation(
      1, GetRecordingConfigureCalledCallback(&configure_reason));
  InitializeForNthSync();
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());
  testing::Mock::VerifyAndClearExpectations(components_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  sync_driver::DataTypeManager::ConfigureResult result;
  result.status = sync_driver::DataTypeManager::OK;
  service()->OnConfigureDone(result);

  // Simulate user entering encryption passphrase. Ensure Configure was called
  // but don't let it continue.
  const syncer::SyncEncryptionHandler::NigoriState nigori_state;
  service()->OnLocalSetPassphraseEncryption(nigori_state);
  EXPECT_EQ(syncer::CONFIGURE_REASON_CATCH_UP, configure_reason);

  // Simulate browser restart. First configuration is a regular one.
  service()->Shutdown();
  syncer::SyncManager::ClearServerDataCallback captured_callback;
  ExpectSyncBackendHostCreationCaptureClearServerData(&captured_callback);
  ExpectDataTypeManagerCreation(
      1, GetRecordingConfigureCalledCallback(&configure_reason));
  service()->RequestStart();
  testing::Mock::VerifyAndClearExpectations(components_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  EXPECT_TRUE(captured_callback.is_null());

  // Simulate configure successful. This time it should be catch up.
  service()->OnConfigureDone(result);
  EXPECT_EQ(syncer::CONFIGURE_REASON_CATCH_UP, configure_reason);
  EXPECT_TRUE(captured_callback.is_null());

  // Simulate catch up configure successful. Ensure that SBH::ClearServerData is
  // called.
  service()->OnConfigureDone(result);
  EXPECT_FALSE(captured_callback.is_null());

  ExpectSyncBackendHostCreation(1);
  ExpectDataTypeManagerCreation(
      1, GetRecordingConfigureCalledCallback(&configure_reason));
  captured_callback.Run();
  testing::Mock::VerifyAndClearExpectations(components_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
}

// Verify that if after OnLocalSetPassphraseEncryption ClearServerData gets
// interrupted, transition again from catch up sync cycle after browser restart.
TEST_F(ProfileSyncServiceTest,
       OnLocalSetPassphraseEncryption_RestartDuringClearServerData) {
  syncer::SyncManager::ClearServerDataCallback captured_callback;
  syncer::ConfigureReason configure_reason = syncer::CONFIGURE_REASON_UNKNOWN;
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSyncEnableClearDataOnPassphraseEncryption);
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);
  ExpectSyncBackendHostCreationCaptureClearServerData(&captured_callback);
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  InitializeForNthSync();
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());
  testing::Mock::VerifyAndClearExpectations(components_factory());

  // Simulate user entering encryption passphrase.
  const syncer::SyncEncryptionHandler::NigoriState nigori_state;
  service()->OnLocalSetPassphraseEncryption(nigori_state);
  EXPECT_FALSE(captured_callback.is_null());
  captured_callback.Reset();

  // Simulate browser restart. First configuration is a regular one.
  service()->Shutdown();
  ExpectSyncBackendHostCreationCaptureClearServerData(&captured_callback);
  ExpectDataTypeManagerCreation(
      1, GetRecordingConfigureCalledCallback(&configure_reason));
  service()->RequestStart();
  testing::Mock::VerifyAndClearExpectations(components_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  EXPECT_TRUE(captured_callback.is_null());

  // Simulate configure successful. This time it should be catch up.
  sync_driver::DataTypeManager::ConfigureResult result;
  result.status = sync_driver::DataTypeManager::OK;
  service()->OnConfigureDone(result);
  EXPECT_EQ(syncer::CONFIGURE_REASON_CATCH_UP, configure_reason);
  EXPECT_TRUE(captured_callback.is_null());

  // Simulate catch up configure successful. Ensure that SBH::ClearServerData is
  // called.
  service()->OnConfigureDone(result);
  EXPECT_FALSE(captured_callback.is_null());

  ExpectSyncBackendHostCreation(1);
  ExpectDataTypeManagerCreation(
      1, GetRecordingConfigureCalledCallback(&configure_reason));
  captured_callback.Run();
  testing::Mock::VerifyAndClearExpectations(components_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
}

// Test that the passphrase prompt due to version change logic gets triggered
// on a datatype type requesting startup, but only happens once.
TEST_F(ProfileSyncServiceTest, PassphrasePromptDueToVersion) {
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);
  ExpectDataTypeManagerCreation(1, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(1);
  InitializeForNthSync();

  sync_driver::SyncPrefs sync_prefs(
      service()->GetSyncClient()->GetPrefService());
  EXPECT_EQ(PRODUCT_VERSION, sync_prefs.GetLastRunVersion());

  sync_prefs.SetPassphrasePrompted(true);

  // Until a datatype requests startup while a passphrase is required the
  // passphrase prompt bit should remain set.
  EXPECT_TRUE(sync_prefs.IsPassphrasePrompted());
  TriggerPassphraseRequired();
  EXPECT_TRUE(sync_prefs.IsPassphrasePrompted());

  // Because the last version was unset, this run should be treated as a new
  // version and force a prompt.
  TriggerDataTypeStartRequest();
  EXPECT_FALSE(sync_prefs.IsPassphrasePrompted());

  // At this point further datatype startup request should have no effect.
  sync_prefs.SetPassphrasePrompted(true);
  TriggerDataTypeStartRequest();
  EXPECT_TRUE(sync_prefs.IsPassphrasePrompted());
}

// Test that when ProfileSyncService receives actionable error
// RESET_LOCAL_SYNC_DATA it restarts sync.
TEST_F(ProfileSyncServiceTest, ResetSyncData) {
  IssueTestTokens();
  CreateService(browser_sync::AUTO_START);
  // Backend should get initialized two times: once during initialization and
  // once when handling actionable error.
  ExpectDataTypeManagerCreation(2, GetDefaultConfigureCalledCallback());
  ExpectSyncBackendHostCreation(2);
  InitializeForNthSync();

  syncer::SyncProtocolError client_cmd;
  client_cmd.action = syncer::RESET_LOCAL_SYNC_DATA;
  service()->OnActionableError(client_cmd);
  EXPECT_EQ(ProfileSyncService::SYNC, service()->backend_mode());
}

// Regression test for crbug/555434. The issue is that check for sessions DTC in
// OnSessionRestoreComplete was creating map entry with nullptr which later was
// dereferenced in OnSyncCycleCompleted. The fix is to use find() to check if
// entry for sessions exists in map.
TEST_F(ProfileSyncServiceTest, ValidPointersInDTCMap) {
  CreateService(browser_sync::AUTO_START);
  service()->OnSessionRestoreComplete();
  service()->OnSyncCycleCompleted();
}

}  // namespace
}  // namespace browser_sync
