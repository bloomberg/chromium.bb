// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bridged_sync_notifier.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/notifier/mock_sync_notifier_observer.h"
#include "sync/notifier/sync_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {
namespace {

using ::testing::NiceMock;
using ::testing::StrictMock;
using content::BrowserThread;
using syncer::HasModelTypes;

class MockChromeSyncNotificationBridge : public ChromeSyncNotificationBridge {
 public:
  MockChromeSyncNotificationBridge(
      const Profile* profile,
      const scoped_refptr<base::SequencedTaskRunner>& sync_task_runner)
      : ChromeSyncNotificationBridge(profile, sync_task_runner) {}
  virtual ~MockChromeSyncNotificationBridge() {}

  MOCK_METHOD2(UpdateRegisteredIds, void(syncer::SyncNotifierObserver*,
                                         const syncer::ObjectIdSet&));
};

class MockSyncNotifier : public syncer::SyncNotifier {
 public:
  MockSyncNotifier() {}
  virtual ~MockSyncNotifier() {}

  MOCK_METHOD2(UpdateRegisteredIds,
               void(syncer::SyncNotifierObserver*, const syncer::ObjectIdSet&));
  MOCK_METHOD1(SetUniqueId, void(const std::string&));
  MOCK_METHOD1(SetStateDeprecated, void(const std::string&));
  MOCK_METHOD2(UpdateCredentials, void(const std::string&, const std::string&));
  MOCK_METHOD1(SendNotification, void(syncer::ModelTypeSet));
};

// All tests just verify that each call is passed through to the delegate, with
// the exception of UpdateRegisteredIds, which also verifies the call is
// forwarded to the bridge.
class BridgedSyncNotifierTest : public testing::Test {
 public:
  BridgedSyncNotifierTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        mock_bridge_(&mock_profile_, ui_loop_.message_loop_proxy()),
        mock_delegate_(new MockSyncNotifier),  // Owned by bridged_notifier_.
        bridged_notifier_(&mock_bridge_, mock_delegate_) {}
  virtual ~BridgedSyncNotifierTest() {}

 protected:
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  NiceMock<ProfileMock> mock_profile_;
  StrictMock<MockChromeSyncNotificationBridge> mock_bridge_;
  MockSyncNotifier* mock_delegate_;
  BridgedSyncNotifier bridged_notifier_;
};

TEST_F(BridgedSyncNotifierTest, UpdateRegisteredIds) {
  syncer::MockSyncNotifierObserver observer;
  EXPECT_CALL(mock_bridge_, UpdateRegisteredIds(
      &observer, syncer::ObjectIdSet()));
  EXPECT_CALL(*mock_delegate_, UpdateRegisteredIds(
      &observer, syncer::ObjectIdSet()));
  bridged_notifier_.UpdateRegisteredIds(&observer, syncer::ObjectIdSet());
}

TEST_F(BridgedSyncNotifierTest, SetUniqueId) {
  std::string unique_id = "unique id";
  EXPECT_CALL(*mock_delegate_, SetUniqueId(unique_id));
  bridged_notifier_.SetUniqueId(unique_id);
}

TEST_F(BridgedSyncNotifierTest, SetStateDeprecated) {
  std::string state = "state";
  EXPECT_CALL(*mock_delegate_, SetStateDeprecated(state));
  bridged_notifier_.SetStateDeprecated(state);
}

TEST_F(BridgedSyncNotifierTest, UpdateCredentials) {
  std::string email = "email";
  std::string token = "token";
  EXPECT_CALL(*mock_delegate_, UpdateCredentials(email, token));
  bridged_notifier_.UpdateCredentials(email, token);
}

TEST_F(BridgedSyncNotifierTest, SendNotification) {
  syncer::ModelTypeSet changed_types(syncer::SESSIONS, syncer::EXTENSIONS);
  EXPECT_CALL(*mock_delegate_, SendNotification(HasModelTypes(changed_types)));
  bridged_notifier_.SendNotification(changed_types);
}

}  // namespace
}  // namespace browser_sync
