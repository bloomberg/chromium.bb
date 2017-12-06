// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/message_transfer_operation.h"

#include "base/timer/mock_timer.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto_test_util.h"
#include "chromeos/components/tether/timer_factory.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

// Arbitrarily chosen value. The MessageType used in this test does not matter
// except that it must be consistent throughout the test.
const MessageType kTestMessageType = MessageType::TETHER_AVAILABILITY_REQUEST;

const uint32_t kTestTimeoutSeconds = 5;

// A test double for MessageTransferOperation is needed because
// MessageTransferOperation has pure virtual methods which must be overridden in
// order to create a concrete instantiation of the class.
class TestOperation : public MessageTransferOperation {
 public:
  TestOperation(const std::vector<cryptauth::RemoteDevice>& devices_to_connect,
                BleConnectionManager* connection_manager)
      : MessageTransferOperation(devices_to_connect, connection_manager) {}
  ~TestOperation() override = default;

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

    if (should_unregister_device_on_message_received_)
      UnregisterDevice(remote_device);
  }

  void OnOperationStarted() override { has_operation_started_ = true; }

  void OnOperationFinished() override { has_operation_finished_ = true; }

  MessageType GetMessageTypeForConnection() override {
    return kTestMessageType;
  }

  uint32_t GetTimeoutSeconds() override { return timeout_seconds_; }

  void set_timeout_seconds(uint32_t timeout_seconds) {
    timeout_seconds_ = timeout_seconds;
  }

  void set_should_unregister_device_on_message_received(
      bool should_unregister_device_on_message_received) {
    should_unregister_device_on_message_received_ =
        should_unregister_device_on_message_received;
  }

  bool has_operation_started() { return has_operation_started_; }

  bool has_operation_finished() { return has_operation_finished_; }

 private:
  struct DeviceMapValue {
    DeviceMapValue() = default;
    ~DeviceMapValue() = default;

    bool has_device_authenticated;
    std::vector<std::shared_ptr<MessageWrapper>> received_messages;
  };

  std::map<cryptauth::RemoteDevice, DeviceMapValue> device_map_;

  uint32_t timeout_seconds_ = kTestTimeoutSeconds;
  bool should_unregister_device_on_message_received_ = false;
  bool has_operation_started_ = false;
  bool has_operation_finished_ = false;
};

class TestTimerFactory : public TimerFactory {
 public:
  ~TestTimerFactory() override = default;

  // TimerFactory:
  std::unique_ptr<base::Timer> CreateOneShotTimer() override {
    EXPECT_FALSE(device_id_for_next_timer_.empty());
    base::MockTimer* mock_timer = new base::MockTimer(
        false /* retain_user_task */, false /* is_repeating */);
    device_id_to_timer_map_[device_id_for_next_timer_] = mock_timer;
    return base::WrapUnique(mock_timer);
  }

  base::MockTimer* GetTimerForDeviceId(const std::string& device_id) {
    return device_id_to_timer_map_[device_id_for_next_timer_];
  }

  void set_device_id_for_next_timer(
      const std::string& device_id_for_next_timer) {
    device_id_for_next_timer_ = device_id_for_next_timer;
  }

 private:
  std::string device_id_for_next_timer_;
  std::unordered_map<std::string, base::MockTimer*> device_id_to_timer_map_;
};

TetherAvailabilityResponse CreateTetherAvailabilityResponse() {
  TetherAvailabilityResponse response;
  response.set_response_code(
      TetherAvailabilityResponse_ResponseCode::
          TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE);
  response.mutable_device_status()->CopyFrom(
      CreateDeviceStatusWithFakeFields());
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
    test_timer_factory_ = new TestTimerFactory();
    operation_ = base::WrapUnique(
        new TestOperation(remote_devices, fake_ble_connection_manager_.get()));
    operation_->SetTimerFactoryForTest(base::WrapUnique(test_timer_factory_));
    VerifyOperationStartedAndFinished(false /* has_started */,
                                      false /* has_finished */);
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

  void TransitionDeviceStatusFromDisconnectedToAuthenticated(
      const cryptauth::RemoteDevice& remote_device) {
    test_timer_factory_->set_device_id_for_next_timer(
        remote_device.GetDeviceId());

    fake_ble_connection_manager_->SetDeviceStatus(
        remote_device.GetDeviceId(),
        cryptauth::SecureChannel::Status::CONNECTING);
    fake_ble_connection_manager_->SetDeviceStatus(
        remote_device.GetDeviceId(),
        cryptauth::SecureChannel::Status::CONNECTED);
    fake_ble_connection_manager_->SetDeviceStatus(
        remote_device.GetDeviceId(),
        cryptauth::SecureChannel::Status::AUTHENTICATING);
    fake_ble_connection_manager_->SetDeviceStatus(
        remote_device.GetDeviceId(),
        cryptauth::SecureChannel::Status::AUTHENTICATED);
  }

  base::MockTimer* GetTimerForDevice(
      const cryptauth::RemoteDevice& remote_device) {
    return test_timer_factory_->GetTimerForDeviceId(
        remote_device.GetDeviceId());
  }

  void VerifyDefaultTimerCreatedForDevice(
      const cryptauth::RemoteDevice& remote_device) {
    VerifyTimerCreatedForDevice(remote_device, kTestTimeoutSeconds);
  }

  void VerifyTimerCreatedForDevice(const cryptauth::RemoteDevice& remote_device,
                                   uint32_t timeout_seconds) {
    EXPECT_TRUE(GetTimerForDevice(remote_device));
    EXPECT_EQ(base::TimeDelta::FromSeconds(timeout_seconds),
              GetTimerForDevice(remote_device)->GetCurrentDelay());
  }

  const std::vector<cryptauth::RemoteDevice> test_devices_;

  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  TestTimerFactory* test_timer_factory_;
  std::unique_ptr<TestOperation> operation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageTransferOperationTest);
};

TEST_F(MessageTransferOperationTest, TestCannotConnectAndReachesRetryLimit) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try to connect and fail. The device should still be registered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try and fail again. The device should still be registered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try and fail a third time. The maximum number of failures has been reached,
  // so the device should be unregistered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  VerifyOperationStartedAndFinished(true /* has_started */,
                                    true /* has_finished */);

  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest, TestFailsThenConnects) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try to connect and fail. The device should still be registered.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[0].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Try again and succeed.
  TransitionDeviceStatusFromDisconnectedToAuthenticated(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  EXPECT_TRUE(operation_->GetReceivedMessages(test_devices_[0]).empty());
}

TEST_F(MessageTransferOperationTest,
       TestSuccessfulConnectionAndReceiveMessage) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Simulate how subclasses behave after a successful response: unregister the
  // device.
  operation_->set_should_unregister_device_on_message_received(true);

  TransitionDeviceStatusFromDisconnectedToAuthenticated(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0].GetDeviceId(),
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, TestDevicesUnregisteredAfterDeletion) {
  ConstructOperation(test_devices_);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[1].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[2].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[3].GetDeviceId()));

  // Delete the operation. All registered devices should be unregistered.
  operation_.reset();
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[1].GetDeviceId()));
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[2].GetDeviceId()));
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[3].GetDeviceId()));
}

TEST_F(MessageTransferOperationTest,
       TestSuccessfulConnectionAndReceiveMessage_TimeoutSeconds) {
  const uint32_t timeout_seconds = 90;

  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  operation_->set_timeout_seconds(timeout_seconds);

  TransitionDeviceStatusFromDisconnectedToAuthenticated(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyTimerCreatedForDevice(test_devices_[0], timeout_seconds);

  EXPECT_EQ(base::TimeDelta::FromSeconds(timeout_seconds),
            GetTimerForDevice(test_devices_[0])->GetCurrentDelay());

  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0].GetDeviceId(),
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest, TestAuthenticatesButTimesOut) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  TransitionDeviceStatusFromDisconnectedToAuthenticated(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  GetTimerForDevice(test_devices_[0])->Fire();

  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->has_operation_finished());
}

TEST_F(MessageTransferOperationTest, TestRepeatedInputDevice) {
  // Construct with two copies of the same device.
  ConstructOperation(
      std::vector<cryptauth::RemoteDevice>{test_devices_[0], test_devices_[0]});
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  TransitionDeviceStatusFromDisconnectedToAuthenticated(test_devices_[0]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0].GetDeviceId(),
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
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));

  // Simulate the authentication of |test_devices_[1]|'s channel. Since the
  // operation was only constructed with |test_devices_[0]|, this operation
  // should not be affected.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[1].GetDeviceId(), MessageType::CONNECT_TETHERING_REQUEST);
  TransitionDeviceStatusFromDisconnectedToAuthenticated(test_devices_[1]);
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[1].GetDeviceId()));
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[1]));

  // Now, receive a message for |test_devices[1]|. Likewise, this operation
  // should not be affected.
  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[1].GetDeviceId(),
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_FALSE(operation_->GetReceivedMessages(test_devices_[0]).size());
}

TEST_F(MessageTransferOperationTest,
       TestAlreadyAuthenticatedBeforeInitialization) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});

  // Simulate the authentication of |test_devices_[0]|'s channel before
  // initialization.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[0].GetDeviceId(), MessageType::CONNECT_TETHERING_REQUEST);
  TransitionDeviceStatusFromDisconnectedToAuthenticated(test_devices_[0]);

  // Now initialize; the authentication handler should have been invoked.
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  // Receiving a message should work at this point.
  fake_ble_connection_manager_->ReceiveMessage(
      test_devices_[0].GetDeviceId(),
      MessageWrapper(CreateTetherAvailabilityResponse()).ToRawMessage());

  EXPECT_EQ(1u, operation_->GetReceivedMessages(test_devices_[0]).size());
  std::shared_ptr<MessageWrapper> message =
      operation_->GetReceivedMessages(test_devices_[0])[0];
  EXPECT_EQ(MessageType::TETHER_AVAILABILITY_RESPONSE,
            message->GetMessageType());
  EXPECT_EQ(CreateTetherAvailabilityResponse().SerializeAsString(),
            message->GetProto()->SerializeAsString());
}

TEST_F(MessageTransferOperationTest,
       AlreadyAuthenticatedBeforeInitialization_TimesOut) {
  ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});

  // Simulate the authentication of |test_devices_[0]|'s channel before
  // initialization.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[0].GetDeviceId(), MessageType::CONNECT_TETHERING_REQUEST);
  TransitionDeviceStatusFromDisconnectedToAuthenticated(test_devices_[0]);

  // Now initialize; the authentication handler should have been invoked.
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  GetTimerForDevice(test_devices_[0])->Fire();
  EXPECT_TRUE(operation_->has_operation_finished());

  // Should still be registered since it was also registered for the
  // CONNECT_TETHERING_REQUEST MessageType.
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
}

TEST_F(MessageTransferOperationTest, MultipleDevices) {
  ConstructOperation(test_devices_);
  InitializeOperation();
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[1].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[2].GetDeviceId()));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[3].GetDeviceId()));

  // Authenticate |test_devices_[0]|'s channel.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[0].GetDeviceId(), MessageType::CONNECT_TETHERING_REQUEST);
  TransitionDeviceStatusFromDisconnectedToAuthenticated(test_devices_[0]);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[0]));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[0].GetDeviceId()));
  VerifyDefaultTimerCreatedForDevice(test_devices_[0]);

  // Fail 3 times to connect to |test_devices_[1]|.
  test_timer_factory_->set_device_id_for_next_timer(
      test_devices_[1].GetDeviceId());
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[1].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[1]));
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[1].GetDeviceId()));
  EXPECT_FALSE(GetTimerForDevice(test_devices_[1]));

  // Authenticate |test_devices_[2]|'s channel.
  fake_ble_connection_manager_->RegisterRemoteDevice(
      test_devices_[2].GetDeviceId(), MessageType::CONNECT_TETHERING_REQUEST);
  TransitionDeviceStatusFromDisconnectedToAuthenticated(test_devices_[2]);
  EXPECT_TRUE(operation_->HasDeviceAuthenticated(test_devices_[2]));
  EXPECT_TRUE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[2].GetDeviceId()));
  VerifyDefaultTimerCreatedForDevice(test_devices_[2]);

  // Fail 3 times to connect to |test_devices_[3]|.
  test_timer_factory_->set_device_id_for_next_timer(
      test_devices_[3].GetDeviceId());
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3].GetDeviceId(),
      cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3].GetDeviceId(),
      cryptauth::SecureChannel::Status::DISCONNECTED);
  EXPECT_FALSE(operation_->HasDeviceAuthenticated(test_devices_[3]));
  EXPECT_FALSE(fake_ble_connection_manager_->IsRegistered(
      test_devices_[3].GetDeviceId()));
  EXPECT_FALSE(GetTimerForDevice(test_devices_[3]));
}

}  // namespace tether

}  // namespace chromeos
