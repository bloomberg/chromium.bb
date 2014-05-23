// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/log_router.h"

#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace password_manager {

namespace {

const char kTestText[] = "abcd1234";

class MockLogReceiver : public LogReceiver {
 public:
  MockLogReceiver() {}

  MOCK_METHOD1(LogSavePasswordProgress, void(const std::string&));
};

class MockClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD1(OnLogRouterAvailabilityChanged, void(bool));
};

}  // namespace

class LogRouterTest : public testing::Test {
 protected:
  testing::StrictMock<MockLogReceiver> receiver_;
  testing::StrictMock<MockLogReceiver> receiver2_;
  testing::StrictMock<MockClient> client_;
};

TEST_F(LogRouterTest, ProcessLog_NoReceiver) {
  LogRouter router;
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver_));
  EXPECT_CALL(receiver_, LogSavePasswordProgress(kTestText)).Times(1);
  router.ProcessLog(kTestText);
  router.UnregisterReceiver(&receiver_);
  // Without receivers, accumulated logs should not have been kept. That means
  // that on the registration of the first receiver, none are returned.
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver_));
  router.UnregisterReceiver(&receiver_);
}

TEST_F(LogRouterTest, ProcessLog_OneReceiver) {
  LogRouter router;
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver_));
  // Check that logs generated after activation are passed.
  EXPECT_CALL(receiver_, LogSavePasswordProgress(kTestText)).Times(1);
  router.ProcessLog(kTestText);
  router.UnregisterReceiver(&receiver_);
}

TEST_F(LogRouterTest, ProcessLog_TwoReceiversAccumulatedLogsPassed) {
  LogRouter router;
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver_));

  // Log something with only the first receiver, to accumulate some logs.
  EXPECT_CALL(receiver_, LogSavePasswordProgress(kTestText)).Times(1);
  EXPECT_CALL(receiver2_, LogSavePasswordProgress(kTestText)).Times(0);
  router.ProcessLog(kTestText);
  // Accumulated logs get passed on registration.
  EXPECT_EQ(kTestText, router.RegisterReceiver(&receiver2_));
  router.UnregisterReceiver(&receiver_);
  router.UnregisterReceiver(&receiver2_);
}

TEST_F(LogRouterTest, ProcessLog_TwoReceiversBothUpdated) {
  LogRouter router;
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver_));
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver2_));

  // Check that both receivers get log updates.
  EXPECT_CALL(receiver_, LogSavePasswordProgress(kTestText)).Times(1);
  EXPECT_CALL(receiver2_, LogSavePasswordProgress(kTestText)).Times(1);
  router.ProcessLog(kTestText);
  router.UnregisterReceiver(&receiver2_);
  router.UnregisterReceiver(&receiver_);
}

TEST_F(LogRouterTest, ProcessLog_TwoReceiversNoUpdateAfterUnregistering) {
  LogRouter router;
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver_));
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver2_));

  // Check that no logs are passed to an unregistered receiver.
  router.UnregisterReceiver(&receiver_);
  EXPECT_CALL(receiver_, LogSavePasswordProgress(_)).Times(0);
  EXPECT_CALL(receiver2_, LogSavePasswordProgress(kTestText)).Times(1);
  router.ProcessLog(kTestText);
  router.UnregisterReceiver(&receiver2_);
}

TEST_F(LogRouterTest, RegisterClient_NoReceivers) {
  LogRouter router;
  EXPECT_FALSE(router.RegisterClient(&client_));
  router.UnregisterClient(&client_);
}

TEST_F(LogRouterTest, RegisterClient_OneReceiverBeforeClient) {
  LogRouter router;
  // First register a receiver.
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver_));
  // The client should be told the LogRouter has some receivers.
  EXPECT_TRUE(router.RegisterClient(&client_));
  // Now unregister the reciever. The client should be told the LogRouter has no
  // receivers.
  EXPECT_CALL(client_, OnLogRouterAvailabilityChanged(false)).Times(1);
  router.UnregisterReceiver(&receiver_);
  router.UnregisterClient(&client_);
}

TEST_F(LogRouterTest, RegisterClient_OneClientBeforeReceiver) {
  LogRouter router;
  // First register a client; the client should be told the LogRouter has no
  // receivers.
  EXPECT_FALSE(router.RegisterClient(&client_));
  // Now register the receiver. The client should be notified.
  EXPECT_CALL(client_, OnLogRouterAvailabilityChanged(true)).Times(1);
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver_));
  // Now unregister the client.
  router.UnregisterClient(&client_);
  // Now unregister the reciever. The client should not hear about it.
  EXPECT_CALL(client_, OnLogRouterAvailabilityChanged(_)).Times(0);
  router.UnregisterReceiver(&receiver_);
}

TEST_F(LogRouterTest, RegisterClient_OneClientTwoReceivers) {
  LogRouter router;
  // First register a client; the client should be told the LogRouter has no
  // receivers.
  EXPECT_FALSE(router.RegisterClient(&client_));
  // Now register the 1st receiver. The client should be notified.
  EXPECT_CALL(client_, OnLogRouterAvailabilityChanged(true)).Times(1);
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver_));
  // Now register the 2nd receiver. The client should not be notified.
  EXPECT_CALL(client_, OnLogRouterAvailabilityChanged(true)).Times(0);
  EXPECT_EQ(std::string(), router.RegisterReceiver(&receiver2_));
  // Now unregister the 1st reciever. The client should not hear about it.
  EXPECT_CALL(client_, OnLogRouterAvailabilityChanged(false)).Times(0);
  router.UnregisterReceiver(&receiver_);
  // Now unregister the 2nd reciever. The client should hear about it.
  EXPECT_CALL(client_, OnLogRouterAvailabilityChanged(false)).Times(1);
  router.UnregisterReceiver(&receiver2_);
  // Now unregister the client.
  router.UnregisterClient(&client_);
}

}  // namespace password_manager
