// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/sync_notifier_factory.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/notifier/invalidation_version_tracker.h"
#include "chrome/browser/sync/notifier/mock_sync_notifier_observer.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/profile_mock.h"
#include "chrome/test/base/test_url_request_context_getter.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_details.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {
namespace {

using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::StrictMock;
using content::BrowserThread;

class SyncNotifierFactoryTest : public testing::Test {
 public:
  // TODO(zea): switch to using real io_thread instead of shared message loop.
  SyncNotifierFactoryTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        command_line_(CommandLine::NO_PROGRAM) {}
  virtual ~SyncNotifierFactoryTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    request_context_getter_ = new TestURLRequestContextGetter;
    factory_.reset(new SyncNotifierFactory(
        &mock_profile_,
        "fake_client_info",
        request_context_getter_,
        base::WeakPtr<sync_notifier::InvalidationVersionTracker>(),
        command_line_));
    message_loop_.RunAllPending();
  }

  virtual void TearDown() OVERRIDE {
    Mock::VerifyAndClearExpectations(&mock_profile_);
    Mock::VerifyAndClearExpectations(&mock_observer_);
    request_context_getter_ = NULL;
    message_loop_.RunAllPending();
    command_line_ = CommandLine(CommandLine::NO_PROGRAM);
  }

  void TriggerRefreshNotification() {
    const syncable::ModelType type = syncable::SESSIONS;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_SYNC_REFRESH,
        content::Source<Profile>(&mock_profile_),
        content::Details<const syncable::ModelType>(&type));
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  NiceMock<ProfileMock> mock_profile_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;
  scoped_ptr<SyncNotifierFactory> factory_;
  CommandLine command_line_;
};

// Test basic creation of a NonBlockingInvalidationNotifier.
TEST_F(SyncNotifierFactoryTest, Basic) {
  scoped_ptr<SyncNotifier> notifier(factory_->CreateSyncNotifier());
  ASSERT_TRUE(notifier.get());
  notifier->AddObserver(&mock_observer_);
  notifier->RemoveObserver(&mock_observer_);
}

// Test basic creation of a P2PNotifier.
TEST_F(SyncNotifierFactoryTest, Basic_P2P) {
  command_line_.AppendSwitchASCII(switches::kSyncNotificationMethod, "p2p");
  scoped_ptr<SyncNotifier> notifier(factory_->CreateSyncNotifier());
  ASSERT_TRUE(notifier.get());
  notifier->AddObserver(&mock_observer_);
  notifier->RemoveObserver(&mock_observer_);
}

// Create a default sync notifier (NonBlockingInvalidationNotifier wrapped by a
// BridgedSyncNotifier) and then trigger a sync refresh notification. The
// observer should receive the notification as a LOCAL_NOTIFICATION.
TEST_F(SyncNotifierFactoryTest, ChromeSyncNotification) {
  syncable::ModelTypePayloadMap type_payloads;
  type_payloads[syncable::SESSIONS] = "";
  EXPECT_CALL(mock_observer_,
              OnIncomingNotification(type_payloads,
                                     LOCAL_NOTIFICATION));

  scoped_ptr<SyncNotifier> notifier(factory_->CreateSyncNotifier());
  ASSERT_TRUE(notifier.get());
  notifier->AddObserver(&mock_observer_);
  TriggerRefreshNotification();
  message_loop_.RunAllPending();
  notifier->RemoveObserver(&mock_observer_);
}


// Create a P2P sync notifier (wrapped by a BridgedSyncNotifier)
// and then trigger a sync refresh notification. The observer should receive
// the notification as a LOCAL_NOTIFICATION.
TEST_F(SyncNotifierFactoryTest, ChromeSyncNotification_P2P) {
  syncable::ModelTypePayloadMap type_payloads;
  type_payloads[syncable::SESSIONS] = "";
  EXPECT_CALL(mock_observer_,
              OnIncomingNotification(type_payloads,
                                     LOCAL_NOTIFICATION));

  command_line_.AppendSwitchASCII(switches::kSyncNotificationMethod, "p2p");
  scoped_ptr<SyncNotifier> notifier(factory_->CreateSyncNotifier());
  ASSERT_TRUE(notifier.get());
  notifier->AddObserver(&mock_observer_);
  TriggerRefreshNotification();
  message_loop_.RunAllPending();
  notifier->RemoveObserver(&mock_observer_);
}

}  // namespace
}  // namespace sync_notifier
