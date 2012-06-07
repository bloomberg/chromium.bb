// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/internal_api/public/syncable/model_type_payload_map.h"
#include "sync/notifier/mock_sync_notifier_observer.h"
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
// Note: Because this object lives on the IO thread, we use a fake (vs a mock)
// so we don't have to worry about possible thread safety issues within
// GTest/GMock.
class FakeSyncNotifierObserverIO
    : public sync_notifier::SyncNotifierObserver {
 public:
  FakeSyncNotifierObserverIO(
      ChromeSyncNotificationBridge* bridge,
      const syncable::ModelTypePayloadMap& expected_payloads)
      : bridge_(bridge),
        received_improper_notification_(false),
        notification_count_(0),
        expected_payloads_(expected_payloads) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    bridge_->AddObserver(this);
  }
  virtual ~FakeSyncNotifierObserverIO() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    bridge_->RemoveObserver(this);
  }

  // SyncNotifierObserver implementation.
  virtual void OnIncomingNotification(
      const syncable::ModelTypePayloadMap& type_payloads,
      sync_notifier::IncomingNotificationSource source) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    notification_count_++;
    if (source != sync_notifier::LOCAL_NOTIFICATION) {
      LOG(ERROR) << "Received notification with wrong source.";
      received_improper_notification_ = true;
    }
    if (expected_payloads_ != type_payloads) {
      LOG(ERROR) << "Received wrong payload.";
      received_improper_notification_ = true;
    }
  }
  virtual void OnNotificationStateChange(bool notifications_enabled) {
    NOTREACHED();
  }

  bool ReceivedProperNotification() const {
    return (notification_count_ == 1) && !received_improper_notification_;
  }

 private:
  ChromeSyncNotificationBridge* bridge_;
  bool received_improper_notification_;
  size_t notification_count_;
  syncable::ModelTypePayloadMap expected_payloads_;
};

class ChromeSyncNotificationBridgeTest : public testing::Test {
 public:
  ChromeSyncNotificationBridgeTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        io_thread_(BrowserThread::IO),
        io_observer_(NULL),
        io_observer_notification_failure_(false),
        bridge_(&mock_profile_),
        done_(true, false) {
    io_thread_.StartIOThread();
  }
  virtual ~ChromeSyncNotificationBridgeTest() {}

 protected:
  virtual void TearDown() OVERRIDE {
    io_thread_.Stop();
    ui_loop_.RunAllPending();
    EXPECT_EQ(NULL, io_observer_);
    if (io_observer_notification_failure_)
      ADD_FAILURE() << "IO Observer did not receive proper notification.";
  }

  void VerifyAndDestroyObserverOnIOThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!io_observer_) {
      io_observer_notification_failure_ = true;
    } else {
      io_observer_notification_failure_ =
          !io_observer_->ReceivedProperNotification();
      delete io_observer_;
      io_observer_ = NULL;
    }
  }

  void VerifyAndDestroyObserver() {
    ASSERT_TRUE(BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ChromeSyncNotificationBridgeTest::
                       VerifyAndDestroyObserverOnIOThread,
                   base::Unretained(this))));
  }

  void CreateObserverOnIOThread(
      syncable::ModelTypePayloadMap expected_payloads) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    io_observer_ = new FakeSyncNotifierObserverIO(&bridge_,
                                                  expected_payloads);
  }

  void CreateObserverWithExpectedPayload(
      syncable::ModelTypePayloadMap expected_payloads) {
    ASSERT_TRUE(BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ChromeSyncNotificationBridgeTest::CreateObserverOnIOThread,
                   base::Unretained(this),
                   expected_payloads)));
    BlockForIOThread();
  }

  void SignalOnIOThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    done_.Signal();
  }

  void BlockForIOThread() {
    done_.Reset();
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ChromeSyncNotificationBridgeTest::SignalOnIOThread,
                   base::Unretained(this)));
    done_.TimedWait(TestTimeouts::action_timeout());
    if (!done_.IsSignaled())
      ADD_FAILURE() << "Timed out waiting for IO thread.";
  }

  void TriggerRefreshNotification(
      int type,
      const syncable::ModelTypePayloadMap& payload_map) {
    content::NotificationService::current()->Notify(
        type,
        content::Source<Profile>(&mock_profile_),
        content::Details<const syncable::ModelTypePayloadMap>(&payload_map));
  }

  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  NiceMock<ProfileMock> mock_profile_;
  // Created/used/destroyed on I/O thread.
  FakeSyncNotifierObserverIO* io_observer_;
  bool io_observer_notification_failure_;
  ChromeSyncNotificationBridge bridge_;
  base::WaitableEvent done_;
};

// Adds an observer on the UI thread, triggers a local refresh notification, and
// ensures the bridge posts a LOCAL_NOTIFICATION with the proper payload to it.
TEST_F(ChromeSyncNotificationBridgeTest, LocalNotification) {
  syncable::ModelTypePayloadMap payload_map;
  payload_map[syncable::SESSIONS] = "";
  StrictMock<sync_notifier::MockSyncNotifierObserver> observer;
  EXPECT_CALL(observer,
              OnIncomingNotification(payload_map,
                                     sync_notifier::LOCAL_NOTIFICATION));
  bridge_.AddObserver(&observer);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                             payload_map);
  ui_loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
}

// Adds an observer on the UI thread, triggers a remote refresh notification,
// and ensures the bridge posts a REMOTE_NOTIFICATION with the proper payload
// to it.
TEST_F(ChromeSyncNotificationBridgeTest, RemoteNotification) {
  syncable::ModelTypePayloadMap payload_map;
  payload_map[syncable::BOOKMARKS] = "";
  StrictMock<sync_notifier::MockSyncNotifierObserver> observer;
  EXPECT_CALL(observer,
              OnIncomingNotification(payload_map,
                                     sync_notifier::REMOTE_NOTIFICATION));
  bridge_.AddObserver(&observer);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                             payload_map);
  ui_loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
}

// Adds an observer on the UI thread, triggers a local refresh notification
// with empty payload map and ensures the bridge posts a
// LOCAL_NOTIFICATION with the proper payload to it.
TEST_F(ChromeSyncNotificationBridgeTest, LocalNotificationEmptyPayloadMap) {
  const syncable::ModelTypeSet enabled_types(
      syncable::BOOKMARKS, syncable::PASSWORDS);
  const syncable::ModelTypePayloadMap enabled_types_payload_map =
      syncable::ModelTypePayloadMapFromEnumSet(enabled_types, std::string());

  StrictMock<sync_notifier::MockSyncNotifierObserver> observer;
  EXPECT_CALL(observer,
              OnIncomingNotification(enabled_types_payload_map,
                                     sync_notifier::LOCAL_NOTIFICATION));
  bridge_.AddObserver(&observer);
  // Set enabled types on the bridge.
  bridge_.UpdateEnabledTypes(enabled_types);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                             syncable::ModelTypePayloadMap());
  ui_loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
}

// Adds an observer on the UI thread, triggers a remote refresh notification
// with empty payload map and ensures the bridge posts a
// REMOTE_NOTIFICATION with the proper payload to it.
TEST_F(ChromeSyncNotificationBridgeTest, RemoteNotificationEmptyPayloadMap) {
  const syncable::ModelTypeSet enabled_types(
      syncable::BOOKMARKS, syncable::TYPED_URLS);
  const syncable::ModelTypePayloadMap enabled_types_payload_map =
      syncable::ModelTypePayloadMapFromEnumSet(enabled_types, std::string());

  StrictMock<sync_notifier::MockSyncNotifierObserver> observer;
  EXPECT_CALL(observer,
              OnIncomingNotification(enabled_types_payload_map,
                                     sync_notifier::REMOTE_NOTIFICATION));
  bridge_.AddObserver(&observer);
  // Set enabled types on the bridge.
  bridge_.UpdateEnabledTypes(enabled_types);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                             syncable::ModelTypePayloadMap());
  ui_loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(&observer);
}

// Adds an observer on the I/O thread. Then triggers a refresh notification on
// the UI thread. We finally verify the proper notification was received by the
// observer and destroy it on the I/O thread.
TEST_F(ChromeSyncNotificationBridgeTest, BasicThreaded) {
  syncable::ModelTypePayloadMap payload_map;
  payload_map[syncable::SESSIONS] = "";
  CreateObserverWithExpectedPayload(payload_map);
  TriggerRefreshNotification(chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                             payload_map);
  VerifyAndDestroyObserver();
}

}  // namespace
}  // namespace browser_sync
