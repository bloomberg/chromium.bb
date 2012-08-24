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
#include "sync/notifier/fake_sync_notifier.h"
#include "sync/notifier/fake_sync_notifier_observer.h"
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

  MOCK_METHOD1(RegisterHandler, void(syncer::SyncNotifierObserver*));
  MOCK_METHOD2(UpdateRegisteredIds,
               void(syncer::SyncNotifierObserver*,
                    const syncer::ObjectIdSet&));
  MOCK_METHOD1(UnregisterHandler, void(syncer::SyncNotifierObserver*));
};

// All tests just verify that each call is passed through to the delegate, with
// the exception of RegisterHandler, UnregisterHandler, and
// UpdateRegisteredIds, which also verifies the call is forwarded to the
// bridge.
class BridgedSyncNotifierTest : public testing::Test {
 public:
  BridgedSyncNotifierTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        mock_bridge_(&mock_profile_, ui_loop_.message_loop_proxy()),
        // Owned by bridged_notifier_.
        fake_delegate_(new syncer::FakeSyncNotifier()),
        bridged_notifier_(&mock_bridge_, fake_delegate_) {}
  virtual ~BridgedSyncNotifierTest() {}

 protected:
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  NiceMock<ProfileMock> mock_profile_;
  StrictMock<MockChromeSyncNotificationBridge> mock_bridge_;
  syncer::FakeSyncNotifier* fake_delegate_;
  BridgedSyncNotifier bridged_notifier_;
};

TEST_F(BridgedSyncNotifierTest, RegisterHandler) {
  const syncer::ObjectIdSet& ids =
      syncer::ModelTypeSetToObjectIdSet(
          syncer::ModelTypeSet(syncer::APPS, syncer::PREFERENCES));

  syncer::FakeSyncNotifierObserver observer;
  EXPECT_CALL(mock_bridge_, RegisterHandler(&observer));
  EXPECT_CALL(mock_bridge_, UpdateRegisteredIds(&observer, ids));
  EXPECT_CALL(mock_bridge_, UnregisterHandler(&observer));

  bridged_notifier_.RegisterHandler(&observer);
  EXPECT_TRUE(fake_delegate_->IsHandlerRegistered(&observer));

  bridged_notifier_.UpdateRegisteredIds(&observer, ids);
  EXPECT_EQ(ids, fake_delegate_->GetRegisteredIds(&observer));

  bridged_notifier_.UnregisterHandler(&observer);
  EXPECT_FALSE(fake_delegate_->IsHandlerRegistered(&observer));
}

TEST_F(BridgedSyncNotifierTest, SetUniqueId) {
  const std::string& unique_id = "unique id";
  bridged_notifier_.SetUniqueId(unique_id);
  EXPECT_EQ(unique_id, fake_delegate_->GetUniqueId());
}

TEST_F(BridgedSyncNotifierTest, SetStateDeprecated) {
  const std::string& state = "state";
  bridged_notifier_.SetStateDeprecated(state);
  EXPECT_EQ(state, fake_delegate_->GetStateDeprecated());
}

TEST_F(BridgedSyncNotifierTest, UpdateCredentials) {
  const std::string& email = "email";
  const std::string& token = "token";
  bridged_notifier_.UpdateCredentials(email, token);
  EXPECT_EQ(email, fake_delegate_->GetCredentialsEmail());
  EXPECT_EQ(token, fake_delegate_->GetCredentialsToken());
}

TEST_F(BridgedSyncNotifierTest, SendNotification) {
  const syncer::ModelTypeSet changed_types(
      syncer::SESSIONS, syncer::EXTENSIONS);
  bridged_notifier_.SendNotification(changed_types);
  EXPECT_TRUE(fake_delegate_->GetLastChangedTypes().Equals(changed_types));
}

}  // namespace
}  // namespace browser_sync
