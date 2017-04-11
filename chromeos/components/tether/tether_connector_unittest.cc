// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_connector.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/components/tether/connect_tethering_operation.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/components/tether/fake_wifi_hotspot_connector.h"
#include "chromeos/components/tether/mock_host_scan_device_prioritizer.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace tether {

namespace {

const char kSsid[] = "ssid";
const char kPassword[] = "password";

const char kWifiNetworkGuid[] = "wifiNetworkGuid";

const char kTetherNetwork1Name[] = "tetherNetwork1Name";
const char kTetherNetwork2Name[] = "tetherNetwork2Name";

std::string CreateWifiConfigurationJsonString() {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << kWifiNetworkGuid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateIdle << "\""
     << "}";
  return ss.str();
}

class TestNetworkConnect : public NetworkConnect {
 public:
  TestNetworkConnect() : tether_delegate_(nullptr) {}
  ~TestNetworkConnect() override {}

  void CallTetherDelegate(const std::string& tether_network_guid) {
    tether_delegate_->ConnectToNetwork(tether_network_guid);
  }

  // NetworkConnect:
  void SetTetherDelegate(
      NetworkConnect::TetherDelegate* tether_delegate) override {
    tether_delegate_ = tether_delegate;
  }

  void DisconnectFromNetworkId(const std::string& network_id) override {}
  bool MaybeShowConfigureUI(const std::string& network_id,
                            const std::string& connect_error) override {
    return false;
  }
  void SetTechnologyEnabled(const chromeos::NetworkTypePattern& technology,
                            bool enabled_state) override {}
  void ShowMobileSetup(const std::string& network_id) override {}
  void ConfigureNetworkIdAndConnect(
      const std::string& network_id,
      const base::DictionaryValue& shill_properties,
      bool shared) override {}
  void CreateConfigurationAndConnect(base::DictionaryValue* shill_properties,
                                     bool shared) override {}
  void CreateConfiguration(base::DictionaryValue* shill_properties,
                           bool shared) override {}
  void ConnectToNetworkId(const std::string& network_id) override {}

 private:
  NetworkConnect::TetherDelegate* tether_delegate_;
};

class FakeConnectTetheringOperation : public ConnectTetheringOperation {
 public:
  FakeConnectTetheringOperation(
      const cryptauth::RemoteDevice& device_to_connect,
      BleConnectionManager* connection_manager,
      HostScanDevicePrioritizer* host_scan_device_prioritizer)
      : ConnectTetheringOperation(device_to_connect,
                                  connection_manager,
                                  host_scan_device_prioritizer) {}

  ~FakeConnectTetheringOperation() override {}

  void SendSuccessfulResponse(const std::string& ssid,
                              const std::string& password) {
    NotifyObserversOfSuccessfulResponse(ssid, password);
  }

  void SendFailedResponse(ConnectTetheringResponse_ResponseCode error_code) {
    NotifyObserversOfConnectionFailure(error_code);
  }

  cryptauth::RemoteDevice GetRemoteDevice() {
    EXPECT_EQ(1u, remote_devices().size());
    return remote_devices()[0];
  }
};

class FakeConnectTetheringOperationFactory
    : public ConnectTetheringOperation::Factory {
 public:
  FakeConnectTetheringOperationFactory() {}
  virtual ~FakeConnectTetheringOperationFactory() {}

  std::vector<FakeConnectTetheringOperation*>& created_operations() {
    return created_operations_;
  }

 protected:
  // ConnectTetheringOperation::Factory:
  std::unique_ptr<ConnectTetheringOperation> BuildInstance(
      const cryptauth::RemoteDevice& device_to_connect,
      BleConnectionManager* connection_manager,
      HostScanDevicePrioritizer* host_scan_device_prioritizer) override {
    FakeConnectTetheringOperation* operation =
        new FakeConnectTetheringOperation(device_to_connect, connection_manager,
                                          host_scan_device_prioritizer);
    created_operations_.push_back(operation);
    return base::WrapUnique(operation);
  }

 private:
  std::vector<FakeConnectTetheringOperation*> created_operations_;
};

}  // namespace

class TetherConnectorTest : public NetworkStateTest {
 public:
  TetherConnectorTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(2u)) {}
  ~TetherConnectorTest() override {}

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    fake_operation_factory_ =
        base::WrapUnique(new FakeConnectTetheringOperationFactory());
    ConnectTetheringOperation::Factory::SetInstanceForTesting(
        fake_operation_factory_.get());

    test_network_connect_ = base::WrapUnique(new TestNetworkConnect());
    fake_wifi_hotspot_connector_ =
        base::MakeUnique<FakeWifiHotspotConnector>(network_state_handler());
    fake_active_host_ = base::MakeUnique<FakeActiveHost>();
    fake_tether_host_fetcher_ = base::MakeUnique<FakeTetherHostFetcher>(
        test_devices_, false /* synchronously_reply_with_results */);
    fake_ble_connection_manager_ = base::MakeUnique<FakeBleConnectionManager>();
    mock_host_scan_device_prioritizer_ =
        base::MakeUnique<MockHostScanDevicePrioritizer>();
    device_id_tether_network_guid_map_ =
        base::MakeUnique<DeviceIdTetherNetworkGuidMap>();

    tether_connector_ = base::WrapUnique(new TetherConnector(
        test_network_connect_.get(), network_state_handler(),
        fake_wifi_hotspot_connector_.get(), fake_active_host_.get(),
        fake_tether_host_fetcher_.get(), fake_ble_connection_manager_.get(),
        mock_host_scan_device_prioritizer_.get(),
        device_id_tether_network_guid_map_.get()));

    SetUpTetherNetworks();
  }

  void TearDown() override {
    // Must delete |fake_wifi_hotspot_connector_| before NetworkStateHandler is
    // destroyed to ensure that NetworkStateHandler has zero observers by the
    // time it reaches its destructor.
    fake_wifi_hotspot_connector_.reset();

    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  std::string GetTetherNetworkGuid(const std::string& device_id) {
    return device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
        device_id);
  }

  void SetUpTetherNetworks() {
    // Add a tether network corresponding to both of the test devices. These
    // networks are expected to be added already before TetherConnector receives
    // its ConnectToNetwork() callback.
    network_state_handler()->AddTetherNetworkState(
        GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
        kTetherNetwork1Name);
    network_state_handler()->AddTetherNetworkState(
        GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
        kTetherNetwork2Name);
  }

  void SuccessfullyJoinWifiNetwork() {
    ConfigureService(CreateWifiConfigurationJsonString());
    fake_wifi_hotspot_connector_->CallMostRecentCallback(kWifiNetworkGuid);
  }

  void VerifyTetherAndWifiNetworkAssociation(
      const std::string& tether_network_guid) {
    const NetworkState* tether_network_state =
        network_state_handler()->GetNetworkStateFromGuid(tether_network_guid);
    EXPECT_TRUE(tether_network_state);
    EXPECT_EQ(kWifiNetworkGuid, tether_network_state->tether_guid());

    const NetworkState* wifi_network_state =
        network_state_handler()->GetNetworkStateFromGuid(kWifiNetworkGuid);
    EXPECT_TRUE(wifi_network_state);
    EXPECT_EQ(tether_network_guid, wifi_network_state->tether_guid());
  }

  const std::vector<cryptauth::RemoteDevice> test_devices_;
  const base::MessageLoop message_loop_;

  std::unique_ptr<FakeConnectTetheringOperationFactory> fake_operation_factory_;
  std::unique_ptr<TestNetworkConnect> test_network_connect_;
  std::unique_ptr<FakeWifiHotspotConnector> fake_wifi_hotspot_connector_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<MockHostScanDevicePrioritizer>
      mock_host_scan_device_prioritizer_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;

  std::unique_ptr<TetherConnector> tether_connector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherConnectorTest);
};

TEST_F(TetherConnectorTest, TestCannotFetchDevice) {
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid("nonexistentDeviceId"));
  fake_tether_host_fetcher_->InvokePendingCallbacks();

  // Since an invalid device ID was used, no connection should have been
  // started.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherConnectorTest, TestConnectTetheringOperationFails) {
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  fake_tether_host_fetcher_->InvokePendingCallbacks();

  // The connection should have started.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // Simulate a failed connection attempt (either the host cannot provide
  // tethering at this time or a timeout occurs).
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
  fake_operation_factory_->created_operations()[0]->SendFailedResponse(
      ConnectTetheringResponse_ResponseCode::
          ConnectTetheringResponse_ResponseCode_UNKNOWN_ERROR);

  // The failure should have resulted in the host being disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherConnectorTest, TestConnectingToWifiFails) {
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  fake_tether_host_fetcher_->InvokePendingCallbacks();

  // The connection should have started.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // Receive a successful response. We should still be connecting.
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
  fake_operation_factory_->created_operations()[0]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());

  // |fake_wifi_hotspot_connector_| should have received the SSID and password
  // above. Verify this, then return an empty string, signaling a failure to
  // connect.
  EXPECT_EQ(kSsid, fake_wifi_hotspot_connector_->most_recent_ssid());
  EXPECT_EQ(kPassword, fake_wifi_hotspot_connector_->most_recent_password());
  fake_wifi_hotspot_connector_->CallMostRecentCallback("");

  // The failure should have resulted in the host being disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherConnectorTest, TestSuccessfulConnection) {
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  fake_tether_host_fetcher_->InvokePendingCallbacks();

  // The connection should have started.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // Receive a successful response. We should still be connecting.
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
  fake_operation_factory_->created_operations()[0]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());

  // |fake_wifi_hotspot_connector_| should have received the SSID and password
  // above. Verify this, then return the GUID corresponding to the connected
  // Wi-Fi network.
  EXPECT_EQ(kSsid, fake_wifi_hotspot_connector_->most_recent_ssid());
  EXPECT_EQ(kPassword, fake_wifi_hotspot_connector_->most_recent_password());
  SuccessfullyJoinWifiNetwork();

  // The active host should now be connected, and the tether and Wi-Fi networks
  // should be associated.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTED,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_EQ(kWifiNetworkGuid, fake_active_host_->GetWifiNetworkGuid());
  VerifyTetherAndWifiNetworkAssociation(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
}

TEST_F(TetherConnectorTest, TestNewConnectionAttemptDuringFetch_SameDevice) {
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));

  // Instead of invoking the pending callbacks on |fake_tether_host_fetcher_|,
  // attempt another connection attempt.
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));

  // Now invoke the callbacks. Only one operation should have been created,
  // even though the callback occurred twice.
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
}

TEST_F(TetherConnectorTest,
       TestNewConnectionAttemptDuringFetch_DifferentDevice) {
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));

  // Instead of invoking the pending callbacks on |fake_tether_host_fetcher_|,
  // attempt another connection attempt, this time to another device.
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[1].GetDeviceId()));

  // Now invoke the callbacks. An operation should have been created for the
  // device 1, not device 0.
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(
      test_devices_[1],
      fake_operation_factory_->created_operations()[0]->GetRemoteDevice());
}

TEST_F(TetherConnectorTest,
       TestNewConnectionAttemptDuringOperation_DifferentDevice) {
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  fake_tether_host_fetcher_->InvokePendingCallbacks();

  // The active host should be device 0.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // An operation should have been created.
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());

  // Before the created operation replies, start a new connection to device 1.
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[1].GetDeviceId()));
  fake_tether_host_fetcher_->InvokePendingCallbacks();

  // Now, the active host should be the second device.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[1].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());

  // A second operation should have been created.
  EXPECT_EQ(2u, fake_operation_factory_->created_operations().size());

  // No connection should have been started.
  EXPECT_TRUE(fake_wifi_hotspot_connector_->most_recent_ssid().empty());
  EXPECT_TRUE(fake_wifi_hotspot_connector_->most_recent_password().empty());

  // The second operation replies successfully, and this response should
  // result in a Wi-Fi connection attempt.
  fake_operation_factory_->created_operations()[1]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_EQ(kSsid, fake_wifi_hotspot_connector_->most_recent_ssid());
  EXPECT_EQ(kPassword, fake_wifi_hotspot_connector_->most_recent_password());
}

TEST_F(TetherConnectorTest,
       TestNewConnectionAttemptDuringWifiConnection_DifferentDevice) {
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  fake_tether_host_fetcher_->InvokePendingCallbacks();
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[0].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(1u, fake_operation_factory_->created_operations().size());

  fake_operation_factory_->created_operations()[0]->SendSuccessfulResponse(
      kSsid, kPassword);
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(kSsid, fake_wifi_hotspot_connector_->most_recent_ssid());
  EXPECT_EQ(kPassword, fake_wifi_hotspot_connector_->most_recent_password());

  // While the connection to the Wi-Fi network is in progress, start a new
  // connection attempt.
  test_network_connect_->CallTetherDelegate(
      GetTetherNetworkGuid(test_devices_[1].GetDeviceId()));
  fake_tether_host_fetcher_->InvokePendingCallbacks();

  // Connect successfully to the first Wi-Fi network. Even though a temporary
  // connection has succeeded, the active host should be CONNECTING to device 1.
  SuccessfullyJoinWifiNetwork();
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTING,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[1].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
            fake_active_host_->GetTetherNetworkGuid());
  EXPECT_TRUE(fake_active_host_->GetWifiNetworkGuid().empty());
}

}  // namespace tether

}  // namespace cryptauth
