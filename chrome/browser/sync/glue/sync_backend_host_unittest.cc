// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_host.h"

#include <cstddef>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/test/base/testing_browser_process_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/test_url_request_context_getter.h"
#include "content/browser/browser_thread.h"
#include "content/common/url_fetcher.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(akalin): Remove this once we fix the TODO below.
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

namespace browser_sync {

namespace {

class MockSyncFrontend : public SyncFrontend {
 public:
  virtual ~MockSyncFrontend() {}

  MOCK_METHOD2(OnBackendInitialized, void(const WeakHandle<JsBackend>&, bool));
  MOCK_METHOD0(OnSyncCycleCompleted, void());
  MOCK_METHOD0(OnAuthError, void());
  MOCK_METHOD0(OnStopSyncingPermanently, void());
  MOCK_METHOD0(OnClearServerDataSucceeded, void());
  MOCK_METHOD0(OnClearServerDataFailed, void());
  MOCK_METHOD1(OnPassphraseRequired, void(sync_api::PassphraseRequiredReason));
  MOCK_METHOD0(OnPassphraseAccepted, void());
  MOCK_METHOD1(OnEncryptionComplete, void(const syncable::ModelTypeSet&));
  MOCK_METHOD1(OnMigrationNeededForTypes, void(const syncable::ModelTypeSet&));
};

}  // namespace

class SyncBackendHostTest : public TestingBrowserProcessTest {
 protected:
  SyncBackendHostTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        io_thread_(BrowserThread::IO) {}

  virtual ~SyncBackendHostTest() {}

  virtual void SetUp() {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
  }

  virtual void TearDown() {
    // Pump messages posted by the sync core thread (which may end up
    // posting on the IO thread).
    ui_loop_.RunAllPending();
    io_thread_.Stop();
    // Pump any messages posted by the IO thread.
    ui_loop_.RunAllPending();
  }

 private:
  MessageLoop ui_loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;
};

TEST_F(SyncBackendHostTest, InitShutdown) {
  std::string k_mock_url = "http://www.example.com";
  FakeURLFetcherFactory test_factory_;
  test_factory_.SetFakeResponse(k_mock_url + "/time?command=get_time", "",
      false);

  TestingProfile profile;
  profile.CreateRequestContext();

  SyncBackendHost backend(&profile);

  // TODO(akalin): Handle this in SyncBackendHost instead of in
  // ProfileSyncService, or maybe figure out a way to share the
  // "register sync prefs" code.
  PrefService* pref_service = profile.GetPrefs();
  pref_service->RegisterStringPref(prefs::kEncryptionBootstrapToken, "");
  pref_service->RegisterBooleanPref(prefs::kSyncHasSetupCompleted,
                                    false,
                                    PrefService::UNSYNCABLE_PREF);

  MockSyncFrontend mock_frontend;
  sync_api::SyncCredentials credentials;
  credentials.email = "user@example.com";
  credentials.sync_token = "sync_token";
  backend.Initialize(&mock_frontend,
                     WeakHandle<JsEventHandler>(),
                     GURL(k_mock_url),
                     syncable::ModelTypeSet(),
                     credentials,
                     true);
  backend.Shutdown(false);
}

TEST_F(SyncBackendHostTest, GetPendingConfigModeState) {
  SyncBackendHost::PendingConfigureDataTypesState state;

  // Empty.
  {
    syncable::ModelTypeSet types_to_add;
    syncable::ModelTypeSet types_to_remove;
    ModelSafeRoutingInfo routing_info;

    SyncBackendHost::GetPendingConfigModeState(
        types_to_add, types_to_remove, base::Callback<void(bool)>(),
        &routing_info, sync_api::CONFIGURE_REASON_RECONFIGURATION,
        &state);

    EXPECT_TRUE(routing_info.empty());
    EXPECT_TRUE(state.ready_task.is_null());
    EXPECT_TRUE(state.types_to_add.empty());
    EXPECT_TRUE(state.added_types.empty());
  }

  // Add type.
  {
    syncable::ModelTypeSet types_to_add;
    types_to_add.insert(syncable::BOOKMARKS);
    syncable::ModelTypeSet types_to_remove;
    ModelSafeRoutingInfo routing_info;

    SyncBackendHost::GetPendingConfigModeState(
        types_to_add, types_to_remove, base::Callback<void(bool)>(),
        &routing_info, sync_api::CONFIGURE_REASON_RECONFIGURATION,
        &state);

    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;
    EXPECT_EQ(expected_routing_info, routing_info);
    EXPECT_TRUE(state.ready_task.is_null());
    EXPECT_EQ(types_to_add, state.types_to_add);
    EXPECT_EQ(types_to_add, state.added_types);
  }

  // Add existing type.
  {
    syncable::ModelTypeSet types_to_add;
    types_to_add.insert(syncable::BOOKMARKS);
    syncable::ModelTypeSet types_to_remove;
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;
    ModelSafeRoutingInfo expected_routing_info = routing_info;

    SyncBackendHost::GetPendingConfigModeState(
        types_to_add, types_to_remove, base::Callback<void(bool)>(),
        &routing_info, sync_api::CONFIGURE_REASON_RECONFIGURATION,
        &state);

    EXPECT_EQ(expected_routing_info, routing_info);
    EXPECT_TRUE(state.ready_task.is_null());
    EXPECT_EQ(types_to_add, state.types_to_add);
    EXPECT_TRUE(state.added_types.empty());
  }

  // Delete type.
  {
    syncable::ModelTypeSet types_to_add;
    syncable::ModelTypeSet types_to_remove;
    types_to_remove.insert(syncable::BOOKMARKS);
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;

    SyncBackendHost::GetPendingConfigModeState(
        types_to_add, types_to_remove, base::Callback<void(bool)>(),
        &routing_info, sync_api::CONFIGURE_REASON_RECONFIGURATION,
        &state);

    EXPECT_TRUE(routing_info.empty());
  }

  // Delete nonexistent type.
  {
    syncable::ModelTypeSet types_to_add;
    syncable::ModelTypeSet types_to_remove;
    types_to_remove.insert(syncable::BOOKMARKS);
    ModelSafeRoutingInfo routing_info;

    SyncBackendHost::GetPendingConfigModeState(
        types_to_add, types_to_remove, base::Callback<void(bool)>(),
        &routing_info, sync_api::CONFIGURE_REASON_RECONFIGURATION,
        &state);

    EXPECT_TRUE(routing_info.empty());
  }
}

// TODO(akalin): Write more SyncBackendHost unit tests.

}  // namespace browser_sync
