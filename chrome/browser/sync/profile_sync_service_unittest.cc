// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/fake_oauth2_token_service.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "google/cacheinvalidation/include/types.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/js/js_arg_list.h"
#include "sync/js/js_event_details.h"
#include "sync/js/js_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(akalin): Add tests here that exercise the whole
// ProfileSyncService/SyncBackendHost stack while mocking out as
// little as possible.

namespace browser_sync {

namespace {

using testing::_;
using testing::AtLeast;
using testing::AtMost;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

void SignalDone(base::WaitableEvent* done) {
  done->Signal();
}

class ProfileSyncServiceTest : public testing::Test {
 protected:
  ProfileSyncServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_DB_THREAD |
                       content::TestBrowserThreadBundle::REAL_FILE_THREAD |
                       content::TestBrowserThreadBundle::REAL_IO_THREAD) {
   }

  virtual ~ProfileSyncServiceTest() {}

  virtual void SetUp() OVERRIDE {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              FakeOAuth2TokenService::BuildTokenService);
    profile_ = builder.Build().Pass();
    invalidation::InvalidationServiceFactory::GetInstance()->
        SetBuildOnlyFakeInvalidatorsForTest(true);
  }

  virtual void TearDown() OVERRIDE {
    // Kill the service before the profile.
    if (service_)
      service_->Shutdown();

    service_.reset();
    profile_.reset();

    // Pump messages posted by the sync thread (which may end up
    // posting on the IO thread).
    base::RunLoop().RunUntilIdle();
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
    base::RunLoop().RunUntilIdle();
  }

  // TODO(akalin): Refactor the StartSyncService*() functions below.

  void StartSyncService() {
    StartSyncServiceAndSetInitialSyncEnded(
        true, true, false, true, syncer::STORAGE_IN_MEMORY);
  }

  void StartSyncServiceAndSetInitialSyncEnded(
      bool set_initial_sync_ended,
      bool issue_auth_token,
      bool synchronous_sync_configuration,
      bool sync_setup_completed,
      syncer::StorageOption storage_option) {
    if (service_)
      return;

    SigninManagerBase* signin =
        SigninManagerFactory::GetForProfile(profile_.get());
    signin->SetAuthenticatedUsername("test");
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get());
    ProfileSyncComponentsFactoryMock* factory =
        new ProfileSyncComponentsFactoryMock();
    service_.reset(new TestProfileSyncService(
        factory,
        profile_.get(),
        signin,
        oauth2_token_service,
        ProfileSyncService::AUTO_START,
        true));
    if (!set_initial_sync_ended)
      service_->dont_set_initial_sync_ended_on_init();

    if (synchronous_sync_configuration)
      service_->set_synchronous_sync_configuration();

    service_->set_storage_option(storage_option);
    if (!sync_setup_completed)
      profile_->GetPrefs()->SetBoolean(prefs::kSyncHasSetupCompleted, false);

    // Register the bookmark data type.
    ON_CALL(*factory, CreateDataTypeManager(_, _, _, _, _, _)).
        WillByDefault(ReturnNewDataTypeManager());

    if (issue_auth_token)
      IssueTestTokens();

    service_->Initialize();
  }

  void WaitForBackendInitDone() {
    for (int i = 0; i < 5; ++i) {
      base::WaitableEvent done(false, false);
      service_->GetBackendForTest()->GetSyncLoopForTesting()
          ->PostTask(FROM_HERE, base::Bind(&SignalDone, &done));
      done.Wait();
      base::RunLoop().RunUntilIdle();
      if (service_->sync_initialized()) {
        return;
      }
    }
    LOG(ERROR) << "Backend not initialized.";
  }

  void IssueTestTokens() {
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get())
        ->UpdateCredentials("test", "oauth2_login_token");
  }

  scoped_ptr<TestProfileSyncService> service_;
  scoped_ptr<TestingProfile> profile_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

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
      report_unrecoverable_error_function) OVERRIDE {}
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
// testing the SyncBackendHost.  It's easier to use than the other tests, since
// it doesn't involve any threads.
class ProfileSyncServiceSimpleTest : public ::testing::Test {
 protected:
  ProfileSyncServiceSimpleTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~ProfileSyncServiceSimpleTest() {}

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
TEST_F(ProfileSyncServiceSimpleTest, InitialState) {
  CreateService(ProfileSyncService::AUTO_START);
  Initialize();
  const std::string& url = service()->sync_service_url().spec();
  EXPECT_TRUE(url == ProfileSyncService::kSyncServerUrl ||
              url == ProfileSyncService::kDevServerUrl);
}

// Verify a successful initialization.
TEST_F(ProfileSyncServiceSimpleTest, SuccessfulInitialization) {
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
TEST_F(ProfileSyncServiceSimpleTest, SetupInProgress) {
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
TEST_F(ProfileSyncServiceSimpleTest, DisabledByPolicyBeforeInit) {
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
TEST_F(ProfileSyncServiceSimpleTest, DisabledByPolicyAfterInit) {
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
TEST_F(ProfileSyncServiceSimpleTest, AbortedByShutdown) {
  CreateService(ProfileSyncService::AUTO_START);
  PrepareDelayedInitSyncBackendHost();

  IssueTestTokens();
  Initialize();
  EXPECT_FALSE(service()->sync_initialized());

  ShutdownAndDeleteService();
}

// Test StopAndSuppress() before we've initialized the backend.
TEST_F(ProfileSyncServiceSimpleTest, EarlyStopAndSuppress) {
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
TEST_F(ProfileSyncServiceSimpleTest, DisableAndEnableSyncTemporarily) {
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

TEST_F(ProfileSyncServiceSimpleTest, EnableSyncAndSignOut) {
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

TEST_F(ProfileSyncServiceTest, JsControllerHandlersBasic) {
  StartSyncService();
  EXPECT_TRUE(service_->sync_initialized());
  EXPECT_TRUE(service_->GetBackendForTest() != NULL);

  base::WeakPtr<syncer::JsController> js_controller =
      service_->GetJsController();
  StrictMock<syncer::MockJsEventHandler> event_handler;
  js_controller->AddJsEventHandler(&event_handler);
  js_controller->RemoveJsEventHandler(&event_handler);
}

TEST_F(ProfileSyncServiceTest,
       JsControllerHandlersDelayedBackendInitialization) {
  StartSyncServiceAndSetInitialSyncEnded(true, false, false, true,
                                                  syncer::STORAGE_IN_MEMORY);

  StrictMock<syncer::MockJsEventHandler> event_handler;
  EXPECT_CALL(event_handler, HandleJsEvent(_, _)).Times(AtLeast(1));

  EXPECT_EQ(NULL, service_->GetBackendForTest());
  EXPECT_FALSE(service_->sync_initialized());

  base::WeakPtr<syncer::JsController> js_controller =
      service_->GetJsController();
  js_controller->AddJsEventHandler(&event_handler);
  // Since we're doing synchronous initialization, backend should be
  // initialized by this call.
  IssueTestTokens();
  EXPECT_TRUE(service_->sync_initialized());
  js_controller->RemoveJsEventHandler(&event_handler);
}

TEST_F(ProfileSyncServiceTest, JsControllerProcessJsMessageBasic) {
  StartSyncService();
  WaitForBackendInitDone();

  StrictMock<syncer::MockJsReplyHandler> reply_handler;

  ListValue arg_list1;
  arg_list1.Append(Value::CreateStringValue("INVALIDATIONS_ENABLED"));
  syncer::JsArgList args1(&arg_list1);
  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState", HasArgs(args1)));

  {
    base::WeakPtr<syncer::JsController> js_controller =
        service_->GetJsController();
    js_controller->ProcessJsMessage("getNotificationState", args1,
                                    reply_handler.AsWeakHandle());
  }

  // This forces the sync thread to process the message and reply.
  base::WaitableEvent done(false, false);
  service_->GetBackendForTest()->GetSyncLoopForTesting()
      ->PostTask(FROM_HERE,
                 base::Bind(&SignalDone, &done));
  done.Wait();

  // Call TearDown() to flush the message loops before the mock is destroyed.
  // TearDown() is idempotent, so it's not a problem that it gets called by the
  // test fixture again later.
  TearDown();
}

TEST_F(ProfileSyncServiceTest,
       JsControllerProcessJsMessageBasicDelayedBackendInitialization) {
  StartSyncServiceAndSetInitialSyncEnded(true, false, false, true,
                                                  syncer::STORAGE_IN_MEMORY);

  StrictMock<syncer::MockJsReplyHandler> reply_handler;

  ListValue arg_list1;
  arg_list1.Append(Value::CreateStringValue("INVALIDATIONS_ENABLED"));
  syncer::JsArgList args1(&arg_list1);
  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState", HasArgs(args1)));

  {
    base::WeakPtr<syncer::JsController> js_controller =
        service_->GetJsController();
    js_controller->ProcessJsMessage("getNotificationState",
                                    args1, reply_handler.AsWeakHandle());
  }

  IssueTestTokens();
  WaitForBackendInitDone();

  // This forces the sync thread to process the message and reply.
  base::WaitableEvent done(false, false);
  service_->GetBackendForTest()->GetSyncLoopForTesting()
      ->PostTask(FROM_HERE,
                 base::Bind(&SignalDone, &done));
  done.Wait();
}

// Make sure that things still work if sync is not enabled, but some old sync
// databases are lingering in the "Sync Data" folder.
TEST_F(ProfileSyncServiceTest, TestStartupWithOldSyncData) {
  const char* nonsense1 = "reginald";
  const char* nonsense2 = "beartato";
  const char* nonsense3 = "harrison";
  base::FilePath temp_directory =
      profile_->GetPath().AppendASCII("Sync Data");
  base::FilePath sync_file1 =
      temp_directory.AppendASCII("BookmarkSyncSettings.sqlite3");
  base::FilePath sync_file2 = temp_directory.AppendASCII("SyncData.sqlite3");
  base::FilePath sync_file3 = temp_directory.AppendASCII("nonsense_file");
  ASSERT_TRUE(file_util::CreateDirectory(temp_directory));
  ASSERT_NE(-1,
            file_util::WriteFile(sync_file1, nonsense1, strlen(nonsense1)));
  ASSERT_NE(-1,
            file_util::WriteFile(sync_file2, nonsense2, strlen(nonsense2)));
  ASSERT_NE(-1,
            file_util::WriteFile(sync_file3, nonsense3, strlen(nonsense3)));

  StartSyncServiceAndSetInitialSyncEnded(true, false, true, false,
                                                  syncer::STORAGE_ON_DISK);
  EXPECT_FALSE(service_->HasSyncSetupCompleted());
  EXPECT_FALSE(service_->sync_initialized());

  // Since we're doing synchronous initialization, backend should be
  // initialized by this call.
  IssueTestTokens();

  // Stop the service so we can read the new Sync Data files that were
  // created.
  service_->Shutdown();
  service_.reset();

  // This file should have been deleted when the whole directory was nuked.
  ASSERT_FALSE(base::PathExists(sync_file3));
  ASSERT_FALSE(base::PathExists(sync_file1));

  // This will still exist, but the text should have changed.
  ASSERT_TRUE(base::PathExists(sync_file2));
  std::string file2text;
  ASSERT_TRUE(base::ReadFileToString(sync_file2, &file2text));
  ASSERT_NE(file2text.compare(nonsense2), 0);
}

// Simulates a scenario where a database is corrupted and it is impossible to
// recreate it.  This test is useful mainly when it is run under valgrind.  Its
// expectations are not very interesting.
TEST_F(ProfileSyncServiceTest, FailToOpenDatabase) {
  StartSyncServiceAndSetInitialSyncEnded(false, true, true, true,
                                                  syncer::STORAGE_INVALID);

  // The backend is not ready.  Ensure the PSS knows this.
  EXPECT_FALSE(service_->sync_initialized());
}

// This setup will allow the database to exist, but leave it empty.  The attempt
// to download control types will silently fail (no downloads have any effect in
// these tests).  The sync_backend_host will notice this and inform the profile
// sync service of the failure to initialize the backed.
TEST_F(ProfileSyncServiceTest, FailToDownloadControlTypes) {
  StartSyncServiceAndSetInitialSyncEnded(false, true, true, true,
                                                  syncer::STORAGE_IN_MEMORY);

  // The backend is not ready.  Ensure the PSS knows this.
  EXPECT_FALSE(service_->sync_initialized());
}

TEST_F(ProfileSyncServiceTest, GetSyncTokenStatus) {
  StartSyncService();
  ProfileSyncService::SyncTokenStatus token_status =
      service_->GetSyncTokenStatus();
  EXPECT_EQ(syncer::CONNECTION_NOT_ATTEMPTED, token_status.connection_status);
  EXPECT_TRUE(token_status.connection_status_update_time.is_null());
  EXPECT_TRUE(token_status.token_request_time.is_null());
  EXPECT_TRUE(token_status.token_receive_time.is_null());

  // Sync engine is given invalid token at initialization and will report
  // auth error when trying to connect.
  service_->OnConnectionStatusChange(syncer::CONNECTION_AUTH_ERROR);
  token_status = service_->GetSyncTokenStatus();
  EXPECT_EQ(syncer::CONNECTION_AUTH_ERROR, token_status.connection_status);
  EXPECT_FALSE(token_status.connection_status_update_time.is_null());
  EXPECT_FALSE(token_status.token_request_time.is_null());
  EXPECT_FALSE(token_status.token_receive_time.is_null());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            token_status.last_get_token_error);
  EXPECT_TRUE(token_status.next_token_request_time.is_null());

  // Simulate successful connection.
  service_->OnConnectionStatusChange(syncer::CONNECTION_OK);
  token_status = service_->GetSyncTokenStatus();
  EXPECT_EQ(syncer::CONNECTION_OK, token_status.connection_status);
}

}  // namespace
}  // namespace browser_sync
