// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/connect_tethering_operation.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chromeos/components/tether/ble_constants.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/components/tether/proto_test_util.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace chromeos {

namespace tether {

namespace {

const char kTestSsid[] = "testSsid";
const char kTestPassword[] = "testPassword";

constexpr base::TimeDelta kConnectTetheringResponseTime =
    base::TimeDelta::FromSeconds(15);

class TestObserver : public ConnectTetheringOperation::Observer {
 public:
  TestObserver() : has_received_failure(false) {}

  void OnSuccessfulConnectTetheringResponse(
      const cryptauth::RemoteDevice& remote_device,
      const std::string& ssid,
      const std::string& password) override {
    this->remote_device = remote_device;
    this->ssid = ssid;
    this->password = password;
  }

  void OnConnectTetheringFailure(
      const cryptauth::RemoteDevice& remote_device,
      ConnectTetheringResponse_ResponseCode error_code) override {
    has_received_failure = true;
    this->remote_device = remote_device;
    this->error_code = error_code;
  }

  cryptauth::RemoteDevice remote_device;
  std::string ssid;
  std::string password;

  bool has_received_failure;
  ConnectTetheringResponse_ResponseCode error_code;
};

std::string CreateConnectTetheringRequestString() {
  ConnectTetheringRequest request;
  return MessageWrapper(request).ToRawMessage();
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

  response.mutable_device_status()->CopyFrom(
      CreateDeviceStatusWithFakeFields());

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
    mock_tether_host_response_recorder_ =
        base::MakeUnique<StrictMock<MockTetherHostResponseRecorder>>();
    test_observer_ = base::WrapUnique(new TestObserver());

    operation_ = base::WrapUnique(new ConnectTetheringOperation(
        test_device_, fake_ble_connection_manager_.get(),
        mock_tether_host_response_recorder_.get(), false /* setup_required */));
    operation_->AddObserver(test_observer_.get());

    test_clock_ = new base::SimpleTestClock();
    test_clock_->SetNow(base::Time::UnixEpoch());
    operation_->SetClockForTest(base::WrapUnique(test_clock_));

    operation_->Initialize();
  }

  void SimulateDeviceAuthenticationAndVerifyMessageSent() {
    VerifyResponseTimeoutSeconds(false /* setup_required */);

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
    test_clock_->Advance(kConnectTetheringResponseTime);

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
      EXPECT_EQ(test_device_, test_observer_->remote_device);
      EXPECT_EQ(std::string(kTestSsid), test_observer_->ssid);
      EXPECT_EQ(std::string(kTestPassword), test_observer_->password);
    } else {
      EXPECT_TRUE(test_observer_->has_received_failure);
      EXPECT_EQ(expected_response_code, test_observer_->error_code);
    }

    histogram_tester_.ExpectTimeBucketCount(
        "InstantTethering.Performance.ConnectTetheringResponseDuration",
        kConnectTetheringResponseTime, 1);
  }

  void VerifyResponseTimeoutSeconds(bool setup_required) {
    uint32_t expected_response_timeout_seconds =
        setup_required
            ? ConnectTetheringOperation::kSetupRequiredResponseTimeoutSeconds
            : MessageTransferOperation::kDefaultTimeoutSeconds;

    EXPECT_EQ(expected_response_timeout_seconds,
              operation_->GetTimeoutSeconds());
  }

  const std::string connect_tethering_request_string_;
  const cryptauth::RemoteDevice test_device_;

  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<StrictMock<MockTetherHostResponseRecorder>>
      mock_tether_host_response_recorder_;
  std::unique_ptr<TestObserver> test_observer_;
  base::SimpleTestClock* test_clock_;
  std::unique_ptr<ConnectTetheringOperation> operation_;

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectTetheringOperationTest);
};

TEST_F(ConnectTetheringOperationTest, TestOperation_SuccessButInvalidResponse) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS,
      true /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_SuccessWithValidResponse) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(test_device_));

  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_SUCCESS,
      false /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_UnknownError) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR,
      false /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_ProvisioningFailed) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

  SimulateDeviceAuthenticationAndVerifyMessageSent();
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_PROVISIONING_FAILED,
      false /* use_proto_without_ssid_and_password */);
}

TEST_F(ConnectTetheringOperationTest, TestCannotConnect) {
  EXPECT_CALL(*mock_tether_host_response_recorder_,
              RecordSuccessfulConnectTetheringResponse(_))
      .Times(0);

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

  histogram_tester_.ExpectTotalCount(
      "InstantTethering.Performance.ConnectTetheringResponseDuration", 0);
}

TEST_F(ConnectTetheringOperationTest, TestOperation_SetupRequired) {
  operation_ = base::WrapUnique(new ConnectTetheringOperation(
      test_device_, fake_ble_connection_manager_.get(),
      mock_tether_host_response_recorder_.get(), true /* setup_required */));
  VerifyResponseTimeoutSeconds(true /* setup_required */);
}

}  // namespace tether

}  // namespace cryptauth
