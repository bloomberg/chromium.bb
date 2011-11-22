// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/js/js_arg_list.h"
#include "chrome/browser/sync/js/js_event_details.h"
#include "chrome/browser/sync/js/js_test_util.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/user_agent.h"
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
using testing::Return;
using testing::StrictMock;

class ProfileSyncServiceTest : public testing::Test {
 protected:
  ProfileSyncServiceTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        io_thread_(BrowserThread::IO) {}

  virtual ~ProfileSyncServiceTest() {}

  virtual void SetUp() {
    io_thread_.StartIOThread();
    profile_.reset(new TestingProfile());
    profile_->CreateRequestContext();

    // We need to set the user agent before the backend host can call
    // webkit_glue::GetUserAgent().
    chrome::VersionInfo version_info;
    std::string product("Chrome/");
    product += version_info.is_valid() ? version_info.Version() : "0.0.0.0";
    webkit_glue::SetUserAgent(webkit_glue::BuildUserAgentFromProduct(product),
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
    // Ensure that the sync objects destruct to avoid memory leaks.
    ui_loop_.RunAllPending();
  }

  // TODO(akalin): Refactor the StartSyncService*() functions below.

  void StartSyncService() {
    StartSyncServiceAndSetInitialSyncEnded(true, true, false, true, true);
  }

  void StartSyncServiceAndSetInitialSyncEnded(
      bool set_initial_sync_ended,
      bool issue_auth_token,
      bool synchronous_sync_configuration,
      bool sync_setup_completed,
      bool expect_create_dtm) {
    if (!service_.get()) {
      // Set bootstrap to true and it will provide a logged in user for test
      service_.reset(new TestProfileSyncService(&factory_,
                                                profile_.get(),
                                                "test", true, NULL));
      if (!set_initial_sync_ended)
        service_->dont_set_initial_sync_ended_on_init();
      if (synchronous_sync_configuration)
        service_->set_synchronous_sync_configuration();
      if (!sync_setup_completed)
        profile_->GetPrefs()->SetBoolean(prefs::kSyncHasSetupCompleted, false);

      if (expect_create_dtm) {
        // Register the bookmark data type.
        EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
            WillOnce(ReturnNewDataTypeManager());
      } else {
        EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).Times(0);
      }

      if (issue_auth_token) {
        profile_->GetTokenService()->IssueAuthTokenForTest(
            GaiaConstants::kSyncService, "token");
      }
      service_->Initialize();
    }
  }

  MessageLoop ui_loop_;
  // Needed by |service_|.
  content::TestBrowserThread ui_thread_;
  // Needed by |service| and |profile_|'s request context.
  content::TestBrowserThread io_thread_;

  scoped_ptr<TestProfileSyncService> service_;
  scoped_ptr<TestingProfile> profile_;
  ProfileSyncComponentsFactoryMock factory_;
};

TEST_F(ProfileSyncServiceTest, InitialState) {
  service_.reset(new TestProfileSyncService(&factory_, profile_.get(),
                                            "", true, NULL));
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
  service_.reset(new TestProfileSyncService(&factory_, profile_.get(),
                                            "", true, NULL));
  service_->Initialize();
  EXPECT_TRUE(service_->IsManaged());
}

TEST_F(ProfileSyncServiceTest, AbortedByShutdown) {
  service_.reset(new TestProfileSyncService(&factory_, profile_.get(),
                                            "test", true, NULL));
  EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).Times(0);
  EXPECT_CALL(factory_, CreateBookmarkSyncComponents(_, _)).Times(0);
  service_->RegisterDataTypeController(
      new BookmarkDataTypeController(&factory_,
                                     profile_.get(),
                                     service_.get()));

  service_->Initialize();
  service_.reset();
}

TEST_F(ProfileSyncServiceTest, DisableAndEnableSyncTemporarily) {
  service_.reset(new TestProfileSyncService(&factory_,
                                            profile_.get(),
                                            "test", true, NULL));
  // Register the bookmark data type.
  EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
      WillRepeatedly(ReturnNewDataTypeManager());

  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "token");

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

  JsController* js_controller = service_->GetJsController();
  StrictMock<MockJsEventHandler> event_handler;
  js_controller->AddJsEventHandler(&event_handler);
  js_controller->RemoveJsEventHandler(&event_handler);
}

TEST_F(ProfileSyncServiceTest,
       JsControllerHandlersDelayedBackendInitialization) {
  StartSyncServiceAndSetInitialSyncEnded(true, false, false, true, true);

  StrictMock<MockJsEventHandler> event_handler;
  EXPECT_CALL(event_handler, HandleJsEvent(_, _)).Times(AtLeast(1));

  EXPECT_EQ(NULL, service_->GetBackendForTest());
  EXPECT_FALSE(service_->sync_initialized());

  JsController* js_controller = service_->GetJsController();
  js_controller->AddJsEventHandler(&event_handler);
  // Since we're doing synchronous initialization, backend should be
  // initialized by this call.
  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service_->sync_initialized());
  js_controller->RemoveJsEventHandler(&event_handler);
}

TEST_F(ProfileSyncServiceTest, JsControllerProcessJsMessageBasic) {
  StartSyncService();

  StrictMock<MockJsReplyHandler> reply_handler;

  ListValue arg_list1;
  arg_list1.Append(Value::CreateBooleanValue(false));
  JsArgList args1(&arg_list1);
  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState", HasArgs(args1)));

  {
    JsController* js_controller = service_->GetJsController();
    js_controller->ProcessJsMessage("getNotificationState", args1,
                                    reply_handler.AsWeakHandle());
  }

  // This forces the sync thread to process the message and reply.
  service_.reset();
  ui_loop_.RunAllPending();
}

TEST_F(ProfileSyncServiceTest,
       JsControllerProcessJsMessageBasicDelayedBackendInitialization) {
  StartSyncServiceAndSetInitialSyncEnded(true, false, false, true, true);

  StrictMock<MockJsReplyHandler> reply_handler;

  ListValue arg_list1;
  arg_list1.Append(Value::CreateBooleanValue(false));
  JsArgList args1(&arg_list1);
  EXPECT_CALL(reply_handler,
              HandleJsReply("getNotificationState", HasArgs(args1)));

  {
    JsController* js_controller = service_->GetJsController();
    js_controller->ProcessJsMessage("getNotificationState",
                                    args1, reply_handler.AsWeakHandle());
  }

  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "token");

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

  StartSyncServiceAndSetInitialSyncEnded(false, false, true, false, true);
  EXPECT_FALSE(service_->HasSyncSetupCompleted());
  EXPECT_FALSE(service_->sync_initialized());

  // Since we're doing synchronous initialization, backend should be
  // initialized by this call.
  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "token");

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

TEST_F(ProfileSyncServiceTest, CorruptDatabase) {
  const char* nonesense = "not a database";

  FilePath temp_directory = profile_->GetPath().AppendASCII("Sync Data");
  FilePath sync_db_file = temp_directory.AppendASCII("SyncData.sqlite3");

  ASSERT_TRUE(file_util::CreateDirectory(temp_directory));
  ASSERT_NE(-1,
            file_util::WriteFile(sync_db_file, nonesense, strlen(nonesense)));

  // Initialize with HasSyncSetupCompleted() set to true and InitialSyncEnded
  // false.  This is to model the scenario that would result when opening the
  // sync database fails.
  StartSyncServiceAndSetInitialSyncEnded(false, true, true, true, false);

  // The backend is not ready.  Ensure the PSS knows this.
  EXPECT_FALSE(service_->sync_initialized());

  // Ensure we will be prepared to initialize a fresh DB next time.
  EXPECT_FALSE(service_->HasSyncSetupCompleted());
}

}  // namespace

}  // namespace browser_sync
