// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"

#include <cstddef>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_state_map.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/object_id_state_map_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

using ::testing::NiceMock;

// Needed by BlockForSyncThread().
void DoNothing() {}

// Since all the interesting stuff happens on the sync thread, we have
// to be careful to use GTest/GMock only on the main thread since they
// are not thread-safe.

class ChromeSyncNotificationBridgeTest : public testing::Test {
 public:
  ChromeSyncNotificationBridgeTest()
      : ui_thread_(content::BrowserThread::UI, &ui_loop_),
        sync_thread_("Sync thread"),
        sync_handler_notification_success_(false) {}

  virtual ~ChromeSyncNotificationBridgeTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(sync_thread_.Start());
    bridge_.reset(
        new ChromeSyncNotificationBridge(
            &mock_profile_, sync_thread_.message_loop_proxy()));
  }

  virtual void TearDown() OVERRIDE {
    bridge_->StopForShutdown();
    sync_thread_.Stop();
    // Must be reset only after the sync thread is stopped.
    bridge_.reset();
    EXPECT_EQ(NULL, sync_handler_.get());
    if (!sync_handler_notification_success_)
      ADD_FAILURE() << "Sync handler did not receive proper notification.";
  }

  void VerifyAndDestroyObserver(
      const syncer::ModelTypeStateMap& expected_states,
      syncer::IncomingInvalidationSource expected_source) {
    ASSERT_TRUE(sync_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&ChromeSyncNotificationBridgeTest::
                       VerifyAndDestroyObserverOnSyncThread,
                   base::Unretained(this),
                   expected_states,
                   expected_source)));
    BlockForSyncThread();
  }

  void CreateObserver() {
    ASSERT_TRUE(sync_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(
            &ChromeSyncNotificationBridgeTest::CreateObserverOnSyncThread,
            base::Unretained(this))));
    BlockForSyncThread();
  }

  void UpdateEnabledTypes(syncer::ModelTypeSet enabled_types) {
    ASSERT_TRUE(sync_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(
            &ChromeSyncNotificationBridgeTest::
                UpdateEnabledTypesOnSyncThread,
            base::Unretained(this),
            enabled_types)));
    BlockForSyncThread();
  }

  void TriggerRefreshNotification(
      int type,
      const syncer::ModelTypeStateMap& state_map) {
    content::NotificationService::current()->Notify(
        type,
        content::Source<Profile>(&mock_profile_),
        content::Details<const syncer::ModelTypeStateMap>(&state_map));
    BlockForSyncThread();
  }

 private:
  void VerifyAndDestroyObserverOnSyncThread(
      const syncer::ModelTypeStateMap& expected_states,
      syncer::IncomingInvalidationSource expected_source) {
    DCHECK(sync_thread_.message_loop_proxy()->RunsTasksOnCurrentThread());
    if (sync_handler_.get()) {
      sync_handler_notification_success_ =
          (sync_handler_->GetInvalidationCount() == 1) &&
          ObjectIdStateMapEquals(
              sync_handler_->GetLastInvalidationIdStateMap(),
              syncer::ModelTypeStateMapToObjectIdStateMap(expected_states)) &&
          (sync_handler_->GetLastInvalidationSource() == expected_source);
      bridge_->UnregisterHandler(sync_handler_.get());
    } else {
      sync_handler_notification_success_ = false;
    }
    sync_handler_.reset();
  }

  void CreateObserverOnSyncThread() {
    DCHECK(sync_thread_.message_loop_proxy()->RunsTasksOnCurrentThread());
    sync_handler_.reset(new syncer::FakeInvalidationHandler());
    bridge_->RegisterHandler(sync_handler_.get());
  }

  void UpdateEnabledTypesOnSyncThread(
      syncer::ModelTypeSet enabled_types) {
    DCHECK(sync_thread_.message_loop_proxy()->RunsTasksOnCurrentThread());
    bridge_->UpdateRegisteredIds(
        sync_handler_.get(), ModelTypeSetToObjectIdSet(enabled_types));
  }

  void BlockForSyncThread() {
    // Post a task to the sync thread's message loop and block until
    // it runs.
    base::RunLoop run_loop;
    ASSERT_TRUE(sync_thread_.message_loop_proxy()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&DoNothing),
        run_loop.QuitClosure()));
    run_loop.Run();
  }

  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  base::Thread sync_thread_;
  NiceMock<ProfileMock> mock_profile_;
  // Created/used/destroyed on sync thread.
  scoped_ptr<syncer::FakeInvalidationHandler> sync_handler_;
  bool sync_handler_notification_success_;
  scoped_ptr<ChromeSyncNotificationBridge> bridge_;
};

// Adds an observer on the sync thread, triggers a local refresh
// invalidation, and ensures the bridge posts a LOCAL_INVALIDATION
// with the proper state to it.
TEST_F(ChromeSyncNotificationBridgeTest, LocalNotification) {
  syncer::ModelTypeStateMap state_map;
  state_map.insert(
      std::make_pair(syncer::SESSIONS, syncer::InvalidationState()));
  CreateObserver();
  UpdateEnabledTypes(syncer::ModelTypeSet(syncer::SESSIONS));
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                             state_map);
  VerifyAndDestroyObserver(state_map, syncer::LOCAL_INVALIDATION);
}

// Adds an observer on the sync thread, triggers a remote refresh
// invalidation, and ensures the bridge posts a REMOTE_INVALIDATION
// with the proper state to it.
TEST_F(ChromeSyncNotificationBridgeTest, RemoteNotification) {
  syncer::ModelTypeStateMap state_map;
  state_map.insert(
      std::make_pair(syncer::SESSIONS, syncer::InvalidationState()));
  CreateObserver();
  UpdateEnabledTypes(syncer::ModelTypeSet(syncer::SESSIONS));
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                             state_map);
  VerifyAndDestroyObserver(state_map, syncer::REMOTE_INVALIDATION);
}

// Adds an observer on the sync thread, triggers a local refresh
// notification with empty state map and ensures the bridge posts a
// LOCAL_INVALIDATION with the proper state to it.
TEST_F(ChromeSyncNotificationBridgeTest, LocalNotificationEmptyPayloadMap) {
  const syncer::ModelTypeSet enabled_types(
      syncer::BOOKMARKS, syncer::PASSWORDS);
  const syncer::ModelTypeStateMap enabled_types_state_map =
      syncer::ModelTypeSetToStateMap(enabled_types, std::string());
  CreateObserver();
  UpdateEnabledTypes(enabled_types);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                             syncer::ModelTypeStateMap());
  VerifyAndDestroyObserver(
      enabled_types_state_map, syncer::LOCAL_INVALIDATION);
}

// Adds an observer on the sync thread, triggers a remote refresh
// notification with empty state map and ensures the bridge posts a
// REMOTE_INVALIDATION with the proper state to it.
TEST_F(ChromeSyncNotificationBridgeTest, RemoteNotificationEmptyPayloadMap) {
  const syncer::ModelTypeSet enabled_types(
      syncer::BOOKMARKS, syncer::TYPED_URLS);
  const syncer::ModelTypeStateMap enabled_types_state_map =
      syncer::ModelTypeSetToStateMap(enabled_types, std::string());
  CreateObserver();
  UpdateEnabledTypes(enabled_types);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                             syncer::ModelTypeStateMap());
  VerifyAndDestroyObserver(
      enabled_types_state_map, syncer::REMOTE_INVALIDATION);
}

}  // namespace
}  // namespace browser_sync
