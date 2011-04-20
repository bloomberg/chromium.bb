// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_host.h"

#include <cstddef>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
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

  MOCK_METHOD0(OnBackendInitialized, void());
  MOCK_METHOD0(OnSyncCycleCompleted, void());
  MOCK_METHOD0(OnAuthError, void());
  MOCK_METHOD0(OnStopSyncingPermanently, void());
  MOCK_METHOD0(OnClearServerDataSucceeded, void());
  MOCK_METHOD0(OnClearServerDataFailed, void());
  MOCK_METHOD1(OnPassphraseRequired, void(bool));
  MOCK_METHOD0(OnPassphraseAccepted, void());
  MOCK_METHOD1(OnEncryptionComplete, void(const syncable::ModelTypeSet&));
  MOCK_METHOD1(OnMigrationNeededForTypes, void(const syncable::ModelTypeSet&));
};

}  // namespace

class SyncBackendHostTest : public testing::Test {
 protected:
  SyncBackendHostTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_) {}

 private:
  MessageLoop ui_loop_;
  BrowserThread ui_thread_;
};

TEST_F(SyncBackendHostTest, InitShutdown) {
  TestingProfile profile;
  profile.CreateRequestContext();

  SyncBackendHost backend(&profile);

  // TODO(akalin): Handle this in SyncBackendHost instead of in
  // ProfileSyncService, or maybe figure out a way to share the
  // "register sync prefs" code.
  PrefService* pref_service = profile.GetPrefs();
  pref_service->RegisterStringPref(prefs::kEncryptionBootstrapToken, "");

  MockSyncFrontend mock_frontend;
  sync_api::SyncCredentials credentials;
  credentials.email = "user@example.com";
  credentials.sync_token = "sync_token";
  backend.Initialize(&mock_frontend,
                     GURL("http://www.example.com"),
                     syncable::ModelTypeSet(),
                     profile.GetRequestContext(),
                     credentials,
                     true);
  backend.Shutdown(false);
  // Scoping for io_thread to get destroyed before other locals.
  {
    // The request context gets deleted on the I/O thread. To prevent a leak
    // supply one here.
    // TODO(sanjeevr): Investigate whether we can do this within
    // ResetRequestContext
    BrowserThread io_thread(BrowserThread::IO, MessageLoop::current());
    profile.ResetRequestContext();
  }
  MessageLoop::current()->RunAllPending();
}

TEST_F(SyncBackendHostTest, MakePendingConfigModeState) {
  // Empty.
  {
    DataTypeController::TypeMap data_type_controllers;
    syncable::ModelTypeSet types;
    ModelSafeRoutingInfo routing_info;

    scoped_ptr<SyncBackendHost::PendingConfigureDataTypesState>
        state(SyncBackendHost::MakePendingConfigModeState(
            data_type_controllers, types, NULL, &routing_info));
    EXPECT_TRUE(routing_info.empty());
    EXPECT_FALSE(state->ready_task.get());
    EXPECT_EQ(types, state->initial_types);
    EXPECT_FALSE(state->deleted_type);
    EXPECT_TRUE(state->added_types.none());
  }

  // No enabled types.
  {
    DataTypeController::TypeMap data_type_controllers;
    data_type_controllers[syncable::BOOKMARKS] = NULL;
    syncable::ModelTypeSet types;
    ModelSafeRoutingInfo routing_info;

    scoped_ptr<SyncBackendHost::PendingConfigureDataTypesState>
        state(SyncBackendHost::MakePendingConfigModeState(
            data_type_controllers, types, NULL, &routing_info));
    EXPECT_TRUE(routing_info.empty());
    EXPECT_FALSE(state->ready_task.get());
    EXPECT_EQ(types, state->initial_types);
    EXPECT_TRUE(state->deleted_type);
    EXPECT_TRUE(state->added_types.none());
  }

  // Add type.
  {
    DataTypeController::TypeMap data_type_controllers;
    data_type_controllers[syncable::BOOKMARKS] = NULL;
    syncable::ModelTypeSet types;
    types.insert(syncable::BOOKMARKS);
    ModelSafeRoutingInfo routing_info;

    scoped_ptr<SyncBackendHost::PendingConfigureDataTypesState>
        state(SyncBackendHost::MakePendingConfigModeState(
            data_type_controllers, types, NULL, &routing_info));

    ModelSafeRoutingInfo expected_routing_info;
    expected_routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;
    EXPECT_EQ(expected_routing_info, routing_info);
    EXPECT_FALSE(state->ready_task.get());
    EXPECT_EQ(types, state->initial_types);
    EXPECT_FALSE(state->deleted_type);

    syncable::ModelTypeBitSet expected_added_types;
    expected_added_types.set(syncable::BOOKMARKS);
    EXPECT_EQ(expected_added_types, state->added_types);
  }

  // Add existing type.
  {
    DataTypeController::TypeMap data_type_controllers;
    data_type_controllers[syncable::BOOKMARKS] = NULL;
    syncable::ModelTypeSet types;
    types.insert(syncable::BOOKMARKS);
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;
    ModelSafeRoutingInfo expected_routing_info = routing_info;

    scoped_ptr<SyncBackendHost::PendingConfigureDataTypesState>
        state(SyncBackendHost::MakePendingConfigModeState(
            data_type_controllers, types, NULL, &routing_info));

    EXPECT_EQ(expected_routing_info, routing_info);
    EXPECT_FALSE(state->ready_task.get());
    EXPECT_EQ(types, state->initial_types);
    EXPECT_FALSE(state->deleted_type);
    EXPECT_TRUE(state->added_types.none());
  }

  // Delete type.
  {
    DataTypeController::TypeMap data_type_controllers;
    data_type_controllers[syncable::BOOKMARKS] = NULL;
    syncable::ModelTypeSet types;
    ModelSafeRoutingInfo routing_info;
    routing_info[syncable::BOOKMARKS] = GROUP_PASSIVE;

    scoped_ptr<SyncBackendHost::PendingConfigureDataTypesState>
        state(SyncBackendHost::MakePendingConfigModeState(
            data_type_controllers, types, NULL, &routing_info));

    ModelSafeRoutingInfo expected_routing_info;
    EXPECT_EQ(expected_routing_info, routing_info);
    EXPECT_FALSE(state->ready_task.get());
    EXPECT_EQ(types, state->initial_types);
    EXPECT_TRUE(state->deleted_type);
    EXPECT_TRUE(state->added_types.none());
  }
}

// TODO(akalin): Write more SyncBackendHost unit tests.

}  // namespace browser_sync
