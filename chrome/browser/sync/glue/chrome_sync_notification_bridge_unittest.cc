// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_state_map.h"
#include "sync/notifier/object_id_state_map_test_util.h"
#include "sync/notifier/sync_notifier_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::StrictMock;
using content::BrowserThread;

// Receives a ChromeSyncNotificationBridge to register to, and an expected
// ModelTypeStateMap. ReceivedProperNotification() will return true only
// if the observer has received a notification with the proper source and
// state.
// Note: Because this object lives on the sync thread, we use a fake
// (vs a mock) so we don't have to worry about possible thread safety
// issues within GTest/GMock.
class FakeSyncNotifierObserver : public syncer::SyncNotifierObserver {
 public:
  FakeSyncNotifierObserver(
      const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner,
      ChromeSyncNotificationBridge* bridge,
      const syncer::ObjectIdStateMap& expected_states,
      syncer::IncomingNotificationSource expected_source)
      : sync_task_runner_(sync_task_runner),
        bridge_(bridge),
        received_improper_notification_(false),
        notification_count_(0),
        expected_states_(expected_states),
        expected_source_(expected_source) {
    DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
    bridge_->RegisterHandler(this);
    const syncer::ObjectIdSet& ids =
        syncer::ObjectIdStateMapToSet(expected_states);
    bridge_->UpdateRegisteredIds(this, ids);
  }

  virtual ~FakeSyncNotifierObserver() {
    DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
    bridge_->UnregisterHandler(this);
  }

  // SyncNotifierObserver implementation.
  virtual void OnIncomingNotification(
      const syncer::ObjectIdStateMap& id_state_map,
      syncer::IncomingNotificationSource source) OVERRIDE {
    DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
    notification_count_++;
    if (source != expected_source_) {
      LOG(ERROR) << "Received notification with wrong source";
      received_improper_notification_ = true;
    }
    if (!::testing::Matches(Eq(expected_states_))(id_state_map)) {
      LOG(ERROR) << "Received wrong state";
      received_improper_notification_ = true;
    }
  }
  virtual void OnNotificationsEnabled() OVERRIDE {
    NOTREACHED();
  }
  virtual void OnNotificationsDisabled(
      syncer::NotificationsDisabledReason reason) OVERRIDE {
    NOTREACHED();
  }

  bool ReceivedProperNotification() const {
    DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
    return (notification_count_ == 1) && !received_improper_notification_;
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;
  ChromeSyncNotificationBridge* const bridge_;
  bool received_improper_notification_;
  size_t notification_count_;
  const syncer::ObjectIdStateMap expected_states_;
  const syncer::IncomingNotificationSource expected_source_;
};

class ChromeSyncNotificationBridgeTest : public testing::Test {
 public:
  ChromeSyncNotificationBridgeTest()
      : ui_thread_(BrowserThread::UI),
        sync_thread_("Sync thread"),
        sync_observer_(NULL),
        sync_observer_notification_failure_(false),
        done_(true, false) {}

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
    EXPECT_EQ(NULL, sync_observer_);
    if (sync_observer_notification_failure_)
      ADD_FAILURE() << "Sync Observer did not receive proper notification.";
  }

  void VerifyAndDestroyObserver() {
    ASSERT_TRUE(sync_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&ChromeSyncNotificationBridgeTest::
                       VerifyAndDestroyObserverOnSyncThread,
                   base::Unretained(this))));
    BlockForSyncThread();
  }

  void CreateObserverWithExpectations(
      const syncer::ModelTypeStateMap& expected_states,
      syncer::IncomingNotificationSource expected_source) {
    const syncer::ObjectIdStateMap& expected_id_state_map =
        syncer::ModelTypeStateMapToObjectIdStateMap(expected_states);
    ASSERT_TRUE(sync_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(
            &ChromeSyncNotificationBridgeTest::CreateObserverOnSyncThread,
            base::Unretained(this),
            expected_id_state_map,
            expected_source)));
    BlockForSyncThread();
  }

  void UpdateBridgeEnabledTypes(syncer::ModelTypeSet enabled_types) {
    ASSERT_TRUE(sync_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(
            &ChromeSyncNotificationBridgeTest::
                UpdateBridgeEnabledTypesOnSyncThread,
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
  }

 private:
  void VerifyAndDestroyObserverOnSyncThread() {
    DCHECK(sync_thread_.message_loop_proxy()->RunsTasksOnCurrentThread());
    if (!sync_observer_) {
      sync_observer_notification_failure_ = true;
    } else {
      sync_observer_notification_failure_ =
          !sync_observer_->ReceivedProperNotification();
      delete sync_observer_;
      sync_observer_ = NULL;
    }
  }

  void CreateObserverOnSyncThread(
      const syncer::ObjectIdStateMap& expected_states,
      syncer::IncomingNotificationSource expected_source) {
    DCHECK(sync_thread_.message_loop_proxy()->RunsTasksOnCurrentThread());
    sync_observer_ = new FakeSyncNotifierObserver(
        sync_thread_.message_loop_proxy(),
        bridge_.get(),
        expected_states,
        expected_source);
  }

  void UpdateBridgeEnabledTypesOnSyncThread(
      syncer::ModelTypeSet enabled_types) {
    DCHECK(sync_thread_.message_loop_proxy()->RunsTasksOnCurrentThread());
    bridge_->UpdateEnabledTypes(enabled_types);
  }

  void SignalOnSyncThread() {
    DCHECK(sync_thread_.message_loop_proxy()->RunsTasksOnCurrentThread());
    done_.Signal();
  }

  void BlockForSyncThread() {
    done_.Reset();
    ASSERT_TRUE(sync_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&ChromeSyncNotificationBridgeTest::SignalOnSyncThread,
                   base::Unretained(this))));
    done_.TimedWait(TestTimeouts::action_timeout());
    if (!done_.IsSignaled())
      ADD_FAILURE() << "Timed out waiting for sync thread.";
  }

  content::TestBrowserThread ui_thread_;
  base::Thread sync_thread_;
  NiceMock<ProfileMock> mock_profile_;
  // Created/used/destroyed on sync thread.
  FakeSyncNotifierObserver* sync_observer_;
  bool sync_observer_notification_failure_;
  scoped_ptr<ChromeSyncNotificationBridge> bridge_;
  base::WaitableEvent done_;
};

// Adds an observer on the sync thread, triggers a local refresh
// notification, and ensures the bridge posts a LOCAL_NOTIFICATION
// with the proper state to it.
TEST_F(ChromeSyncNotificationBridgeTest, LocalNotification) {
  syncer::ModelTypeStateMap state_map;
  state_map.insert(
      std::make_pair(syncer::SESSIONS, syncer::InvalidationState()));
  CreateObserverWithExpectations(state_map, syncer::LOCAL_NOTIFICATION);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                             state_map);
  VerifyAndDestroyObserver();
}

// Adds an observer on the sync thread, triggers a remote refresh
// notification, and ensures the bridge posts a REMOTE_NOTIFICATION
// with the proper state to it.
TEST_F(ChromeSyncNotificationBridgeTest, RemoteNotification) {
  syncer::ModelTypeStateMap state_map;
  state_map.insert(
      std::make_pair(syncer::SESSIONS, syncer::InvalidationState()));
  CreateObserverWithExpectations(state_map, syncer::REMOTE_NOTIFICATION);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                             state_map);
  VerifyAndDestroyObserver();
}

// Adds an observer on the sync thread, triggers a local refresh
// notification with empty state map and ensures the bridge posts a
// LOCAL_NOTIFICATION with the proper state to it.
TEST_F(ChromeSyncNotificationBridgeTest, LocalNotificationEmptyPayloadMap) {
  const syncer::ModelTypeSet enabled_types(
      syncer::BOOKMARKS, syncer::PASSWORDS);
  const syncer::ModelTypeStateMap enabled_types_state_map =
      syncer::ModelTypeSetToStateMap(enabled_types, std::string());
  CreateObserverWithExpectations(
      enabled_types_state_map, syncer::LOCAL_NOTIFICATION);
  UpdateBridgeEnabledTypes(enabled_types);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                             syncer::ModelTypeStateMap());
  VerifyAndDestroyObserver();
}

// Adds an observer on the sync thread, triggers a remote refresh
// notification with empty state map and ensures the bridge posts a
// REMOTE_NOTIFICATION with the proper state to it.
TEST_F(ChromeSyncNotificationBridgeTest, RemoteNotificationEmptyPayloadMap) {
  const syncer::ModelTypeSet enabled_types(
      syncer::BOOKMARKS, syncer::TYPED_URLS);
  const syncer::ModelTypeStateMap enabled_types_state_map =
      syncer::ModelTypeSetToStateMap(enabled_types, std::string());
  CreateObserverWithExpectations(
      enabled_types_state_map, syncer::REMOTE_NOTIFICATION);
  UpdateBridgeEnabledTypes(enabled_types);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                             syncer::ModelTypeStateMap());
  VerifyAndDestroyObserver();
}

}  // namespace
}  // namespace browser_sync
