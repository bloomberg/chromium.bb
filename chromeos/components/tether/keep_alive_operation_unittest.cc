// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/keep_alive_operation.h"

#include <memory>
#include <vector>

#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

class TestObserver : public KeepAliveOperation::Observer {
 public:
  TestObserver() : has_run_callback_(false) {}

  virtual ~TestObserver() {}

  bool has_run_callback() { return has_run_callback_; }

  void OnOperationFinished() override { has_run_callback_ = true; }

 private:
  bool has_run_callback_;
};

std::string CreateKeepAliveTickleString() {
  KeepAliveTickle tickle;
  return MessageWrapper(tickle).ToRawMessage();
}

}  // namespace

class KeepAliveOperationTest : public testing::Test {
 protected:
  KeepAliveOperationTest()
      : keep_alive_tickle_string_(CreateKeepAliveTickleString()),
        test_device_(cryptauth::GenerateTestRemoteDevices(1)[0]) {}

  void SetUp() override {
    fake_ble_connection_manager_ = base::MakeUnique<FakeBleConnectionManager>();

    operation_ = base::WrapUnique(new KeepAliveOperation(
        test_device_, fake_ble_connection_manager_.get()));

    test_observer_ = base::WrapUnique(new TestObserver());
    operation_->AddObserver(test_observer_.get());

    operation_->Initialize();
  }

  void SimulateDeviceAuthenticationAndVerifyMessageSent() {
    operation_->OnDeviceAuthenticated(test_device_);

    // Verify that the message was sent successfully.
    std::vector<FakeBleConnectionManager::SentMessage>& sent_messages =
        fake_ble_connection_manager_->sent_messages();
    ASSERT_EQ(1u, sent_messages.size());
    EXPECT_EQ(test_device_, sent_messages[0].remote_device);
    EXPECT_EQ(keep_alive_tickle_string_, sent_messages[0].message);
  }

  const std::string keep_alive_tickle_string_;
  const cryptauth::RemoteDevice test_device_;

  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<KeepAliveOperation> operation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeepAliveOperationTest);
};

TEST_F(KeepAliveOperationTest, TestSendsKeepAliveTickle) {
  EXPECT_FALSE(test_observer_->has_run_callback());
  SimulateDeviceAuthenticationAndVerifyMessageSent();
  EXPECT_TRUE(test_observer_->has_run_callback());
}

}  // namespace tether

}  // namespace cryptauth
