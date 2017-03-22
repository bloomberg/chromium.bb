// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scanner_operation.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "chromeos/components/tether/ble_constants.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/message_wrapper.h"
#include "chromeos/components/tether/mock_host_scan_device_prioritizer.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace chromeos {

namespace tether {

namespace {

const char kDefaultCarrier[] = "Google Fi";

class TestHostScanDevicePrioritizer : public MockHostScanDevicePrioritizer {
 public:
  TestHostScanDevicePrioritizer() : MockHostScanDevicePrioritizer() {}
  ~TestHostScanDevicePrioritizer() override {}

  // Simply reverses the device order.
  void SortByHostScanOrder(
      std::vector<cryptauth::RemoteDevice>* remote_devices) const override {
    for (size_t i = 0; i < remote_devices->size() / 2; ++i) {
      std::iter_swap(remote_devices->begin() + i,
                     remote_devices->end() - i - 1);
    }
  }

  void VerifyHasBeenPrioritized(
      const std::vector<cryptauth::RemoteDevice>& original,
      const std::vector<cryptauth::RemoteDevice>& prioritized) {
    std::vector<cryptauth::RemoteDevice> copy_of_original = original;
    SortByHostScanOrder(&copy_of_original);
    EXPECT_EQ(copy_of_original, prioritized);
  }
};

class TestObserver : public HostScannerOperation::Observer {
 public:
  TestObserver()
      : has_received_update(false), has_final_scan_result_been_sent(false) {}

  void OnTetherAvailabilityResponse(
      std::vector<HostScannerOperation::ScannedDeviceInfo>&
          scanned_device_list_so_far,
      bool is_final_scan_result) override {
    has_received_update = true;
    scanned_devices_so_far = scanned_device_list_so_far;
    has_final_scan_result_been_sent = is_final_scan_result;
  }

  bool has_received_update;
  std::vector<HostScannerOperation::ScannedDeviceInfo> scanned_devices_so_far;
  bool has_final_scan_result_been_sent;
};

std::string CreateTetherAvailabilityRequestString() {
  TetherAvailabilityRequest request;
  return MessageWrapper(request).ToRawMessage();
}

DeviceStatus CreateFakeDeviceStatus(std::string cell_provider_name) {
  WifiStatus wifi_status;
  wifi_status.set_status_code(
      WifiStatus_StatusCode::WifiStatus_StatusCode_CONNECTED);
  wifi_status.set_ssid("Google A");

  DeviceStatus device_status;
  device_status.set_battery_percentage(75);
  device_status.set_cell_provider(cell_provider_name);
  device_status.set_connection_strength(4);
  device_status.mutable_wifi_status()->CopyFrom(wifi_status);

  return device_status;
}

std::string CreateTetherAvailabilityResponseString(
    TetherAvailabilityResponse_ResponseCode response_code,
    const std::string& cell_provider_name) {
  TetherAvailabilityResponse response;
  response.set_response_code(response_code);
  response.mutable_device_status()->CopyFrom(
      CreateFakeDeviceStatus(cell_provider_name));
  return MessageWrapper(response).ToRawMessage();
}

}  // namespace

class HostScannerOperationTest : public testing::Test {
 protected:
  HostScannerOperationTest()
      : tether_availability_request_string_(
            CreateTetherAvailabilityRequestString()),
        test_devices_(cryptauth::GenerateTestRemoteDevices(5)) {
    // These tests are written under the assumption that there are a maximum of
    // 3 connection attempts; they need to be edited if this value changes.
    EXPECT_EQ(3u, MessageTransferOperation::kMaxConnectionAttemptsPerDevice);
  }

  void SetUp() override {
    fake_ble_connection_manager_ = base::MakeUnique<FakeBleConnectionManager>();
    test_host_scan_device_prioritizer_ =
        base::MakeUnique<StrictMock<TestHostScanDevicePrioritizer>>();
    test_observer_ = base::WrapUnique(new TestObserver());
  }

  void ConstructOperation(
      const std::vector<cryptauth::RemoteDevice>& remote_devices) {
    operation_ = base::WrapUnique(new HostScannerOperation(
        remote_devices, fake_ble_connection_manager_.get(),
        test_host_scan_device_prioritizer_.get()));
    operation_->AddObserver(test_observer_.get());

    // Verify that the devices have been correctly prioritized.
    test_host_scan_device_prioritizer_->VerifyHasBeenPrioritized(
        remote_devices, operation_->remote_devices());

    EXPECT_FALSE(test_observer_->has_received_update);
    operation_->Initialize();
    EXPECT_TRUE(test_observer_->has_received_update);
    EXPECT_TRUE(test_observer_->scanned_devices_so_far.empty());
    EXPECT_FALSE(test_observer_->has_final_scan_result_been_sent);
  }

  void SimulateDeviceAuthenticationAndVerifyMessageSent(
      const cryptauth::RemoteDevice& remote_device,
      size_t expected_num_messages_sent) {
    // Verify that before the authentication, one fewer than the expected number
    // of messages has been sent.
    EXPECT_EQ(expected_num_messages_sent - 1,
              fake_ble_connection_manager_->sent_messages().size());

    operation_->OnDeviceAuthenticated(remote_device);

    // Now, verify that the message was sent successfully.
    std::vector<FakeBleConnectionManager::SentMessage>& sent_messages =
        fake_ble_connection_manager_->sent_messages();
    ASSERT_EQ(expected_num_messages_sent, sent_messages.size());
    EXPECT_EQ(remote_device,
              sent_messages[expected_num_messages_sent - 1].remote_device);
    EXPECT_EQ(tether_availability_request_string_,
              sent_messages[expected_num_messages_sent - 1].message);
  }

  void SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      const cryptauth::RemoteDevice& remote_device,
      TetherAvailabilityResponse_ResponseCode response_code,
      const std::string& cell_provider_name,
      bool expected_to_be_last_scan_result) {
    size_t num_scanned_device_results_so_far =
        test_observer_->scanned_devices_so_far.size();

    fake_ble_connection_manager_->ReceiveMessage(
        remote_device, CreateTetherAvailabilityResponseString(
                           response_code, cell_provider_name));

    bool tether_available =
        response_code ==
        TetherAvailabilityResponse_ResponseCode::
            TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE;
    bool set_up_required =
        response_code ==
        TetherAvailabilityResponse_ResponseCode::
            TetherAvailabilityResponse_ResponseCode_SETUP_NEEDED;
    if (tether_available || set_up_required) {
      // If tether is available or set up is needed, the observer callback
      // should be invoked with an updated list.
      EXPECT_EQ(num_scanned_device_results_so_far + 1,
                test_observer_->scanned_devices_so_far.size());

      HostScannerOperation::ScannedDeviceInfo last_received_info =
          test_observer_->scanned_devices_so_far
              [test_observer_->scanned_devices_so_far.size() - 1];
      EXPECT_EQ(cell_provider_name,
                last_received_info.device_status.cell_provider());
      EXPECT_EQ(set_up_required, last_received_info.set_up_required);
    }

    EXPECT_EQ(expected_to_be_last_scan_result,
              test_observer_->has_final_scan_result_been_sent);
  }

  void TestOperationWithOneDevice(
      TetherAvailabilityResponse_ResponseCode response_code) {
    ConstructOperation(std::vector<cryptauth::RemoteDevice>{test_devices_[0]});
    SimulateDeviceAuthenticationAndVerifyMessageSent(test_devices_[0], 1u);
    SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
        test_devices_[0], response_code, std::string(kDefaultCarrier), true);
  }

  const std::string tether_availability_request_string_;
  const std::vector<cryptauth::RemoteDevice> test_devices_;

  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<StrictMock<TestHostScanDevicePrioritizer>>
      test_host_scan_device_prioritizer_;
  std::unique_ptr<TestObserver> test_observer_;
  std::unique_ptr<HostScannerOperation> operation_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostScannerOperationTest);
};

TEST_F(HostScannerOperationTest, TestDevicesArePrioritizedDuringConstruction) {
  // Verification of device order prioritization occurs in ConstructOperation().
  ConstructOperation(test_devices_);
}

TEST_F(HostScannerOperationTest, TestOperation_OneDevice_UnknownError) {
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(_))
      .Times(0);

  TestOperationWithOneDevice(
      TetherAvailabilityResponse_ResponseCode ::
          TetherAvailabilityResponse_ResponseCode_UNKNOWN_ERROR);
}

TEST_F(HostScannerOperationTest, TestOperation_OneDevice_TetherAvailable) {
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]));

  TestOperationWithOneDevice(
      TetherAvailabilityResponse_ResponseCode ::
          TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE);
}

TEST_F(HostScannerOperationTest, TestOperation_OneDevice_SetupNeeded) {
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]));

  TestOperationWithOneDevice(
      TetherAvailabilityResponse_ResponseCode ::
          TetherAvailabilityResponse_ResponseCode_SETUP_NEEDED);
}

TEST_F(HostScannerOperationTest, TestOperation_OneDevice_NoReception) {
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(_))
      .Times(0);

  TestOperationWithOneDevice(
      TetherAvailabilityResponse_ResponseCode ::
          TetherAvailabilityResponse_ResponseCode_NO_RECEPTION);
}

TEST_F(HostScannerOperationTest, TestOperation_OneDevice_NoSimCard) {
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(_))
      .Times(0);

  TestOperationWithOneDevice(
      TetherAvailabilityResponse_ResponseCode ::
          TetherAvailabilityResponse_ResponseCode_NO_SIM_CARD);
}

TEST_F(HostScannerOperationTest,
       TestOperation_OneDevice_NotificationsDisabled) {
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(_))
      .Times(0);

  TestOperationWithOneDevice(
      TetherAvailabilityResponse_ResponseCode ::
          TetherAvailabilityResponse_ResponseCode_NOTIFICATIONS_DISABLED);
}

TEST_F(HostScannerOperationTest, TestMultipleDevices) {
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]));
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]))
      .Times(0);
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]));
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]))
      .Times(0);
  EXPECT_CALL(*test_host_scan_device_prioritizer_,
              RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]))
      .Times(0);

  ConstructOperation(test_devices_);

  // Simulate devices 0 and 2 authenticating successfully. Use different carrier
  // names to ensure that the DeviceStatus is being stored correctly.
  SimulateDeviceAuthenticationAndVerifyMessageSent(test_devices_[0], 1u);
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      test_devices_[0],
      TetherAvailabilityResponse_ResponseCode ::
          TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE,
      "firstCarrierName", false /* expected_to_be_last_scan_result */);

  SimulateDeviceAuthenticationAndVerifyMessageSent(test_devices_[2], 2u);
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      test_devices_[2],
      TetherAvailabilityResponse_ResponseCode ::
          TetherAvailabilityResponse_ResponseCode_TETHER_AVAILABLE,
      "secondCarrierName", false /* expected_to_be_last_scan_result */);

  // Simulate device 1 failing to connect.
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

  // The scan should still not be over, and no new scan results should have
  // come in.
  EXPECT_FALSE(test_observer_->has_final_scan_result_been_sent);
  EXPECT_EQ(2u, test_observer_->scanned_devices_so_far.size());

  // Simulate device 3 failing to connect.
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3], cryptauth::SecureChannel::Status::DISCONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3], cryptauth::SecureChannel::Status::DISCONNECTED);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3], cryptauth::SecureChannel::Status::CONNECTING);
  fake_ble_connection_manager_->SetDeviceStatus(
      test_devices_[3], cryptauth::SecureChannel::Status::DISCONNECTED);

  // The scan should still not be over, and no new scan results should have
  // come in.
  EXPECT_FALSE(test_observer_->has_final_scan_result_been_sent);
  EXPECT_EQ(2u, test_observer_->scanned_devices_so_far.size());

  // Simulate device 4 connecting successfully but responding with a code
  // indicating that reception is not available.
  SimulateDeviceAuthenticationAndVerifyMessageSent(test_devices_[4], 3u);
  SimulateResponseReceivedAndVerifyObserverCallbackInvoked(
      test_devices_[4],
      TetherAvailabilityResponse_ResponseCode ::
          TetherAvailabilityResponse_ResponseCode_NO_RECEPTION,
      "noService", true /* expected_to_be_last_scan_result */);

  // The scan should be over, and still no new scan results should have come in.
  EXPECT_TRUE(test_observer_->has_final_scan_result_been_sent);
  EXPECT_EQ(2u, test_observer_->scanned_devices_so_far.size());
}

}  // namespace tether

}  // namespace cryptauth
