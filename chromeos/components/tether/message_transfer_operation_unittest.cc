// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/message_transfer_operation.h"

#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

// Arbitrarily chosen value. The MessageType used in this test does not matter
// except that it must be consistent throughout the test.
const MessageType kTestMessageType = MessageType::TETHER_AVAILABILITY_REQUEST;

// A test double for MessageTransferOperation is needed because
// MessageTransferOperation has pure virtual methods which must be overridden in
// order to create a concrete instantiation of the class.
class TestOperation : public MessageTransferOperation {
 public:
  TestOperation(const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
                BleConnectionManager* connection_manager)
      : MessageTransferOperation(devices_to_connect, connection_manager),
        has_operation_started_(false),
        has_operation_finished_(false) {}
  ~TestOperation() override {}

  bool HasDeviceAuthenticated(const cryptauth::RemoteDevice& remote_device) {
    const auto iter = device_map_.find(remote_device);
    if (iter == device_map_.end()) {
      return false;
    }

    return iter->second.has_device_authenticated;
  }

  std::vector<std::shared_ptr<MessageWrapper>> GetReceivedMessages(
      const cryptauth::RemoteDevice& remote_device) {
    const auto iter = device_map_.find(remote_device);
    if (iter == device_map_.end()) {
      return std::vector<std::shared_ptr<MessageWrapper>>();
    }

    return iter->second.received_messages;
  }

  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      const cryptauth::RemoteDevice& remote_device) override {
    device_map_[remote_device].has_device_authenticated = true;
  }

  void OnMessageReceived(
      std::unique_ptr<MessageWrapper> message_wrapper,
      const cryptauth::RemoteDevice& remote_device) override {
    device_map_[remote_device].received_messages.push_back(
        std::move(message_wrapper));
  }

  void OnOperationStarted() override { has_operation_started_ = true; }

  void OnOperationFinished() override { has_operation_finished_ = true; }

  MessageType GetMessageTypeForConnection() override {
    return kTestMessageType;
  }

  bool has_operation_started() { return has_operation_started_; }

  bool has_operation_finished() { return has_operation_finished_; }

 private:
  struct DeviceMapValue {
    DeviceMapValue() {}
    ~DeviceMapValue() {}

    bool has_device_authenticated;
    std::vector<std::shared_ptr<MessageWrapper>> received_messages;
  };

  std::map<cryptauth::RemoteDevice, DeviceMapValue> device_map_;

  bool has_operation_started_;
  bool has_operation_finished_;
};

DeviceStatus CreateFakeDeviceStatus() {
  WifiStatus wifi_status;
  wifi_status.set_status_code(
      WifiStatus_StatusCode::WifiStatus_StatusCode_CONNECTED);
  wifi_status.set_ssid("Google A");

  DeviceStatus device_status;
  device_status.set_battery_percentage(75);
  device_status.set_cell_provider("Google Fi");
  device_status.set_connection_strength(4);
  device_status.mutable_wifi_status()->CopyFrom(wifi_status);

  return device_status;
}

TetherAvailabilityResponse CreateTetherAvailabilityResponse() {
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode::
          TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE);
  response.mutable_device_status()->CopyFrom(CreateFakeDeviceStatus());
  return response;
}

}  // namespace

class MessageTransferOperationTest : public testing::Test {
 protected:
  MessageTransferOperationTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(4)) {
    // These tests are written under the assumption that there are a maximum of
    // 3 connection attempts; they need to be edited if this value changes.
    EXPECT_EQ(3u, MessageTransferOperation::kMaxConnectionAttemptsPerDevice);
  }

  void SetUp() override {
    fake_ble_connection_manager_ = base::MakeUnique<FakeBleConnectionManager>();
  }

  void ConstructOperation(std::vector<cryptauth::RemoteDevice> remote_devices) {
    operation_ = base::WrapUnique(
        new TestOperation(remote_devices, fake_ble_connection_manager_.get()));
    VerifyOperationStartedAndFinished(false /* has_started */,
                                      false /* has_finished */);
  }

  bool IsDeviceRegistered(const cryptauth::RemoteDevice& remote_device) const {
    DCHECK(operation_);
    return std::find(operation_->remote_devices_.begin(),
                     operation_->remote_devices_.end(),
                     remote_device) != operation_->remote_devices_.end();
  }

  void InitializeOperation() {
    VerifyOperationStartedAndFinished(false /* has_started */,
                                      false /* has_finished */);
    operation_->Initialize();
    VerifyOperationStartedAndFinished(true /* has_started */,
                                      false /* has_finished */);
  }

  void VerifyOperationStartedAndFinished(bool has_started, bool has_finished) {
    EXPECT_EQ(has_started, operation_->has_operation_started());
    EXPECT_EQ(has_finished, operation_->has_operation_finished());
  }

  const std::vector<cryptauth::RemoteDevice> test_devices_;

  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<TestOperation> operation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageTransferOperationTest);
};

TEST_F(MessageTransferOperationTest, TestCannotConnectAndReachesRetryLimit) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));

  // Try to connect and fail. The device should still be registered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));

  // Try and fail again. The device should still be registered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));

  // Try and fail a third time. The maximum number of failures has been reached,
  // so the device should be unregistered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[0]));
  VerifyOperationStartedAndFinished(true /* has_started */,
                                    true /* has_finished */);

  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest, TestFailsAuthentication) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));

  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED);

  // When authentication fails, we consider this a fatal error; the device
  // should be unregistered.
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[0]));
  VerifyOperationStartedAndFinished(true /* has_started */,
                                    true /* has_finished */);

  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest, TestFailsThenConnects) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));

  // Try to connect and fail. The device should still be registered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));

  // Try again and succeed.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED);
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));

  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest,
       TestSuccessfulConnectionAndReceiveMessage) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));

  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED);
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));

  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0],
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, TestRepeatedInputDevice) {
  // Construct with two copies of the same device.
  ConstructOperation(
      std::vector<cryptauth::RemoteDevice>{test_devices_[0], test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));

  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED);
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));

  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0],
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  // Should still have received only one message even though the device was
  // repeated twice in the constructor.
  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, TestReceiveEventForOtherDevice) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));

  // Simulate the authentication of |test_devices_[1]|'s channel. Since the
  // operation was only constructed with |test_devices_[0]|, this operation
  // should not be affected.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[1], MessageType::CONNECT_TETHERING_REQUEST);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1], cryptauth::SecureChannel::Status::CONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1], cryptauth::SecureChannel::Status::AUTHENTICATING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1], cryptauth::SecureChannel::Status::AUTHENTICATED);
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[1]));
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[1]));

  // Now, receive a message for |test_devices[1]|. Likewise, this operation
  // should not be affected.
  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[1],
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_FALSE(operation_->GetReceivedMessages(test_devices_[0]).size());
}

TEST_F(MessageTransferOperationTest,
       TestAlreadyAuthenticatedBeforeInitialization) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});

  // Simulate the authentication of |test_devices_[0]|'s channel before
  // initialization.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[0], MessageType::CONNECT_TETHERING_REQUEST);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED);

  // Now initialize; the authentication handler should have been invoked.
  InitializeOperation();
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));

  // Receiving a message should work at this point.
  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0],
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, MultipleDevices) {
  ConstructOperation(test_devices_);
  InitializeOperation();
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[1]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[2]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[3]));

  // Authenticate |test_devices_[0]|'s channel.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[0], MessageType::CONNECT_TETHERING_REQUEST);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::CONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0]));

  // Fail 3 times to connect to |test_devices_[1]|.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[1]));
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[1]));

  // Authenticate |test_devices_[2]|'s channel.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[2], MessageType::CONNECT_TETHERING_REQUEST);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[2], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[2], cryptauth::SecureChannel::Status::CONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[2], cryptauth::SecureChannel::Status::AUTHENTICATING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[2], cryptauth::SecureChannel::Status::AUTHENTICATED);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[2]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[2]));

  // Fail to authenticate |test_devices_[3]|'s channel.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[3], MessageType::CONNECT_TETHERING_REQUEST);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3], cryptauth::SecureChannel::Status::CONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3], cryptauth::SecureChannel::Status::AUTHENTICATING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3], cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[3]));
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[3]));
}

}  // namespace tether

}  // namespace chromeos
