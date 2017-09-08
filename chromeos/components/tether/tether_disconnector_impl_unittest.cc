// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_disconnector_impl.h"

#include "base/memory/ptr_util.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/fake_tether_connector.h"
#include "chromeos/components/tether/fake_wifi_hotspot_disconnector.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

const char kSuccessResult[] = "success";
const char kWifiNetworkGuid[] = "wifiNetworkGuid";

}  // namespace

class TetherDisconnectorImplTest : public testing::Test {
 public:
  TetherDisconnectorImplTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(2u)) {}
  ~TetherDisconnectorImplTest() override {}

  void SetUp() override {
    fake_active_host_ = base::MakeUnique<FakeActiveHost>();
    fake_wifi_hotspot_disconnector_ =
        base::MakeUnique<FakeWifiHotspotDisconnector>();
    fake_disconnect_tethering_request_sender_ =
        base::MakeUnique<FakeDisconnectTetheringRequestSender>();
    fake_tether_connector_ = base::MakeUnique<FakeTetherConnector>();
    device_id_tether_network_guid_map_ =
        base::MakeUnique<DeviceIdTetherNetworkGuidMap>();

    tether_disconnector_ = base::MakeUnique<TetherDisconnectorImpl>(
        fake_active_host_.get(), fake_wifi_hotspot_disconnector_.get(),
        fake_disconnect_tethering_request_sender_.get(),
        fake_tether_connector_.get(), device_id_tether_network_guid_map_.get());
  }

  std::string GetTetherNetworkGuid(const std::string& device_id) {
    return device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
        device_id);
  }

  void SuccessCallback() { disconnection_result_ = kSuccessResult; }

  void ErrorCallback(const std::string& error_name) {
    disconnection_result_ = error_name;
  }

  void CallDisconnect(const std::string& tether_network_guid) {
    tether_disconnector_->DisconnectFromNetwork(
        tether_network_guid,
        base::Bind(&TetherDisconnectorImplTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&TetherDisconnectorImplTest::ErrorCallback,
                   base::Unretained(this)));
  }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(disconnection_result_);
    return result;
  }

  // Verifies that no Wi-Fi disconnection was requested and that no
  // DisconnectTetheringRequest message was sent.
  void VerifyNoDisconnectionOccurred() {
    EXPECT_TRUE(
        fake_wifi_hotspot_disconnector_->last_disconnected_wifi_network_guid()
            .empty());
    EXPECT_TRUE(
        fake_disconnect_tethering_request_sender_->device_ids_sent_requests()
            .empty());
  }

  const std::vector<cryptauth::RemoteDevice> test_devices_;

  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeWifiHotspotDisconnector> fake_wifi_hotspot_disconnector_;
  std::unique_ptr<FakeDisconnectTetheringRequestSender>
      fake_disconnect_tethering_request_sender_;
  std::unique_ptr<FakeTetherConnector> fake_tether_connector_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;

  std::string disconnection_result_;

  std::unique_ptr<TetherDisconnectorImpl> tether_disconnector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherDisconnectorImplTest);
};

TEST_F(TetherDisconnectorImplTest, DisconnectWhenAlreadyDisconnected) {
  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());

  VerifyNoDisconnectionOccurred();

  // Should still be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenOtherDeviceConnected) {
  // Set device 1 as connected.
  fake_active_host_->SetActiveHostConnected(
      test_devices_[1].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
      "someWifiNetworkGuid");

  // Try to disconnect device 0; this should fail since the device is not
  // connected.
  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());

  VerifyNoDisconnectionOccurred();

  // Should still be connected to the other host.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTED,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[1].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenConnecting_CancelFails) {
  fake_active_host_->SetActiveHostConnecting(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  fake_tether_connector_->set_should_cancel_successfully(false);

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorDisconnectFailed,
            GetResultAndReset());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_tether_connector_->last_canceled_tether_network_guid());

  VerifyNoDisconnectionOccurred();

  // Note: This test does not check the active host's status because it will be
  // changed by TetherConnector.
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenConnecting_CancelSucceeds) {
  fake_active_host_->SetActiveHostConnecting(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  fake_tether_connector_->set_should_cancel_successfully(true);

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_tether_connector_->last_canceled_tether_network_guid());

  VerifyNoDisconnectionOccurred();

  // Note: This test does not check the active host's status because it will be
  // changed by TetherConnector.
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenConnected_Failure) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  fake_wifi_hotspot_disconnector_->set_disconnection_error_name("failureName");

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ("failureName", GetResultAndReset());

  EXPECT_EQ(
      kWifiNetworkGuid,
      fake_wifi_hotspot_disconnector_->last_disconnected_wifi_network_guid());
  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenConnected_Success) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  EXPECT_EQ(
      kWifiNetworkGuid,
      fake_wifi_hotspot_disconnector_->last_disconnected_wifi_network_guid());
  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

}  // namespace tether

}  // namespace chromeos
