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
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_test_util.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
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
using testing::Return;
using testing::StrictMock;

class ProfileSyncServiceTest : public testing::Test {
 protected:
  ProfileSyncServiceTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
    profile_.reset(new TestingProfile());
  }
  virtual ~ProfileSyncServiceTest() {
    // Kill the service before the profile.
    service_.reset();
    profile_.reset();

    // Ensure that the sync objects destruct to avoid memory leaks.
    MessageLoop::current()->RunAllPending();
  }
  virtual void SetUp() {
    profile_->CreateRequestContext();
  }
  virtual void TearDown() {
    {
      // The request context gets deleted on the I/O thread. To prevent a leak
      // supply one here.
      BrowserThread io_thread(BrowserThread::IO, MessageLoop::current());
      profile_->ResetRequestContext();
    }
    MessageLoop::current()->RunAllPending();
  }

  // TODO(akalin): Refactor the StartSyncService*() functions below.

  void StartSyncService() {
    StartSyncServiceAndSetInitialSyncEnded(true, true, false, true);
  }

  void StartSyncServiceAndSetInitialSyncEnded(
      bool set_initial_sync_ended,
      bool issue_auth_token,
      bool synchronous_sync_configuration,
      bool sync_setup_completed) {
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

      // Register the bookmark data type.
      EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
          WillOnce(ReturnNewDataTypeManager());

      if (issue_auth_token) {
        profile_->GetTokenService()->IssueAuthTokenForTest(
            GaiaConstants::kSyncService, "token");
      }
      service_->Initialize();
    }
  }

  // This serves as the "UI loop" on which the ProfileSyncService lives and
  // operates. It is needed because the SyncBackend can post tasks back to
  // the service, meaning it can't be null. It doesn't have to be running,
  // though -- OnInitializationCompleted is the only example (so far) in this
  // test where we need to Run the loop to swallow a task and then quit, to
  // avoid leaking the ProfileSyncService (the PostTask will retain the callee
  // and caller until the task is run).
  MessageLoop message_loop_;
  BrowserThread ui_thread_;

  scoped_ptr<TestProfileSyncService> service_;
  scoped_ptr<TestingProfile> profile_;
  ProfileSyncFactoryMock factory_;
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
#if defined(OS_CHROMEOS) && defined(GOOGLE_CHROME_BUILD)
#define MAYBE_JsFrontendHandlersBasic DISABLED_JsFrontendHandlersBasic
#else
#define MAYBE_JsFrontendHandlersBasic JsFrontendHandlersBasic
#endif
TEST_F(ProfileSyncServiceTest, MAYBE_JsFrontendHandlersBasic) {
  StartSyncService();

  StrictMock<MockJsEventHandler> event_handler;

  SyncBackendHostForProfileSyncTest* test_backend =
      service_->GetBackendForTest();

  EXPECT_TRUE(service_->sync_initialized());
  ASSERT_TRUE(test_backend != NULL);
  ASSERT_TRUE(test_backend->GetJsBackend() != NULL);
  EXPECT_EQ(NULL, test_backend->GetJsBackend()->GetParentJsEventRouter());

  JsFrontend* js_backend = service_->GetJsFrontend();
  js_backend->AddHandler(&event_handler);
  ASSERT_TRUE(test_backend->GetJsBackend() != NULL);
  EXPECT_TRUE(test_backend->GetJsBackend()->GetParentJsEventRouter() != NULL);

  js_backend->RemoveHandler(&event_handler);
  EXPECT_EQ(NULL, test_backend->GetJsBackend()->GetParentJsEventRouter());
}

TEST_F(ProfileSyncServiceTest,
       JsFrontendHandlersDelayedBackendInitialization) {
  StartSyncServiceAndSetInitialSyncEnded(true, false, false, true);

  StrictMock<MockJsEventHandler> event_handler;
  EXPECT_CALL(event_handler,
              HandleJsEvent("onSyncServiceStateChanged",
                            HasArgs(JsArgList()))).Times(AtLeast(3));
  // For some reason, these events may or may not fire.
  //
  // TODO(akalin): Figure out exactly why there's non-determinism
  // here, and if possible remove it.
  EXPECT_CALL(event_handler, HandleJsEvent("onChangesApplied", _))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler, HandleJsEvent("onChangesComplete", _))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler, HandleJsEvent("onSyncNotificationStateChange", _))
      .Times(AtMost(1));

  EXPECT_EQ(NULL, service_->GetBackendForTest());
  EXPECT_FALSE(service_->sync_initialized());

  JsFrontend* js_backend = service_->GetJsFrontend();
  js_backend->AddHandler(&event_handler);
  // Since we're doing synchronous initialization, backend should be
  // initialized by this call.
  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "token");

  SyncBackendHostForProfileSyncTest* test_backend =
      service_->GetBackendForTest();

  EXPECT_TRUE(service_->sync_initialized());
  ASSERT_TRUE(test_backend != NULL);
  ASSERT_TRUE(test_backend->GetJsBackend() != NULL);
  EXPECT_TRUE(test_backend->GetJsBackend()->GetParentJsEventRouter() != NULL);

  js_backend->RemoveHandler(&event_handler);
  EXPECT_EQ(NULL, test_backend->GetJsBackend()->GetParentJsEventRouter());
}

TEST_F(ProfileSyncServiceTest, JsFrontendProcessMessageBasic) {
  StartSyncService();

  StrictMock<MockJsEventHandler> event_handler;
  // For some reason, these events may or may not fire.
  EXPECT_CALL(event_handler, HandleJsEvent("onChangesApplied", _))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler, HandleJsEvent("onChangesComplete", _))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler, HandleJsEvent("onSyncNotificationStateChange", _))
      .Times(AtMost(1));

  ListValue arg_list1;
  arg_list1.Append(Value::CreateBooleanValue(true));
  arg_list1.Append(Value::CreateIntegerValue(5));
  JsArgList args1(arg_list1);
  EXPECT_CALL(event_handler, HandleJsEvent("testMessage1", HasArgs(args1)));

  ListValue arg_list2;
  arg_list2.Append(Value::CreateStringValue("test"));
  arg_list2.Append(arg_list1.DeepCopy());
  JsArgList args2(arg_list2);
  EXPECT_CALL(event_handler,
              HandleJsEvent("delayTestMessage2", HasArgs(args2)));

  ListValue arg_list3;
  arg_list3.Append(arg_list1.DeepCopy());
  arg_list3.Append(arg_list2.DeepCopy());
  JsArgList args3(arg_list3);

  JsFrontend* js_backend = service_->GetJsFrontend();

  // Never replied to.
  js_backend->ProcessMessage("notRepliedTo", args3, &event_handler);

  // Replied to later.
  js_backend->ProcessMessage("delayTestMessage2", args2, &event_handler);

  js_backend->AddHandler(&event_handler);

  // Replied to immediately.
  js_backend->ProcessMessage("testMessage1", args1, &event_handler);

  // Fires off reply for delayTestMessage2.
  message_loop_.RunAllPending();

  // Never replied to.
  js_backend->ProcessMessage("delayNotRepliedTo", args3, &event_handler);

  js_backend->RemoveHandler(&event_handler);

  message_loop_.RunAllPending();

  // Never replied to.
  js_backend->ProcessMessage("notRepliedTo", args3, &event_handler);
}

TEST_F(ProfileSyncServiceTest,
       JsFrontendProcessMessageBasicDelayedBackendInitialization) {
  StartSyncServiceAndSetInitialSyncEnded(true, false, false, true);

  StrictMock<MockJsEventHandler> event_handler;
  // For some reason, these events may or may not fire.
  EXPECT_CALL(event_handler, HandleJsEvent("onChangesApplied", _))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler, HandleJsEvent("onChangesComplete", _))
      .Times(AtMost(1));
  EXPECT_CALL(event_handler, HandleJsEvent("onSyncNotificationStateChange", _))
      .Times(AtMost(1));

  ListValue arg_list1;
  arg_list1.Append(Value::CreateBooleanValue(true));
  arg_list1.Append(Value::CreateIntegerValue(5));
  JsArgList args1(arg_list1);
  EXPECT_CALL(event_handler, HandleJsEvent("testMessage1", HasArgs(args1)));

  ListValue arg_list2;
  arg_list2.Append(Value::CreateStringValue("test"));
  arg_list2.Append(arg_list1.DeepCopy());
  JsArgList args2(arg_list2);
  EXPECT_CALL(event_handler, HandleJsEvent("testMessage2", HasArgs(args2)));

  ListValue arg_list3;
  arg_list3.Append(arg_list1.DeepCopy());
  arg_list3.Append(arg_list2.DeepCopy());
  JsArgList args3(arg_list3);
  EXPECT_CALL(event_handler,
              HandleJsEvent("delayTestMessage3", HasArgs(args3)));

  const JsArgList kNoArgs;

  EXPECT_CALL(event_handler, HandleJsEvent("onSyncServiceStateChanged",
      HasArgs(kNoArgs))).Times(AtLeast(3));

  JsFrontend* js_backend = service_->GetJsFrontend();

  // We expect a reply for this message, even though its sent before
  // |event_handler| is added as a handler.
  js_backend->ProcessMessage("testMessage1", args1, &event_handler);

  js_backend->AddHandler(&event_handler);

  js_backend->ProcessMessage("testMessage2", args2, &event_handler);
  js_backend->ProcessMessage("delayTestMessage3", args3, &event_handler);

  // Fires testMessage1 and testMessage2.
  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "token");

  // Fires delayTestMessage3.
  message_loop_.RunAllPending();

  js_backend->ProcessMessage("delayNotRepliedTo", kNoArgs, &event_handler);

  js_backend->RemoveHandler(&event_handler);

  message_loop_.RunAllPending();

  js_backend->ProcessMessage("notRepliedTo", kNoArgs, &event_handler);
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

  StartSyncServiceAndSetInitialSyncEnded(false, false, true, false);
  EXPECT_FALSE(service_->HasSyncSetupCompleted());

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

}  // namespace

}  // namespace browser_sync
