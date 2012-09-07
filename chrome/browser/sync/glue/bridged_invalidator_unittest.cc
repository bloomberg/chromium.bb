// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bridged_invalidator.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/test/test_browser_thread.h"
#include "google/cacheinvalidation/include/types.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/fake_invalidator.h"
#include "sync/notifier/invalidator_test_template.h"
#include "sync/notifier/object_id_state_map_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
class InvalidationStateTracker;
}  // namespace syncer

namespace browser_sync {

// BridgedInvalidatorTestDelegate has to be visible from the syncer
// namespace (where InvalidatorTest lives).
class BridgedInvalidatorTestDelegate {
 public:
  BridgedInvalidatorTestDelegate()
      : ui_thread_(content::BrowserThread::UI, &ui_loop_),
        bridge_(&mock_profile_, ui_loop_.message_loop_proxy()),
        fake_delegate_(NULL) {
    // Pump |ui_loop_| to fully initialize |bridge_|.
    ui_loop_.RunAllPending();
  }

  ~BridgedInvalidatorTestDelegate() {
    DestroyInvalidator();
  }

  void CreateInvalidator(
      const std::string& /* initial_state (unused) */,
      const base::WeakPtr<syncer::InvalidationStateTracker>&
      /* invalidation_state_tracker (unused) */) {
    DCHECK(!fake_delegate_);
    DCHECK(!invalidator_.get());
    fake_delegate_ = new syncer::FakeInvalidator();
    invalidator_.reset(new BridgedInvalidator(
        &bridge_, fake_delegate_, syncer::DEFAULT_INVALIDATION_ERROR));
  }

  BridgedInvalidator* GetInvalidator() {
    return invalidator_.get();
  }

  ChromeSyncNotificationBridge* GetBridge() {
    return &bridge_;
  }

  syncer::FakeInvalidator* GetFakeDelegate() {
    return fake_delegate_;
  }

  void DestroyInvalidator() {
    invalidator_.reset();
    fake_delegate_ = NULL;
    bridge_.StopForShutdown();
    ui_loop_.RunAllPending();
  }

  void WaitForInvalidator() {
    // Do nothing.
  }

  void TriggerOnInvalidatorStateChange(syncer::InvalidatorState state) {
    fake_delegate_->EmitOnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(
      const syncer::ObjectIdStateMap& id_state_map,
      syncer::IncomingInvalidationSource source) {
    fake_delegate_->EmitOnIncomingInvalidation(id_state_map, source);
  }

  static bool InvalidatorHandlesDeprecatedState() {
    return false;
  }

 private:
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  ::testing::NiceMock<ProfileMock> mock_profile_;
  ChromeSyncNotificationBridge bridge_;
  // Owned by |invalidator_|.
  syncer::FakeInvalidator* fake_delegate_;
  scoped_ptr<BridgedInvalidator> invalidator_;
};

namespace {

// All tests just verify that each call is passed through to the delegate, with
// the exception of RegisterHandler, UnregisterHandler, and
// UpdateRegisteredIds, which also verifies the call is forwarded to the
// bridge.
class BridgedInvalidatorTest : public testing::Test {
 public:
  BridgedInvalidatorTest() {
    delegate_.CreateInvalidator(
        "fake_state", base::WeakPtr<syncer::InvalidationStateTracker>());
  }

  virtual ~BridgedInvalidatorTest() {}

 protected:
  BridgedInvalidatorTestDelegate delegate_;
};

TEST_F(BridgedInvalidatorTest, HandlerMethods) {
  syncer::ObjectIdSet ids;
  ids.insert(invalidation::ObjectId(1, "id1"));

  syncer::FakeInvalidationHandler handler;

  delegate_.GetInvalidator()->RegisterHandler(&handler);
  EXPECT_TRUE(delegate_.GetFakeDelegate()->IsHandlerRegistered(&handler));
  EXPECT_TRUE(delegate_.GetBridge()->IsHandlerRegisteredForTest(&handler));

  delegate_.GetInvalidator()->UpdateRegisteredIds(&handler, ids);
  EXPECT_EQ(ids, delegate_.GetFakeDelegate()->GetRegisteredIds(&handler));
  EXPECT_EQ(ids, delegate_.GetBridge()->GetRegisteredIdsForTest(&handler));

  delegate_.GetInvalidator()->UnregisterHandler(&handler);
  EXPECT_FALSE(delegate_.GetFakeDelegate()->IsHandlerRegistered(&handler));
  EXPECT_FALSE(delegate_.GetBridge()->IsHandlerRegisteredForTest(&handler));
}

TEST_F(BridgedInvalidatorTest, GetInvalidatorState) {
  EXPECT_EQ(syncer::DEFAULT_INVALIDATION_ERROR,
            delegate_.GetInvalidator()->GetInvalidatorState());
}

TEST_F(BridgedInvalidatorTest, SetUniqueId) {
  const std::string& unique_id = "unique id";
  delegate_.GetInvalidator()->SetUniqueId(unique_id);
  EXPECT_EQ(unique_id, delegate_.GetFakeDelegate()->GetUniqueId());
}

TEST_F(BridgedInvalidatorTest, SetStateDeprecated) {
  const std::string& state = "state";
  delegate_.GetInvalidator()->SetStateDeprecated(state);
  EXPECT_EQ(state, delegate_.GetFakeDelegate()->GetStateDeprecated());
}

TEST_F(BridgedInvalidatorTest, UpdateCredentials) {
  const std::string& email = "email";
  const std::string& token = "token";
  delegate_.GetInvalidator()->UpdateCredentials(email, token);
  EXPECT_EQ(email, delegate_.GetFakeDelegate()->GetCredentialsEmail());
  EXPECT_EQ(token, delegate_.GetFakeDelegate()->GetCredentialsToken());
}

TEST_F(BridgedInvalidatorTest, SendInvalidation) {
  syncer::ObjectIdSet ids;
  ids.insert(invalidation::ObjectId(1, "id1"));
  ids.insert(invalidation::ObjectId(2, "id2"));
  const syncer::ObjectIdStateMap& id_state_map =
      syncer::ObjectIdSetToStateMap(ids, "payload");
  delegate_.GetInvalidator()->SendInvalidation(id_state_map);
  EXPECT_THAT(id_state_map,
              Eq(delegate_.GetFakeDelegate()->GetLastSentIdStateMap()));
}

}  // namespace
}  // namespace browser_sync

namespace syncer {
namespace {

INSTANTIATE_TYPED_TEST_CASE_P(
    BridgedInvalidatorTest, InvalidatorTest,
    ::browser_sync::BridgedInvalidatorTestDelegate);

}  // namespace
}  // namespace syncer
