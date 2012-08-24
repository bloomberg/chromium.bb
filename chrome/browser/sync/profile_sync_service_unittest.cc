// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_browser_thread.h"
#include "google/cacheinvalidation/include/types.h"
#include "sync/js/js_arg_list.h"
#include "sync/js/js_event_details.h"
#include "sync/js/js_test_util.h"
#include "sync/notifier/mock_sync_notifier_observer.h"
#include "sync/notifier/object_id_state_map_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webkit_glue.h"

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

class ProfileSyncServiceTest : public testing::Test {
 protected:
  ProfileSyncServiceTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        db_thread_(BrowserThread::DB),
        file_thread_(BrowserThread::FILE),
        io_thread_(BrowserThread::IO) {}

  virtual ~ProfileSyncServiceTest() {}

  virtual void SetUp() {
    file_thread_.Start();
    io_thread_.StartIOThread();
    profile_.reset(new TestingProfile());
    profile_->CreateRequestContext();

    // We need to set the user agent before the backend host can call
    // webkit_glue::GetUserAgent().
    webkit_glue::SetUserAgent(content::GetContentClient()->GetUserAgent(),
                              false);
  }

  virtual void TearDown() {
    // Kill the service before the profile.
    service_.reset();
    profile_.reset();
    // Pump messages posted by the sync core thread (which may end up
    // posting on the IO thread).
    ui_loop_.RunAllPending();
    io_thread_.Stop();
    file_thread_.Stop();
    // Ensure that the sync objects destruct to avoid memory leaks.
    ui_loop_.RunAllPending();
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
    if (!service_.get()) {
      SigninManager* signin =
          SigninManagerFactory::GetForProfile(profile_.get());
      signin->SetAuthenticatedUsername("test");
      ProfileSyncComponentsFactoryMock* factory =
          new ProfileSyncComponentsFactoryMock();
      service_.reset(new TestProfileSyncService(
          factory,
          profile_.get(),
          signin,
          ProfileSyncService::AUTO_START,
          true,
          base::Closure()));
      if (!set_initial_sync_ended)
        service_->dont_set_initial_sync_ended_on_init();
      if (synchronous_sync_configuration)
        service_->set_synchronous_sync_configuration();
      service_->set_storage_option(storage_option);
      if (!sync_setup_completed)
        profile_->GetPrefs()->SetBoolean(prefs::kSyncHasSetupCompleted, false);

      // Register the bookmark data type.
      ON_CALL(*factory, CreateDataTypeManager(_, _, _)).
          WillByDefault(ReturnNewDataTypeManager());

      if (issue_auth_token) {
        IssueTestTokens();
      }
      service_->Initialize();
    }
  }

  void IssueTestTokens() {
    TokenService* token_service =
        TokenServiceFactory::GetForProfile(profile_.get());
    token_service->IssueAuthTokenForTest(
        GaiaConstants::kSyncService, "token1");
    token_service->IssueAuthTokenForTest(
        GaiaConstants::kGaiaOAuth2LoginRefreshToken, "token2");
  }

  MessageLoop ui_loop_;
  // Needed by |service_|.
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  // Needed by DisableAndEnableSyncTemporarily test case.
  content::TestBrowserThread file_thread_;
  // Needed by |service| and |profile_|'s request context.
  content::TestBrowserThread io_thread_;

  scoped_ptr<TestProfileSyncService> service_;
  scoped_ptr<TestingProfile> profile_;
};

TEST_F(ProfileSyncServiceTest, InitialState) {
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_.get());
  service_.reset(new TestProfileSyncService(
      new ProfileSyncComponentsFactoryMock(),
      profile_.get(),
      signin,
      ProfileSyncService::MANUAL_START,
      true,
      base::Closure()));
  EXPECT_TRUE(
      service_->sync_service_url().spec() ==
        ProfileSyncService::kSyncServerUrl ||
      service_->sync_service_url().spec() ==
        ProfileSyncService::kDevServerUrl);
}

TEST_F(ProfileSyncServiceTest, DisabledByPolicy) {
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kSyncManaged,
      Value::CreateBooleanValue(true));
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_.get());
  service_.reset(new TestProfileSyncService(
      new ProfileSyncComponentsFactoryMock(),
      profile_.get(),
      signin,
      ProfileSyncService::MANUAL_START,
      true,
      base::Closure()));
  service_->Initialize();
  EXPECT_TRUE(service_->IsManaged());
}

TEST_F(ProfileSyncServiceTest, AbortedByShutdown) {
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_.get());
  signin->SetAuthenticatedUsername("test");
  ProfileSyncComponentsFactoryMock* factory =
      new ProfileSyncComponentsFactoryMock();
  service_.reset(new TestProfileSyncService(
      factory,
      profile_.get(),
      signin,
      ProfileSyncService::AUTO_START,
      true,
      base::Closure()));
  EXPECT_CALL(*factory, CreateDataTypeManager(_, _, _)).Times(0);
  EXPECT_CALL(*factory, CreateBookmarkSyncComponents(_, _)).
      Times(0);
  service_->RegisterDataTypeController(
      new BookmarkDataTypeController(service_->factory(),
                                     profile_.get(),
                                     service_.get()));

  service_->Initialize();
  service_.reset();
}

TEST_F(ProfileSyncServiceTest, DisableAndEnableSyncTemporarily) {
  SigninManager* signin = SigninManagerFactory::GetForProfile(profile_.get());
  signin->SetAuthenticatedUsername("test");
  ProfileSyncComponentsFactoryMock* factory =
      new ProfileSyncComponentsFactoryMock();
  service_.reset(new TestProfileSyncService(
      factory,
      profile_.get(),
      signin,
      ProfileSyncService::AUTO_START,
      true,
      base::Closure()));
  // Register the bookmark data type.
  EXPECT_CALL(*factory, CreateDataTypeManager(_, _, _)).
      WillRepeatedly(ReturnNewDataTypeManager());

  IssueTestTokens();

  service_->Initialize();
  EXPECT_TRUE(service_->sync_initialized());
  EXPECT_TRUE(service_->GetBackendForTest() != NULL);
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));

  service_->StopAndSuppress();
  EXPECT_FALSE(service_->sync_initialized());
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));

  service_->UnsuppressAndStart();
  EXPECT_TRUE(service_->sync_initialized());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kSyncSuppressStart));
}

TEST_F(ProfileSyncServiceTest, JsControllerHandlersBasic) {
  StartSyncService();
  EXPECT_TRUE(service_->sync_initialized());
  EXPECT_TRUE(service_->GetBackendForTest() != NULL);

  syncer::JsController* js_controller = service_->GetJsController();
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

  syncer::JsController* js_controller = service_->GetJsController();
  js_controller->AddJsEventHandler(&event_handler);
  // Since we're doing synchronous initialization, backend should be
  // initialized by this call.
  IssueTestTokens();
  EXPECT_TRUE(service_->sync_initialized());
  js_controller->RemoveJsEventHandler(&event_handler);
}

TEST_F(ProfileSyncServiceTest, JsControllerProcessJsMessageBasic) {
  StartSyncService();

  StrictMock<syncer::MockJsReplyHandler> reply_handler;

  ListValue arg_list1;
  arg_list1.Append(Value::CreateStringValue("TRANSIENT_NOTIFICATION_ERROR"));
  syncer::JsArgList args1(&arg_list1);
  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState", HasArgs(args1)));

  {
    syncer::JsController* js_controller = service_->GetJsController();
    js_controller->ProcessJsMessage("getNotificationState", args1,
                                    reply_handler.AsWeakHandle());
  }

  // This forces the sync thread to process the message and reply.
  service_.reset();
  ui_loop_.RunAllPending();
}

TEST_F(ProfileSyncServiceTest,
       JsControllerProcessJsMessageBasicDelayedBackendInitialization) {
  StartSyncServiceAndSetInitialSyncEnded(true, false, false, true,
                                         syncer::STORAGE_IN_MEMORY);

  StrictMock<syncer::MockJsReplyHandler> reply_handler;

  ListValue arg_list1;
  arg_list1.Append(Value::CreateStringValue("TRANSIENT_NOTIFICATION_ERROR"));
  syncer::JsArgList args1(&arg_list1);
  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState", HasArgs(args1)));

  {
    syncer::JsController* js_controller = service_->GetJsController();
    js_controller->ProcessJsMessage("getNotificationState",
                                    args1, reply_handler.AsWeakHandle());
  }

  IssueTestTokens();

  // This forces the sync thread to process the message and reply.
  service_.reset();
  ui_loop_.RunAllPending();
}

// Make sure that things still work if sync is not enabled, but some old sync
// databases are lingering in the "Sync Data" folder.
TEST_F(ProfileSyncServiceTest, TestStartupWithOldSyncData) {
  const char* nonsense1 = "reginald";
  const char* nonsense2 = "beartato";
  const char* nonsense3 = "harrison";
  FilePath temp_directory = profile_->GetPath().AppendASCII("Sync Data");
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

  StartSyncServiceAndSetInitialSyncEnded(false, false, true, false,
                                         syncer::STORAGE_ON_DISK);
  EXPECT_FALSE(service_->HasSyncSetupCompleted());
  EXPECT_FALSE(service_->sync_initialized());

  // Since we're doing synchronous initialization, backend should be
  // initialized by this call.
  IssueTestTokens();

  // Stop the service so we can read the new Sync Data files that were
  // created.
  service_.reset();

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
  StartSyncServiceAndSetInitialSyncEnded(false, true, true, true,
                                         syncer::STORAGE_INVALID);

  // The backend is not ready.  Ensure the PSS knows this.
  EXPECT_FALSE(service_->sync_initialized());
}

// Register for some IDs with the ProfileSyncService and trigger some
// invalidation messages.  They should be received by the observer.
// Then unregister and trigger the invalidation messages again.  Those
// shouldn't be received by the observer.
TEST_F(ProfileSyncServiceTest, UpdateRegisteredInvalidationIds) {
  StartSyncService();

  syncer::ObjectIdSet ids;
  ids.insert(invalidation::ObjectId(1, "id1"));
  ids.insert(invalidation::ObjectId(2, "id2"));
  const syncer::ObjectIdStateMap& states =
      syncer::ObjectIdSetToStateMap(ids, "payload");

  StrictMock<syncer::MockSyncNotifierObserver> observer;
  EXPECT_CALL(observer, OnNotificationsEnabled());
  EXPECT_CALL(observer, OnIncomingNotification(
      states, syncer::REMOTE_NOTIFICATION));
  EXPECT_CALL(observer, OnNotificationsDisabled(
      syncer::TRANSIENT_NOTIFICATION_ERROR));

  service_->RegisterInvalidationHandler(&observer);
  service_->UpdateRegisteredInvalidationIds(&observer, ids);

  SyncBackendHostForProfileSyncTest* const backend =
      service_->GetBackendForTest();

  backend->EmitOnNotificationsEnabled();
  backend->EmitOnIncomingNotification(states, syncer::REMOTE_NOTIFICATION);
  backend->EmitOnNotificationsDisabled(syncer::TRANSIENT_NOTIFICATION_ERROR);

  Mock::VerifyAndClearExpectations(&observer);

  service_->UnregisterInvalidationHandler(&observer);

  backend->EmitOnNotificationsEnabled();
  backend->EmitOnIncomingNotification(states, syncer::REMOTE_NOTIFICATION);
  backend->EmitOnNotificationsDisabled(syncer::TRANSIENT_NOTIFICATION_ERROR);
}

// Register for some IDs with the ProfileSyncService, restart sync,
// and trigger some invalidation messages.  They should still be
// received by the observer.
TEST_F(ProfileSyncServiceTest, UpdateRegisteredInvalidationIdsPersistence) {
  StartSyncService();

  syncer::ObjectIdSet ids;
  ids.insert(invalidation::ObjectId(3, "id3"));
  const syncer::ObjectIdStateMap& states =
      syncer::ObjectIdSetToStateMap(ids, "payload");

  StrictMock<syncer::MockSyncNotifierObserver> observer;
  EXPECT_CALL(observer, OnNotificationsEnabled());
  EXPECT_CALL(observer, OnIncomingNotification(
      states, syncer::REMOTE_NOTIFICATION));
  // This may get called more than once, as a real notifier is
  // created.
  EXPECT_CALL(observer, OnNotificationsDisabled(
      syncer::TRANSIENT_NOTIFICATION_ERROR)).Times(AtLeast(1));

  service_->RegisterInvalidationHandler(&observer);
  service_->UpdateRegisteredInvalidationIds(&observer, ids);

  service_->StopAndSuppress();
  service_->UnsuppressAndStart();

  SyncBackendHostForProfileSyncTest* const backend =
      service_->GetBackendForTest();

  backend->EmitOnNotificationsEnabled();
  backend->EmitOnIncomingNotification(states, syncer::REMOTE_NOTIFICATION);
  backend->EmitOnNotificationsDisabled(syncer::TRANSIENT_NOTIFICATION_ERROR);
}

}  // namespace
}  // namespace browser_sync
