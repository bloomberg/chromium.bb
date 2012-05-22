// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_client.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/base/notifier_options.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;

class MockObserver : public PushClient::Observer {
 public:
  MOCK_METHOD1(OnNotificationStateChange, void(bool));
  MOCK_METHOD1(OnIncomingNotification, void(const Notification&));
};

}  // namespace

class PushClientTest : public testing::Test {
 protected:
  PushClientTest() {
    notifier_options_.request_context_getter =
        new TestURLRequestContextGetter(message_loop_.message_loop_proxy());
  }

  virtual ~PushClientTest() {}

  virtual void SetUp() OVERRIDE {
    push_client_.reset(new PushClient(notifier_options_));
    push_client_->AddObserver(&mock_observer_);
  }

  virtual void TearDown() OVERRIDE {
    // Clear out any messages posted by PushClient.
    message_loop_.RunAllPending();
    push_client_->RemoveObserver(&mock_observer_);
    push_client_.reset();
  }

  // The sockets created by the XMPP code expect an IO loop.
  MessageLoopForIO message_loop_;
  NotifierOptions notifier_options_;
  StrictMock<MockObserver> mock_observer_;
  scoped_ptr<PushClient> push_client_;
  FakeBaseTask fake_base_task_;
};

TEST_F(PushClientTest, OnIncomingNotification) {
  EXPECT_CALL(mock_observer_, OnIncomingNotification(_));
  push_client_->SimulateOnNotificationReceivedForTest(Notification());
}

TEST_F(PushClientTest, ConnectAndSubscribe) {
  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  push_client_->SimulateConnectAndSubscribeForTest(
      fake_base_task_.AsWeakPtr());
}

TEST_F(PushClientTest, Disconnect) {
  EXPECT_CALL(mock_observer_, OnNotificationStateChange(false));
  push_client_->SimulateDisconnectForTest();
}

TEST_F(PushClientTest, SubscriptionError) {
  EXPECT_CALL(mock_observer_, OnNotificationStateChange(false));
  push_client_->SimulateSubscriptionErrorForTest();
}

TEST_F(PushClientTest, SendNotification) {
  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  EXPECT_CALL(mock_observer_, OnIncomingNotification(_));

  push_client_->SimulateConnectAndSubscribeForTest(
      fake_base_task_.AsWeakPtr());
  push_client_->ReflectSentNotificationsForTest();
  push_client_->SendNotification(Notification());
}

TEST_F(PushClientTest, SendNotificationPending) {
  push_client_->ReflectSentNotificationsForTest();
  push_client_->SendNotification(Notification());

  Mock::VerifyAndClearExpectations(&mock_observer_);

  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  EXPECT_CALL(mock_observer_, OnIncomingNotification(_));

  push_client_->SimulateConnectAndSubscribeForTest(
      fake_base_task_.AsWeakPtr());
}

}  // namespace notifier
