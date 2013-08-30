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
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
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

class ProfileSyncServiceTestHarness {
 public:
  ProfileSyncServiceTestHarness()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_DB_THREAD |
                       content::TestBrowserThreadBundle::REAL_FILE_THREAD |
                       content::TestBrowserThreadBundle::REAL_IO_THREAD) {
   }

  void SetUp() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              FakeOAuth2TokenService::BuildTokenService);
    profile = builder.Build().Pass();
    invalidation::InvalidationServiceFactory::GetInstance()->
        SetBuildOnlyFakeInvalidatorsForTest(true);
  }

  void TearDown() {
    // Kill the service before the profile.
    if (service) {
      service->Shutdown();
    }
    service.reset();
    profile.reset();
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
    if (!service) {
      SigninManagerBase* signin =
          SigninManagerFactory::GetForProfile(profile.get());
      signin->SetAuthenticatedUsername("test");
      ProfileSyncComponentsFactoryMock* factory =
          new ProfileSyncComponentsFactoryMock();
      service.reset(new TestProfileSyncService(
          factory,
          profile.get(),
          signin,
          ProfileSyncService::AUTO_START,
          true));
      if (!set_initial_sync_ended)
        service->dont_set_initial_sync_ended_on_init();
      if (synchronous_sync_configuration)
        service->set_synchronous_sync_configuration();
      service->set_storage_option(storage_option);
      if (!sync_setup_completed)
        profile->GetPrefs()->SetBoolean(prefs::kSyncHasSetupCompleted, false);

      // Register the bookmark data type.
      ON_CALL(*factory, CreateDataTypeManager(_, _, _, _, _, _)).
          WillByDefault(ReturnNewDataTypeManager());

      if (issue_auth_token) {
        IssueTestTokens();
      }
      service->Initialize();
    }
  }

  void WaitForBackendInitDone() {
    for (int i = 0; i < 5; ++i) {
      base::WaitableEvent done(false, false);
      service->GetBackendForTest()->GetSyncLoopForTesting()
          ->PostTask(FROM_HERE,
                     base::Bind(&SignalDone, &done));
      done.Wait();
      base::RunLoop().RunUntilIdle();
      if (service->sync_initialized()) {
        return;
      }
    }
    LOG(ERROR) << "Backend not initialized.";
  }

  void IssueTestTokens() {
    TokenService* token_service =
        TokenServiceFactory::GetForProfile(profile.get());
    token_service->IssueAuthTokenForTest(
        GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth2_login_token");
    token_service->IssueAuthTokenForTest(
          GaiaConstants::kSyncService, "token");
  }

  scoped_ptr<TestProfileSyncService> service;
  scoped_ptr<TestingProfile> profile;

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

class ProfileSyncServiceTest : public testing::Test {
 protected:
  virtual void SetUp() {
    harness_.SetUp();
  }

  virtual void TearDown() {
    harness_.TearDown();
  }

  ProfileSyncServiceTestHarness harness_;
};

TEST_F(ProfileSyncServiceTest, InitialState) {
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(harness_.profile.get());
  harness_.service.reset(new TestProfileSyncService(
      new ProfileSyncComponentsFactoryMock(),
      harness_.profile.get(),
      signin,
      ProfileSyncService::MANUAL_START,
      true));
  harness_.service->Initialize();
  EXPECT_TRUE(
      harness_.service->sync_service_url().spec() ==
        ProfileSyncService::kSyncServerUrl ||
      harness_.service->sync_service_url().spec() ==
        ProfileSyncService::kDevServerUrl);
}

// Tests that the sync service doesn't forget to notify observers about
// setup state.
TEST(ProfileSyncServiceTestBasic, SetupInProgress) {
  ProfileSyncService service(
      NULL, NULL, NULL, ProfileSyncService::MANUAL_START);
  TestProfileSyncServiceObserver observer(&service);
  service.AddObserver(&observer);
  service.SetSetupInProgress(true);
  EXPECT_TRUE(observer.first_setup_in_progress());
  service.SetSetupInProgress(false);
  EXPECT_FALSE(observer.first_setup_in_progress());
  service.RemoveObserver(&observer);
}

TEST_F(ProfileSyncServiceTest, DisabledByPolicy) {
  harness_.profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSyncManaged,
      Value::CreateBooleanValue(true));
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(harness_.profile.get());
  harness_.service.reset(new TestProfileSyncService(
      new ProfileSyncComponentsFactoryMock(),
      harness_.profile.get(),
      signin,
      ProfileSyncService::MANUAL_START,
      true));
  harness_.service->Initialize();
  EXPECT_TRUE(harness_.service->IsManaged());
}

TEST_F(ProfileSyncServiceTest, AbortedByShutdown) {
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(harness_.profile.get());
  signin->SetAuthenticatedUsername("test");
  ProfileSyncComponentsFactoryMock* factory =
      new ProfileSyncComponentsFactoryMock();
  harness_.service.reset(new TestProfileSyncService(
      factory,
      harness_.profile.get(),
      signin,
      ProfileSyncService::AUTO_START,
      true));
  EXPECT_CALL(*factory, CreateDataTypeManager(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*factory, CreateBookmarkSyncComponents(_, _)).
      Times(0);
  harness_.service->RegisterDataTypeController(
      new BookmarkDataTypeController(harness_.service->factory(),
                                     harness_.profile.get(),
                                     harness_.service.get()));

  harness_.service->Initialize();
  harness_.service->Shutdown();
  harness_.service.reset();
}

TEST_F(ProfileSyncServiceTest, DisableAndEnableSyncTemporarily) {
  SigninManagerBase* signin =
      SigninManagerFactory::GetForProfile(harness_.profile.get());
  signin->SetAuthenticatedUsername("test");
  ProfileSyncComponentsFactoryMock* factory =
      new ProfileSyncComponentsFactoryMock();
  harness_.service.reset(new TestProfileSyncService(
      factory,
      harness_.profile.get(),
      signin,
      ProfileSyncService::AUTO_START,
      true));
  // Register the bookmark data type.
  EXPECT_CALL(*factory, CreateDataTypeManager(_, _, _, _, _, _)).
      WillRepeatedly(ReturnNewDataTypeManager());

  harness_.IssueTestTokens();

  harness_.service->Initialize();
  EXPECT_TRUE(harness_.service->sync_initialized());
  EXPECT_TRUE(harness_.service->GetBackendForTest() != NULL);
  EXPECT_FALSE(
      harness_.profile->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));

  harness_.service->StopAndSuppress();
  EXPECT_FALSE(harness_.service->sync_initialized());
  EXPECT_TRUE(
      harness_.profile->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));

  harness_.service->UnsuppressAndStart();
  EXPECT_TRUE(harness_.service->sync_initialized());
  EXPECT_FALSE(
      harness_.profile->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));
}

// Certain ProfileSyncService tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out" and policy.
#if !defined (OS_CHROMEOS)

TEST_F(ProfileSyncServiceTest, EnableSyncAndSignOut) {
  SigninManager* signin =
      SigninManagerFactory::GetForProfile(harness_.profile.get());
  signin->SetAuthenticatedUsername("test@test.com");
  ProfileSyncComponentsFactoryMock* factory =
      new ProfileSyncComponentsFactoryMock();
  harness_.service.reset(new TestProfileSyncService(
      factory,
      harness_.profile.get(),
      signin,
      ProfileSyncService::AUTO_START,
      true));
  // Register the bookmark data type.
  EXPECT_CALL(*factory, CreateDataTypeManager(_, _, _, _, _, _)).
      WillRepeatedly(ReturnNewDataTypeManager());

  harness_.IssueTestTokens();

  harness_.service->Initialize();
  EXPECT_TRUE(harness_.service->sync_initialized());
  EXPECT_TRUE(harness_.service->GetBackendForTest() != NULL);
  EXPECT_FALSE(
      harness_.profile->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));

  signin->SignOut();
  EXPECT_FALSE(harness_.service->sync_initialized());
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(ProfileSyncServiceTest, JsControllerHandlersBasic) {
  harness_.StartSyncService();
  EXPECT_TRUE(harness_.service->sync_initialized());
  EXPECT_TRUE(harness_.service->GetBackendForTest() != NULL);

  base::WeakPtr<syncer::JsController> js_controller =
      harness_.service->GetJsController();
  StrictMock<syncer::MockJsEventHandler> event_handler;
  js_controller->AddJsEventHandler(&event_handler);
  js_controller->RemoveJsEventHandler(&event_handler);
}

TEST_F(ProfileSyncServiceTest,
       JsControllerHandlersDelayedBackendInitialization) {
  harness_.StartSyncServiceAndSetInitialSyncEnded(true, false, false, true,
                                                  syncer::STORAGE_IN_MEMORY);

  StrictMock<syncer::MockJsEventHandler> event_handler;
  EXPECT_CALL(event_handler, HandleJsEvent(_, _)).Times(AtLeast(1));

  EXPECT_EQ(NULL, harness_.service->GetBackendForTest());
  EXPECT_FALSE(harness_.service->sync_initialized());

  base::WeakPtr<syncer::JsController> js_controller =
      harness_.service->GetJsController();
  js_controller->AddJsEventHandler(&event_handler);
  // Since we're doing synchronous initialization, backend should be
  // initialized by this call.
  harness_.IssueTestTokens();
  EXPECT_TRUE(harness_.service->sync_initialized());
  js_controller->RemoveJsEventHandler(&event_handler);
}

TEST_F(ProfileSyncServiceTest, JsControllerProcessJsMessageBasic) {
  harness_.StartSyncService();
  harness_.WaitForBackendInitDone();

  StrictMock<syncer::MockJsReplyHandler> reply_handler;

  ListValue arg_list1;
  arg_list1.Append(Value::CreateStringValue("INVALIDATIONS_ENABLED"));
  syncer::JsArgList args1(&arg_list1);
  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState", HasArgs(args1)));

  {
    base::WeakPtr<syncer::JsController> js_controller =
        harness_.service->GetJsController();
    js_controller->ProcessJsMessage("getNotificationState", args1,
                                    reply_handler.AsWeakHandle());
  }

  // This forces the sync thread to process the message and reply.
  base::WaitableEvent done(false, false);
  harness_.service->GetBackendForTest()->GetSyncLoopForTesting()
      ->PostTask(FROM_HERE,
                 base::Bind(&SignalDone, &done));
  done.Wait();
  harness_.TearDown();
}

TEST_F(ProfileSyncServiceTest,
       JsControllerProcessJsMessageBasicDelayedBackendInitialization) {
  harness_.StartSyncServiceAndSetInitialSyncEnded(true, false, false, true,
                                                  syncer::STORAGE_IN_MEMORY);

  StrictMock<syncer::MockJsReplyHandler> reply_handler;

  ListValue arg_list1;
  arg_list1.Append(Value::CreateStringValue("INVALIDATIONS_ENABLED"));
  syncer::JsArgList args1(&arg_list1);
  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState", HasArgs(args1)));

  {
    base::WeakPtr<syncer::JsController> js_controller =
        harness_.service->GetJsController();
    js_controller->ProcessJsMessage("getNotificationState",
                                    args1, reply_handler.AsWeakHandle());
  }

  harness_.IssueTestTokens();
  harness_.WaitForBackendInitDone();

  // This forces the sync thread to process the message and reply.
  base::WaitableEvent done(false, false);
  harness_.service->GetBackendForTest()->GetSyncLoopForTesting()
      ->PostTask(FROM_HERE,
                 base::Bind(&SignalDone, &done));
  done.Wait();  harness_.TearDown();
}

// Make sure that things still work if sync is not enabled, but some old sync
// databases are lingering in the "Sync Data" folder.
TEST_F(ProfileSyncServiceTest, TestStartupWithOldSyncData) {
  const char* nonsense1 = "reginald";
  const char* nonsense2 = "beartato";
  const char* nonsense3 = "harrison";
  base::FilePath temp_directory =
      harness_.profile->GetPath().AppendASCII("Sync Data");
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

  harness_.StartSyncServiceAndSetInitialSyncEnded(true, false, true, false,
                                                  syncer::STORAGE_ON_DISK);
  EXPECT_FALSE(harness_.service->HasSyncSetupCompleted());
  EXPECT_FALSE(harness_.service->sync_initialized());

  // Since we're doing synchronous initialization, backend should be
  // initialized by this call.
  harness_.IssueTestTokens();

  // Stop the service so we can read the new Sync Data files that were
  // created.
  harness_.service->Shutdown();
  harness_.service.reset();

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
  harness_.StartSyncServiceAndSetInitialSyncEnded(false, true, true, true,
                                                  syncer::STORAGE_INVALID);

  // The backend is not ready.  Ensure the PSS knows this.
  EXPECT_FALSE(harness_.service->sync_initialized());
}

// This setup will allow the database to exist, but leave it empty.  The attempt
// to download control types will silently fail (no downloads have any effect in
// these tests).  The sync_backend_host will notice this and inform the profile
// sync service of the failure to initialize the backed.
TEST_F(ProfileSyncServiceTest, FailToDownloadControlTypes) {
  harness_.StartSyncServiceAndSetInitialSyncEnded(false, true, true, true,
                                                  syncer::STORAGE_IN_MEMORY);

  // The backend is not ready.  Ensure the PSS knows this.
  EXPECT_FALSE(harness_.service->sync_initialized());
}

}  // namespace
}  // namespace browser_sync
