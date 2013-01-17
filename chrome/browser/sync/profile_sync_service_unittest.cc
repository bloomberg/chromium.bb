// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
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
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_browser_thread.h"
#include "google/cacheinvalidation/include/types.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/js/js_arg_list.h"
#include "sync/js/js_event_details.h"
#include "sync/js/js_test_util.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/invalidator.h"
#include "sync/notifier/invalidator_test_template.h"
#include "sync/notifier/object_id_invalidation_map_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/user_agent/user_agent.h"

// TODO(akalin): Add tests here that exercise the whole
// ProfileSyncService/SyncBackendHost stack while mocking out as
// little as possible.

namespace browser_sync {

namespace {

using content::BrowserThread;
using testing::_;
using testing::AtLeast;
using testing::AtMost;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

class ProfileSyncServiceTestHarness {
 public:
  ProfileSyncServiceTestHarness()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        db_thread_(BrowserThread::DB),
        file_thread_(BrowserThread::FILE),
        io_thread_(BrowserThread::IO) {}

  ~ProfileSyncServiceTestHarness() {}

  void SetUp() {
    file_thread_.Start();
    io_thread_.StartIOThread();
    profile.reset(new TestingProfile());
    profile->CreateRequestContext();

    // We need to set the user agent before the backend host can call
    // webkit_glue::GetUserAgent().
    webkit_glue::SetUserAgent(content::GetContentClient()->GetUserAgent(),
                              false);
  }

  void TearDown() {
    // Kill the service before the profile.
    if (service.get()) {
      service->Shutdown();
    }
    service.reset();
    profile.reset();
    // Pump messages posted by the sync thread (which may end up
    // posting on the IO thread).
    ui_loop_.RunUntilIdle();
    io_thread_.Stop();
    file_thread_.Stop();
    // Ensure that the sync objects destruct to avoid memory leaks.
    ui_loop_.RunUntilIdle();
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
    if (!service.get()) {
      SigninManager* signin =
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
      ON_CALL(*factory, CreateDataTypeManager(_, _, _, _)).
          WillByDefault(ReturnNewDataTypeManager());

      if (issue_auth_token) {
        IssueTestTokens();
      }
      service->Initialize();
    }
  }

  void IssueTestTokens() {
    TokenService* token_service =
        TokenServiceFactory::GetForProfile(profile.get());
    token_service->IssueAuthTokenForTest(
        GaiaConstants::kSyncService, "token1");
    token_service->IssueAuthTokenForTest(
        GaiaConstants::kGaiaOAuth2LoginRefreshToken, "token2");
  }

  scoped_ptr<TestProfileSyncService> service;
  scoped_ptr<TestingProfile> profile;

 private:
  MessageLoop ui_loop_;
  // Needed by |service|.
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  // Needed by DisableAndEnableSyncTemporarily test case.
  content::TestBrowserThread file_thread_;
  // Needed by |service| and |profile|'s request context.
  content::TestBrowserThread io_thread_;
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
  SigninManager* signin =
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
  SigninManager* signin =
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
  SigninManager* signin =
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
  EXPECT_CALL(*factory, CreateDataTypeManager(_, _, _, _)).Times(0);
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
  SigninManager* signin =
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
  EXPECT_CALL(*factory, CreateDataTypeManager(_, _, _, _)).
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

TEST_F(ProfileSyncServiceTest, JsControllerHandlersBasic) {
  harness_.StartSyncService();
  EXPECT_TRUE(harness_.service->sync_initialized());
  EXPECT_TRUE(harness_.service->GetBackendForTest() != NULL);

  syncer::JsController* js_controller = harness_.service->GetJsController();
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

  syncer::JsController* js_controller = harness_.service->GetJsController();
  js_controller->AddJsEventHandler(&event_handler);
  // Since we're doing synchronous initialization, backend should be
  // initialized by this call.
  harness_.IssueTestTokens();
  EXPECT_TRUE(harness_.service->sync_initialized());
  js_controller->RemoveJsEventHandler(&event_handler);
}

TEST_F(ProfileSyncServiceTest, JsControllerProcessJsMessageBasic) {
  harness_.StartSyncService();

  StrictMock<syncer::MockJsReplyHandler> reply_handler;

  ListValue arg_list1;
  arg_list1.Append(Value::CreateStringValue("TRANSIENT_INVALIDATION_ERROR"));
  syncer::JsArgList args1(&arg_list1);
  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState", HasArgs(args1)));

  {
    syncer::JsController* js_controller = harness_.service->GetJsController();
    js_controller->ProcessJsMessage("getNotificationState", args1,
                                    reply_handler.AsWeakHandle());
  }

  // This forces the sync thread to process the message and reply.
  harness_.TearDown();
}

TEST_F(ProfileSyncServiceTest,
       JsControllerProcessJsMessageBasicDelayedBackendInitialization) {
  harness_.StartSyncServiceAndSetInitialSyncEnded(true, false, false, true,
                                                  syncer::STORAGE_IN_MEMORY);

  StrictMock<syncer::MockJsReplyHandler> reply_handler;

  ListValue arg_list1;
  arg_list1.Append(Value::CreateStringValue("TRANSIENT_INVALIDATION_ERROR"));
  syncer::JsArgList args1(&arg_list1);
  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState", HasArgs(args1)));

  {
    syncer::JsController* js_controller = harness_.service->GetJsController();
    js_controller->ProcessJsMessage("getNotificationState",
                                    args1, reply_handler.AsWeakHandle());
  }

  harness_.IssueTestTokens();

  // This forces the sync thread to process the message and reply.
  harness_.TearDown();
}

// Make sure that things still work if sync is not enabled, but some old sync
// databases are lingering in the "Sync Data" folder.
TEST_F(ProfileSyncServiceTest, TestStartupWithOldSyncData) {
  const char* nonsense1 = "reginald";
  const char* nonsense2 = "beartato";
  const char* nonsense3 = "harrison";
  FilePath temp_directory =
      harness_.profile->GetPath().AppendASCII("Sync Data");
  FilePath sync_file1 =
      temp_directory.AppendASCII("BookmarkSyncSettings.sqlite3");
  FilePath sync_file2 = temp_directory.AppendASCII("SyncData.sqlite3");
  FilePath sync_file3 = temp_directory.AppendASCII("nonsense_file");
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
  ASSERT_FALSE(file_util::PathExists(sync_file3));
  ASSERT_FALSE(file_util::PathExists(sync_file1));

  // This will still exist, but the text should have changed.
  ASSERT_TRUE(file_util::PathExists(sync_file2));
  std::string file2text;
  ASSERT_TRUE(file_util::ReadFileToString(sync_file2, &file2text));
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

// Register a handler with the ProfileSyncService, and disable and
// reenable sync.  The handler should get notified of the state
// changes.
// Flaky on all platforms. http://crbug.com/154491
TEST_F(ProfileSyncServiceTest, DISABLED_DisableInvalidationsOnStop) {
  harness_.StartSyncServiceAndSetInitialSyncEnded(
      true, true, true, true, syncer::STORAGE_IN_MEMORY);

  syncer::FakeInvalidationHandler handler;
  harness_.service->RegisterInvalidationHandler(&handler);

  SyncBackendHostForProfileSyncTest* const backend =
      harness_.service->GetBackendForTest();

  backend->EmitOnInvalidatorStateChange(syncer::INVALIDATIONS_ENABLED);
  EXPECT_EQ(syncer::INVALIDATIONS_ENABLED, handler.GetInvalidatorState());

  harness_.service->StopAndSuppress();
  EXPECT_EQ(syncer::TRANSIENT_INVALIDATION_ERROR,
            handler.GetInvalidatorState());

  harness_.service->UnsuppressAndStart();
  EXPECT_EQ(syncer::INVALIDATIONS_ENABLED, handler.GetInvalidatorState());

  harness_.service->UnregisterInvalidationHandler(&handler);
}

// Register for some IDs with the ProfileSyncService, restart sync,
// and trigger some invalidation messages.  They should still be
// received by the handler.
TEST_F(ProfileSyncServiceTest, UpdateRegisteredInvalidationIdsPersistence) {
  harness_.StartSyncService();

  syncer::ObjectIdSet ids;
  ids.insert(invalidation::ObjectId(3, "id3"));
  const syncer::ObjectIdInvalidationMap& states =
      syncer::ObjectIdSetToInvalidationMap(ids, "payload");

  syncer::FakeInvalidationHandler handler;

  harness_.service->RegisterInvalidationHandler(&handler);
  harness_.service->UpdateRegisteredInvalidationIds(&handler, ids);

  harness_.service->StopAndSuppress();
  harness_.service->UnsuppressAndStart();

  SyncBackendHostForProfileSyncTest* const backend =
      harness_.service->GetBackendForTest();

  backend->EmitOnInvalidatorStateChange(syncer::INVALIDATIONS_ENABLED);
  EXPECT_EQ(syncer::INVALIDATIONS_ENABLED, handler.GetInvalidatorState());

  backend->EmitOnIncomingInvalidation(states, syncer::REMOTE_INVALIDATION);
  EXPECT_THAT(states, Eq(handler.GetLastInvalidationMap()));
  EXPECT_EQ(syncer::REMOTE_INVALIDATION, handler.GetLastInvalidationSource());

  backend->EmitOnInvalidatorStateChange(syncer::TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(syncer::TRANSIENT_INVALIDATION_ERROR,
            handler.GetInvalidatorState());

  harness_.service->UnregisterInvalidationHandler(&handler);
}

// Thin Invalidator wrapper around ProfileSyncService.
class ProfileSyncServiceInvalidator : public syncer::Invalidator {
 public:
  explicit ProfileSyncServiceInvalidator(ProfileSyncService* service)
      : service_(service) {}

  virtual ~ProfileSyncServiceInvalidator() {}

  // Invalidator implementation.
  virtual void RegisterHandler(syncer::InvalidationHandler* handler) OVERRIDE {
    service_->RegisterInvalidationHandler(handler);
  }

  virtual void UpdateRegisteredIds(syncer::InvalidationHandler* handler,
                                   const syncer::ObjectIdSet& ids) OVERRIDE {
    service_->UpdateRegisteredInvalidationIds(handler, ids);
  }

  virtual void UnregisterHandler(
      syncer::InvalidationHandler* handler) OVERRIDE {
    service_->UnregisterInvalidationHandler(handler);
  }

  virtual syncer::InvalidatorState GetInvalidatorState() const OVERRIDE {
    return service_->GetInvalidatorState();
  }

  virtual void SetUniqueId(const std::string& unique_id) OVERRIDE {
    // Do nothing.
  }

  virtual void SetStateDeprecated(const std::string& state) OVERRIDE {
    // Do nothing.
  }

  virtual void UpdateCredentials(
      const std::string& email, const std::string& token) OVERRIDE {
    // Do nothing.
  }

  virtual void SendInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) OVERRIDE {
    // Do nothing.
  }

 private:
  ProfileSyncService* const service_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceInvalidator);
};

}  // namespace


// ProfileSyncServiceInvalidatorTestDelegate has to be visible from
// the syncer namespace (where InvalidatorTest lives).
class ProfileSyncServiceInvalidatorTestDelegate {
 public:
  ProfileSyncServiceInvalidatorTestDelegate() {}

  ~ProfileSyncServiceInvalidatorTestDelegate() {
    DestroyInvalidator();
  }

  void CreateInvalidator(
      const std::string& initial_state,
      const base::WeakPtr<syncer::InvalidationStateTracker>&
          invalidation_state_tracker) {
    DCHECK(!invalidator_.get());
    harness_.SetUp();
    harness_.StartSyncService();
    invalidator_.reset(
        new ProfileSyncServiceInvalidator(harness_.service.get()));
  }

  ProfileSyncServiceInvalidator* GetInvalidator() {
    return invalidator_.get();
  }

  void DestroyInvalidator() {
    invalidator_.reset();
    harness_.TearDown();
  }

  void WaitForInvalidator() {
    // Do nothing.
  }

  void TriggerOnInvalidatorStateChange(syncer::InvalidatorState state) {
    harness_.service->GetBackendForTest()->EmitOnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map,
      syncer::IncomingInvalidationSource source) {
    harness_.service->GetBackendForTest()->EmitOnIncomingInvalidation(
        invalidation_map, source);
  }

  static bool InvalidatorHandlesDeprecatedState() {
    return false;
  }

 private:
  ProfileSyncServiceTestHarness harness_;
  scoped_ptr<ProfileSyncServiceInvalidator> invalidator_;
};

}  // namespace browser_sync

namespace syncer {
namespace {

// ProfileSyncService should behave just like an invalidator.
INSTANTIATE_TYPED_TEST_CASE_P(
    ProfileSyncServiceInvalidatorTest, InvalidatorTest,
    ::browser_sync::ProfileSyncServiceInvalidatorTestDelegate);

}  // namespace
}  // namespace syncer
