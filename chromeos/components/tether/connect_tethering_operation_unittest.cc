// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/connect_tethering_operation.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "chromeos/components/tether/ble_constants.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

const char kTestSsid[] = "testSsid";
const char kTestPassword[] = "testPassword";

class TestObserver : public ConnectTetheringOperation::Observer {
 public:
  TestObserver() : has_received_failure(false) {}

  void OnSuccessfulConnectTetheringResponse(
      const std::string& ssid,
      const std::string& password) override {
    this->ssid = ssid;
    this->password = password;
  }

  void OnConnectTetheringFailure(
      ConnectTetheringResponse_ResponseCode error_code) override {
    has_received_failure = true;
    this->error_code = error_code;
  }

  std::string ssid;
  std::string password;

  bool has_received_failure;
  ConnectTetheringResponse_ResponseCode error_code;
};

std::string CreateConnectTetheringRequestString() {
  ConnectTetheringRequest request;
  return MessageWrapper(request).ToRawMessage();
}

DeviceStatus CreateFakeDeviceStatus() {
  WifiStatus wifi_status;
  wifi_status.set_status_code(
      WifiStatus_StatusCode::WifiStatus_StatusCode_NOT_CONNECTED);

  DeviceStatus device_status;
  device_status.set_battery_percentage(75);
  device_status.set_cell_provider("Google Fi");
  device_status.set_connection_strength(4);
  device_status.mutable_wifi_status()->CopyFrom(wifi_status);

  return device_status;
}

std::string CreateConnectTetheringResponseString(
    ConnectTetheringResponse_ResponseCode response_code,
    bool use_proto_without_ssid_and_password) {
  ConnectTetheringResponse response;
  response.set_response_code(response_code);

  // Only set SSID/password if |response_code| is SUCCESS.
  if (!use_proto_without_ssid_and_password &&
      response_code == ConnectTetheringResponse_ResponseCode::
                           ConnectTetheringResponse_ResponseCode_SUCCESS) {
    response.set_ssid(std::string(kTestSsid));
    response.set_password(std::string(kTestPassword));
  }

  response.mutable_device_status()->CopyFrom(CreateFakeDeviceStatus());

  return MessageWrapper(response).ToRawMessage();
}

}  // namespace

class ConnectTetheringOperationTest : public testing::Test {
 protected:
  ConnectTetheringOperationTest()
      : connect_tethering_request_string_(
            CreateConnectTetheringRequestString()),
        test_device_(cryptauth::GenerateTestRemoteDevices(1)[0]) {
    // These tests are written under the assumption that there are a maximum of
    // 3 connection attempts; they need to be edited if this value changes.
    EXPECT_EQ(3u, MessageTransferOperation::kMaxConnectionAttemptsPerDevice);
  }

  void SetUp() override {
    fake_ble_connection_manager_ = base::MakeUnique<FakeBleConnectionManager>();
    test_observer_ = base::WrapUnique(new TestObserver());

    operation_ = base::WrapUnique(new ConnectTetheringOperation(
        test_device_, fake_ble_connection_manager_.get()));
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
    EXPECT_EQ(connect_tethering_request_string_, sent_messages[0].message);
  }

  void SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode response_code,
      bool use_proto_without_ssid_and_password) {
    fake_ble_connection_manager_->ReceiveMessage(
        test_device_, CreateConnectTetheringResponseString(
                          response_code, use_proto_without_ssid_and_password));

    bool is_success_response =
        response_code == ConnectTetheringResponse_ResponseCode::
                             ConnectTetheringResponse_ResponseCode_SUCCESS;
    ConnectTetheringResponse_ResponseCode expected_response_code;
    if (is_success_response && use_proto_without_ssid_and_password) {
      expected_response_code = ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR;
    } else if (is_success_response && !use_proto_without_ssid_and_password) {
      expected_response_code = ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS;
    } else {
      expected_response_code = response_code;
    }

    if (expected_response_code ==
        ConnectTetheringResponse_ResponseCode::
            ConnectTetheringResponse_ResponseCode_SUCCESS) {
      EXPECT_EQ(std::string(kTestSsid), test_observer_->ssid);
      EXPECT_EQ(std::string(kTestPassword), test_observer_->password);
    } else {
      EXPECT_TRUE(test_observer_->has_received_failure);
      EXPECT_EQ(expected_response_code, test_observer_->error_code);
    }
  }

  const std::string connect_tethering_request_string_;
  const cryptauth::RemoteDevice test_device_;

  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<TestObserver> test_observer_;
  std::unique_ptr<ConnectTetheringOperation> operation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectTetheringOperationTest);
};

TEST_F(ConnectTetheringOperationTest, TestOperation_SuccessButInvalidResponse) {
  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS,
      true /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_SuccessWithValidResponse) {
  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS,
      false /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_UnknownError) {
  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR,
      false /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_ProvisioningFailed) {
  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_PROVISIONING_FAILED,
      false /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestCannotConnect) {
  // Simulate the device failing to connect.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_device_, cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_device_, cryptauth::SecureChannel::Status::DISCONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_device_, cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_device_, cryptauth::SecureChannel::Status::DISCONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_device_, cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_device_, cryptauth::SecureChannel::Status::DISCONNECTED);

  // The maximum number of connection failures has occurred.
  EXPECT_TRUE(test_observer_->has_received_failure);
  EXPECT_EQ(ConnectTetheringResponse_ResponseCode::
                ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR,
            test_observer_->error_code);
}

}  // namespace tether

}  // namespace cryptauth
