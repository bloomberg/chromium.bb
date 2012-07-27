// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
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
#include "sync/internal_api/public/base/model_type_payload_map.h"
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
// ModelTypePayloadMap. ReceivedProperNotification() will return true only
// if the observer has received a notification with the proper source and
// payload.
// Note: Because this object lives on the sync thread, we use a fake
// (vs a mock) so we don't have to worry about possible thread safety
// issues within GTest/GMock.
class FakeSyncNotifierObserver : public syncer::SyncNotifierObserver {
 public:
  FakeSyncNotifierObserver(
      const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner,
      ChromeSyncNotificationBridge* bridge,
      const syncer::ObjectIdPayloadMap& expected_payloads,
      syncer::IncomingNotificationSource expected_source)
      : sync_task_runner_(sync_task_runner),
        bridge_(bridge),
        received_improper_notification_(false),
        notification_count_(0),
        expected_payloads_(expected_payloads),
        expected_source_(expected_source) {
    DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
    // TODO(dcheng): We might want a function to go from ObjectIdPayloadMap ->
    // ObjectIdSet to avoid this rather long incantation...
    const syncer::ObjectIdSet& ids = syncer::ModelTypeSetToObjectIdSet(
        syncer::ModelTypePayloadMapToEnumSet(
            syncer::ObjectIdPayloadMapToModelTypePayloadMap(
                expected_payloads)));
    bridge_->UpdateRegisteredIds(this, ids);
  }

  virtual ~FakeSyncNotifierObserver() {
    DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
    bridge_->UpdateRegisteredIds(this, syncer::ObjectIdSet());
  }

  // SyncNotifierObserver implementation.
  virtual void OnIncomingNotification(
      const syncer::ObjectIdPayloadMap& id_payloads,
      syncer::IncomingNotificationSource source) OVERRIDE {
    DCHECK(sync_task_runner_->RunsTasksOnCurrentThread());
    notification_count_++;
    if (source != expected_source_) {
      LOG(ERROR) << "Received notification with wrong source";
      received_improper_notification_ = true;
    }
    if (expected_payloads_ != id_payloads) {
      LOG(ERROR) << "Received wrong payload";
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
  const syncer::ObjectIdPayloadMap expected_payloads_;
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
      const syncer::ModelTypePayloadMap& expected_payloads,
      syncer::IncomingNotificationSource expected_source) {
    const syncer::ObjectIdPayloadMap& expected_id_payloads =
        syncer::ModelTypePayloadMapToObjectIdPayloadMap(expected_payloads);
    ASSERT_TRUE(sync_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(
            &ChromeSyncNotificationBridgeTest::CreateObserverOnSyncThread,
            base::Unretained(this),
            expected_id_payloads,
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
      const syncer::ModelTypePayloadMap& payload_map) {
    content::NotificationService::current()->Notify(
        type,
        content::Source<Profile>(&mock_profile_),
        content::Details<const syncer::ModelTypePayloadMap>(&payload_map));
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
      const syncer::ObjectIdPayloadMap& expected_payloads,
      syncer::IncomingNotificationSource expected_source) {
    DCHECK(sync_thread_.message_loop_proxy()->RunsTasksOnCurrentThread());
    sync_observer_ = new FakeSyncNotifierObserver(
        sync_thread_.message_loop_proxy(),
        bridge_.get(),
        expected_payloads,
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
// with the proper payload to it.
TEST_F(ChromeSyncNotificationBridgeTest, LocalNotification) {
  syncer::ModelTypePayloadMap payload_map;
  payload_map[syncer::SESSIONS] = "";
  CreateObserverWithExpectations(payload_map, syncer::LOCAL_NOTIFICATION);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                             payload_map);
  VerifyAndDestroyObserver();
}

// Adds an observer on the sync thread, triggers a remote refresh
// notification, and ensures the bridge posts a REMOTE_NOTIFICATION
// with the proper payload to it.
TEST_F(ChromeSyncNotificationBridgeTest, RemoteNotification) {
  syncer::ModelTypePayloadMap payload_map;
  payload_map[syncer::SESSIONS] = "";
  CreateObserverWithExpectations(payload_map, syncer::REMOTE_NOTIFICATION);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                             payload_map);
  VerifyAndDestroyObserver();
}

// Adds an observer on the sync thread, triggers a local refresh
// notification with empty payload map and ensures the bridge posts a
// LOCAL_NOTIFICATION with the proper payload to it.
TEST_F(ChromeSyncNotificationBridgeTest, LocalNotificationEmptyPayloadMap) {
  const syncer::ModelTypeSet enabled_types(
      syncer::BOOKMARKS, syncer::PASSWORDS);
  const syncer::ModelTypePayloadMap enabled_types_payload_map =
      syncer::ModelTypePayloadMapFromEnumSet(enabled_types, std::string());
  CreateObserverWithExpectations(
      enabled_types_payload_map, syncer::LOCAL_NOTIFICATION);
  UpdateBridgeEnabledTypes(enabled_types);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                             syncer::ModelTypePayloadMap());
  VerifyAndDestroyObserver();
}

// Adds an observer on the sync thread, triggers a remote refresh
// notification with empty payload map and ensures the bridge posts a
// REMOTE_NOTIFICATION with the proper payload to it.
TEST_F(ChromeSyncNotificationBridgeTest, RemoteNotificationEmptyPayloadMap) {
  const syncer::ModelTypeSet enabled_types(
      syncer::BOOKMARKS, syncer::TYPED_URLS);
  const syncer::ModelTypePayloadMap enabled_types_payload_map =
      syncer::ModelTypePayloadMapFromEnumSet(enabled_types, std::string());
  CreateObserverWithExpectations(
      enabled_types_payload_map, syncer::REMOTE_NOTIFICATION);
  UpdateBridgeEnabledTypes(enabled_types);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                             syncer::ModelTypePayloadMap());
  VerifyAndDestroyObserver();
}

}  // namespace
}  // namespace browser_sync
