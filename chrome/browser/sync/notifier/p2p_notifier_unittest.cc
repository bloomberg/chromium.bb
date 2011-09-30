// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/p2p_notifier.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/notifier/mock_sync_notifier_observer.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;

class FakeTalkMediator : public notifier::TalkMediator {
 public:
  FakeTalkMediator() : delegate_(NULL) {}
  virtual ~FakeTalkMediator() {}

  // notifier::TalkMediator implementation.
  virtual void SetDelegate(Delegate* delegate) OVERRIDE {
    delegate_ = delegate;
  }
  virtual void SetAuthToken(const std::string& email,
                            const std::string& token,
                            const std::string& token_service) OVERRIDE {}
  virtual bool Login() OVERRIDE {
    if (delegate_) {
      delegate_->OnNotificationStateChange(true /* notifications_enabled */);
    }
    return true;
  }
  virtual bool Logout() OVERRIDE {
    if (delegate_) {
      delegate_->OnNotificationStateChange(false /* notifiations_enabled */);
    }
    return true;
  }
  virtual void SendNotification(const notifier::Notification& data) OVERRIDE {
    if (delegate_) {
      delegate_->OnOutgoingNotification();
      delegate_->OnIncomingNotification(data);
    }
  }
  virtual void AddSubscription(
      const notifier::Subscription& subscription) OVERRIDE {}

 private:
  Delegate* delegate_;
};

class P2PNotifierTest : public testing::Test {
 protected:
  P2PNotifierTest() : talk_mediator_(NULL) {}

  virtual void SetUp() {
    talk_mediator_ = new FakeTalkMediator();
    p2p_notifier_.reset(new P2PNotifier(talk_mediator_, NOTIFY_OTHERS));
    p2p_notifier_->AddObserver(&mock_observer_);
  }

  virtual void TearDown() {
    p2p_notifier_->RemoveObserver(&mock_observer_);
    p2p_notifier_.reset();
    talk_mediator_ = NULL;
  }

  syncable::ModelTypePayloadMap MakePayloadMap(
      const syncable::ModelTypeSet& types) {
    return syncable::ModelTypePayloadMapFromBitSet(
        syncable::ModelTypeBitSetFromSet(types), "");
  }

  MessageLoop message_loop_;
  // Owned by |p2p_notifier_|.
  notifier::TalkMediator* talk_mediator_;
  scoped_ptr<P2PNotifier> p2p_notifier_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;
};

TEST_F(P2PNotifierTest, P2PNotificationTarget) {
  for (int i = FIRST_NOTIFICATION_TARGET;
       i <= LAST_NOTIFICATION_TARGET; ++i) {
    P2PNotificationTarget target = static_cast<P2PNotificationTarget>(i);
    const std::string& target_str = P2PNotificationTargetToString(target);
    EXPECT_FALSE(target_str.empty());
    EXPECT_EQ(target, P2PNotificationTargetFromString(target_str));
  }
  EXPECT_EQ(NOTIFY_SELF, P2PNotificationTargetFromString("unknown"));
}

TEST_F(P2PNotifierTest, P2PNotificationDataIsTargeted) {
  {
    const P2PNotificationData notification_data(
        "sender", NOTIFY_SELF, syncable::ModelTypeSet());
    EXPECT_TRUE(notification_data.IsTargeted("sender"));
    EXPECT_FALSE(notification_data.IsTargeted("other1"));
    EXPECT_FALSE(notification_data.IsTargeted("other2"));
  }
  {
    const P2PNotificationData notification_data(
        "sender", NOTIFY_OTHERS, syncable::ModelTypeSet());
    EXPECT_FALSE(notification_data.IsTargeted("sender"));
    EXPECT_TRUE(notification_data.IsTargeted("other1"));
    EXPECT_TRUE(notification_data.IsTargeted("other2"));
  }
  {
    const P2PNotificationData notification_data(
        "sender", NOTIFY_ALL, syncable::ModelTypeSet());
    EXPECT_TRUE(notification_data.IsTargeted("sender"));
    EXPECT_TRUE(notification_data.IsTargeted("other1"));
    EXPECT_TRUE(notification_data.IsTargeted("other2"));
  }
}

TEST_F(P2PNotifierTest, P2PNotificationDataDefault) {
  const P2PNotificationData notification_data;
  EXPECT_TRUE(notification_data.IsTargeted(""));
  EXPECT_FALSE(notification_data.IsTargeted("other1"));
  EXPECT_FALSE(notification_data.IsTargeted("other2"));
  EXPECT_TRUE(notification_data.GetChangedTypes().empty());
  const std::string& notification_data_str = notification_data.ToString();
  EXPECT_EQ(
      "{\"changedTypes\":[],\"notificationType\":\"notifySelf\","
      "\"senderId\":\"\"}", notification_data_str);

  P2PNotificationData notification_data_parsed;
  EXPECT_TRUE(notification_data_parsed.ResetFromString(notification_data_str));
  EXPECT_TRUE(notification_data.Equals(notification_data_parsed));
}

TEST_F(P2PNotifierTest, P2PNotificationDataNonDefault) {
  syncable::ModelTypeSet changed_types;
  changed_types.insert(syncable::BOOKMARKS);
  changed_types.insert(syncable::THEMES);
  const P2PNotificationData notification_data(
      "sender", NOTIFY_ALL, changed_types);
  EXPECT_TRUE(notification_data.IsTargeted("sender"));
  EXPECT_TRUE(notification_data.IsTargeted("other1"));
  EXPECT_TRUE(notification_data.IsTargeted("other2"));
  EXPECT_EQ(changed_types, notification_data.GetChangedTypes());
  const std::string& notification_data_str = notification_data.ToString();
  EXPECT_EQ(
      "{\"changedTypes\":[\"Bookmarks\",\"Themes\"],"
      "\"notificationType\":\"notifyAll\","
      "\"senderId\":\"sender\"}", notification_data_str);

  P2PNotificationData notification_data_parsed;
  EXPECT_TRUE(notification_data_parsed.ResetFromString(notification_data_str));
  EXPECT_TRUE(notification_data.Equals(notification_data_parsed));
}

TEST_F(P2PNotifierTest, NotificationsBasic) {
  syncable::ModelTypeSet enabled_types;
  enabled_types.insert(syncable::BOOKMARKS);
  enabled_types.insert(syncable::PREFERENCES);

  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  EXPECT_CALL(mock_observer_,
              OnIncomingNotification(MakePayloadMap(enabled_types)));

  p2p_notifier_->SetUniqueId("sender");
  p2p_notifier_->UpdateCredentials("foo@bar.com", "fake_token");
  p2p_notifier_->UpdateEnabledTypes(enabled_types);
  // Sent with target NOTIFY_OTHERS so should not be propagated to
  // |mock_observer_|.
  {
    syncable::ModelTypeSet changed_types;
    enabled_types.insert(syncable::THEMES);
    enabled_types.insert(syncable::APPS);
    p2p_notifier_->SendNotification(changed_types);
  }
}

TEST_F(P2PNotifierTest, SendNotificationData) {
  syncable::ModelTypeSet enabled_types;
  enabled_types.insert(syncable::BOOKMARKS);
  enabled_types.insert(syncable::PREFERENCES);

  syncable::ModelTypeSet changed_types;
  changed_types.insert(syncable::THEMES);
  changed_types.insert(syncable::APPS);

  const syncable::ModelTypePayloadMap& changed_payload_map =
      MakePayloadMap(changed_types);

  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  EXPECT_CALL(mock_observer_,
              OnIncomingNotification(MakePayloadMap(enabled_types)));

  p2p_notifier_->SetUniqueId("sender");
  p2p_notifier_->UpdateCredentials("foo@bar.com", "fake_token");
  p2p_notifier_->UpdateEnabledTypes(enabled_types);

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_->SendNotificationDataForTest(P2PNotificationData());

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map));
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_SELF, changed_types));

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_SELF, changed_types));

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_SELF, syncable::ModelTypeSet()));

  // Should be dropped.
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_OTHERS, changed_types));

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map));
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_OTHERS, changed_types));

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_OTHERS, syncable::ModelTypeSet()));

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map));
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_ALL, changed_types));

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map));
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_ALL, changed_types));

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_ALL, syncable::ModelTypeSet()));
}

}  // namespace

}  // namespace sync_notifier
