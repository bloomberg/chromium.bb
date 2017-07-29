// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/disconnect_tethering_operation.h"

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

class TestObserver : public DisconnectTetheringOperation::Observer {
 public:
  TestObserver() : success_(false) {}

  virtual ~TestObserver() {}

  std::string last_device_id() { return last_device_id_; }

  bool WasLastOperationSuccessful() {
    EXPECT_TRUE(!last_device_id_.empty());
    return success_;
  }

  // DisconnectTetheringOperation::Observer:
  void OnOperationFinished(const std::string& device_id,
                           bool success) override {
    last_device_id_ = device_id;
    success_ = success;
  }

 private:
  std::string last_device_id_;
  bool success_;
};

std::string CreateDisconnectTetheringString() {
  DisconnectTetheringRequest request;
  return MessageWrapper(request).ToRawMessage();
}

}  // namespace

class DisconnectTetheringOperationTest : public testing::Test {
 protected:
  DisconnectTetheringOperationTest()
      : disconnect_tethering_request_string_(CreateDisconnectTetheringString()),
        test_device_(cryptauth::GenerateTestRemoteDevices(1)[0]) {}

  void SetUp() override {
    fake_ble_connection_manager_ = base::MakeUnique<FakeBleConnectionManager>();

    operation_ = base::WrapUnique(new DisconnectTetheringOperation(
        test_device_, fake_ble_connection_manager_.get()));

    test_observer_ = base::WrapUnique(new TestObserver());
    operation_->AddObserver(test_observer_.get());

    operation_->Initialize();
  }

  void SimulateDeviceAuthenticationAndVerifyMessageSent() {
    operation_->OnDeviceAuthenticated(test_device_);

    // Verify that the message was passed to |fake_ble_connection_manager_|
    // correctly.
    std::vector<FakeBleConnectionManager::SentMessage>& sent_messages =
        fake_ble_connection_manager_->sent_messages();
    ASSERT_EQ(1u, sent_messages.size());
    EXPECT_EQ(test_device_, sent_messages[0].remote_device);
    EXPECT_EQ(disconnect_tethering_request_string_, sent_messages[0].message);

    // Now, simulate the message being sent.
    int last_sequence_number =
        fake_ble_connection_manager_->last_sequence_number();
    EXPECT_NE(last_sequence_number, -1);
    fake_ble_connection_manager_->SetMessageSent(last_sequence_number);
  }

  void SimulateConnectionTimeout() {
    operation_->UnregisterDevice(test_device_);
  }

  const std::string disconnect_tethering_request_string_;
  const cryptauth::RemoteDevice test_device_;

  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<DisconnectTetheringOperation> operation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisconnectTetheringOperationTest);
};

TEST_F(DisconnectTetheringOperationTest, TestSuccess) {
  SimulateDeviceAuthenticationAndVerifyMessageSent();
  EXPECT_EQ(test_device_.GetDeviceId(), test_observer_->last_device_id());
  EXPECT_TRUE(test_observer_->WasLastOperationSuccessful());
}

TEST_F(DisconnectTetheringOperationTest, TestFailure) {
  SimulateConnectionTimeout();
  EXPECT_EQ(test_device_.GetDeviceId(), test_observer_->last_device_id());
  EXPECT_FALSE(test_observer_->WasLastOperationSuccessful());
}

}  // namespace tether

}  // namespace cryptauth
