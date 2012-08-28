// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bridged_invalidator.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/fake_invalidator.h"
#include "sync/notifier/invalidator.h"
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

  MOCK_METHOD1(RegisterHandler, void(syncer::InvalidationHandler*));
  MOCK_METHOD2(UpdateRegisteredIds,
               void(syncer::InvalidationHandler*,
                    const syncer::ObjectIdSet&));
  MOCK_METHOD1(UnregisterHandler, void(syncer::InvalidationHandler*));
};

// All tests just verify that each call is passed through to the delegate, with
// the exception of RegisterHandler, UnregisterHandler, and
// UpdateRegisteredIds, which also verifies the call is forwarded to the
// bridge.
class BridgedInvalidatorTest : public testing::Test {
 public:
  BridgedInvalidatorTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        mock_bridge_(&mock_profile_, ui_loop_.message_loop_proxy()),
        // Owned by bridged_invalidator_.
        fake_delegate_(new syncer::FakeInvalidator()),
        bridged_invalidator_(&mock_bridge_, fake_delegate_) {}
  virtual ~BridgedInvalidatorTest() {}

 protected:
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  NiceMock<ProfileMock> mock_profile_;
  StrictMock<MockChromeSyncNotificationBridge> mock_bridge_;
  syncer::FakeInvalidator* fake_delegate_;
  BridgedInvalidator bridged_invalidator_;
};

TEST_F(BridgedInvalidatorTest, RegisterHandler) {
  const syncer::ObjectIdSet& ids =
      syncer::ModelTypeSetToObjectIdSet(
          syncer::ModelTypeSet(syncer::APPS, syncer::PREFERENCES));

  syncer::FakeInvalidationHandler handler;
  EXPECT_CALL(mock_bridge_, RegisterHandler(&handler));
  EXPECT_CALL(mock_bridge_, UpdateRegisteredIds(&handler, ids));
  EXPECT_CALL(mock_bridge_, UnregisterHandler(&handler));

  bridged_invalidator_.RegisterHandler(&handler);
  EXPECT_TRUE(fake_delegate_->IsHandlerRegistered(&handler));

  bridged_invalidator_.UpdateRegisteredIds(&handler, ids);
  EXPECT_EQ(ids, fake_delegate_->GetRegisteredIds(&handler));

  bridged_invalidator_.UnregisterHandler(&handler);
  EXPECT_FALSE(fake_delegate_->IsHandlerRegistered(&handler));
}

TEST_F(BridgedInvalidatorTest, SetUniqueId) {
  const std::string& unique_id = "unique id";
  bridged_invalidator_.SetUniqueId(unique_id);
  EXPECT_EQ(unique_id, fake_delegate_->GetUniqueId());
}

TEST_F(BridgedInvalidatorTest, SetStateDeprecated) {
  const std::string& state = "state";
  bridged_invalidator_.SetStateDeprecated(state);
  EXPECT_EQ(state, fake_delegate_->GetStateDeprecated());
}

TEST_F(BridgedInvalidatorTest, UpdateCredentials) {
  const std::string& email = "email";
  const std::string& token = "token";
  bridged_invalidator_.UpdateCredentials(email, token);
  EXPECT_EQ(email, fake_delegate_->GetCredentialsEmail());
  EXPECT_EQ(token, fake_delegate_->GetCredentialsToken());
}

TEST_F(BridgedInvalidatorTest, SendNotification) {
  const syncer::ModelTypeSet changed_types(
      syncer::SESSIONS, syncer::EXTENSIONS);
  bridged_invalidator_.SendNotification(changed_types);
  EXPECT_TRUE(fake_delegate_->GetLastChangedTypes().Equals(changed_types));
}

}  // namespace
}  // namespace browser_sync
