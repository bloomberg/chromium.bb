// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_disconnector.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/components/tether/connect_tethering_operation.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "chromeos/components/tether/fake_network_configuration_remover.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/components/tether/fake_wifi_hotspot_connector.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_connection_handler.h"
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

const char kSuccessResult[] = "success";

const char kWifiNetworkGuid[] = "wifiNetworkGuid";

std::string CreateConnectedWifiConfigurationJsonString() {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << kWifiNetworkGuid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateOnline << "\""
     << "}";
  return ss.str();
}

class TestNetworkConnectionHandler : public NetworkConnectionHandler {
 public:
  TestNetworkConnectionHandler(base::Closure disconnect_callback)
      : disconnect_callback_(disconnect_callback) {}
  ~TestNetworkConnectionHandler() override {}

  std::string last_disconnect_service_path() {
    return last_disconnect_service_path_;
  }

  base::Closure last_disconnect_success_callback() {
    return last_disconnect_success_callback_;
  }

  network_handler::ErrorCallback last_disconnect_error_callback() {
    return last_disconnect_error_callback_;
  }

  // NetworkConnectionHandler:
  void DisconnectNetwork(
      const std::string& service_path,
      const base::Closure& success_callback,
      const network_handler::ErrorCallback& error_callback) override {
    last_disconnect_service_path_ = service_path;
    last_disconnect_success_callback_ = success_callback;
    last_disconnect_error_callback_ = error_callback;

    disconnect_callback_.Run();
  }
  void ConnectToNetwork(const std::string& service_path,
                        const base::Closure& success_callback,
                        const network_handler::ErrorCallback& error_callback,
                        bool check_error_state) override {}
  bool HasConnectingNetwork(const std::string& service_path) override {
    return false;
  }
  bool HasPendingConnectRequest() override { return false; }
  void Init(NetworkStateHandler* network_state_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler) override {}

 private:
  base::Closure disconnect_callback_;

  std::string last_disconnect_service_path_;
  base::Closure last_disconnect_success_callback_;
  network_handler::ErrorCallback last_disconnect_error_callback_;
};

class TestTetherConnector : public TetherConnector {
 public:
  TestTetherConnector()
      : TetherConnector(nullptr /* network_state_handler */,
                        nullptr /* wifi_hotspot_connector */,
                        nullptr /* active_host */,
                        nullptr /* tether_host_fetcher */,
                        nullptr /* connection_manager */,
                        nullptr /* tether_host_response_recorder */,
                        nullptr /* device_id_tether_network_guid_map */),
        should_cancel_successfully_(true) {}
  ~TestTetherConnector() override {}

  void set_should_cancel_successfully(bool should_cancel_successfully) {
    should_cancel_successfully_ = should_cancel_successfully;
  }

  std::string last_canceled_tether_network_guid() {
    return last_canceled_tether_network_guid_;
  }

  // TetherConnector:
  bool CancelConnectionAttempt(
      const std::string& tether_network_guid) override {
    last_canceled_tether_network_guid_ = tether_network_guid;
    return should_cancel_successfully_;
  }

 private:
  bool should_cancel_successfully_;
  std::string last_canceled_tether_network_guid_;
};

class FakeDisconnectTetheringOperation : public DisconnectTetheringOperation {
 public:
  FakeDisconnectTetheringOperation(
      const cryptauth::RemoteDevice& device_to_connect,
      BleConnectionManager* connection_manager)
      : DisconnectTetheringOperation(device_to_connect, connection_manager) {}

  ~FakeDisconnectTetheringOperation() override {}

  void NotifyFinished(bool success) {
    NotifyObserversOperationFinished(success);
  }

  cryptauth::RemoteDevice GetRemoteDevice() {
    EXPECT_EQ(1u, remote_devices().size());
    return remote_devices()[0];
  }
};

class FakeDisconnectTetheringOperationFactory
    : public DisconnectTetheringOperation::Factory {
 public:
  FakeDisconnectTetheringOperationFactory() {}
  virtual ~FakeDisconnectTetheringOperationFactory() {}

  std::vector<FakeDisconnectTetheringOperation*>& created_operations() {
    return created_operations_;
  }

 protected:
  // DisconnectTetheringOperation::Factory:
  std::unique_ptr<DisconnectTetheringOperation> BuildInstance(
      const cryptauth::RemoteDevice& device_to_connect,
      BleConnectionManager* connection_manager) override {
    FakeDisconnectTetheringOperation* operation =
        new FakeDisconnectTetheringOperation(device_to_connect,
                                             connection_manager);
    created_operations_.push_back(operation);
    return base::WrapUnique(operation);
  }

 private:
  std::vector<FakeDisconnectTetheringOperation*> created_operations_;
};

}  // namespace

class TetherDisconnectorTest : public NetworkStateTest {
 public:
  TetherDisconnectorTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(2u)) {}
  ~TetherDisconnectorTest() override {}

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    should_disconnect_successfully_ = true;

    test_network_connection_handler_ =
        base::WrapUnique(new TestNetworkConnectionHandler(base::Bind(
            &TetherDisconnectorTest::OnNetworkConnectionManagerDisconnect,
            base::Unretained(this))));
    fake_active_host_ = base::MakeUnique<FakeActiveHost>();
    fake_ble_connection_manager_ = base::MakeUnique<FakeBleConnectionManager>();
    fake_network_configuration_remover_ =
        base::MakeUnique<FakeNetworkConfigurationRemover>();
    test_tether_connector_ = base::WrapUnique(new TestTetherConnector());
    device_id_tether_network_guid_map_ =
        base::MakeUnique<DeviceIdTetherNetworkGuidMap>();
    fake_tether_host_fetcher_ = base::MakeUnique<FakeTetherHostFetcher>(
        test_devices_, true /* synchronously_reply_with_results */);

    fake_operation_factory_ =
        base::WrapUnique(new FakeDisconnectTetheringOperationFactory());
    DisconnectTetheringOperation::Factory::SetInstanceForTesting(
        fake_operation_factory_.get());

    SetUpTetherNetworks();

    tether_disconnector_ = base::MakeUnique<TetherDisconnector>(
        test_network_connection_handler_.get(), network_state_handler(),
        fake_active_host_.get(), fake_ble_connection_manager_.get(),
        fake_network_configuration_remover_.get(), test_tether_connector_.get(),
        device_id_tether_network_guid_map_.get(),
        fake_tether_host_fetcher_.get());
  }

  void TearDown() override {
    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  std::string GetTetherNetworkGuid(const std::string& device_id) {
    return device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
        device_id);
  }

  void SetUpTetherNetworks() {
    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TECHNOLOGY_ENABLED);

    // Add a tether network corresponding to both of the test devices. These
    // networks are expected to be added already before
    // TetherDisconnector::DisconnectFromNetwork() is called.
    network_state_handler()->AddTetherNetworkState(
        GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
        "TetherNetworkName1", "TetherNetworkCarrier1",
        85 /* battery_percentage */, 75 /* signal_strength */,
        true /* has_connected_to_host */);
    network_state_handler()->AddTetherNetworkState(
        GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
        "TetherNetworkName2", "TetherNetworkCarrier2",
        90 /* battery_percentage */, 50 /* signal_strength */,
        true /* has_connected_to_host */);
  }

  void SimulateConnectionToWifiNetwork() {
    wifi_service_path_ =
        ConfigureService(CreateConnectedWifiConfigurationJsonString());
    EXPECT_FALSE(wifi_service_path_.empty());
  }

  void SuccessCallback() { disconnection_result_ = kSuccessResult; }

  void ErrorCallback(const std::string& error_name) {
    disconnection_result_ = error_name;
  }

  void CallDisconnect(const std::string& tether_network_guid) {
    tether_disconnector_->DisconnectFromNetwork(
        tether_network_guid,
        base::Bind(&TetherDisconnectorTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&TetherDisconnectorTest::ErrorCallback,
                   base::Unretained(this)));
  }

  // This function is called by
  // TestNetworkConnectionHandler::DisconnectFromNetwork().
  void OnNetworkConnectionManagerDisconnect() {
    EXPECT_EQ(wifi_service_path_,
              test_network_connection_handler_->last_disconnect_service_path());

    if (should_disconnect_successfully_) {
      SetServiceProperty(wifi_service_path_, shill::kStateProperty,
                         base::Value(shill::kStateIdle));
      EXPECT_FALSE(
          test_network_connection_handler_->last_disconnect_success_callback()
              .is_null());
      test_network_connection_handler_->last_disconnect_success_callback()
          .Run();
    } else {
      EXPECT_FALSE(
          test_network_connection_handler_->last_disconnect_error_callback()
              .is_null());
      network_handler::RunErrorCallback(
          test_network_connection_handler_->last_disconnect_error_callback(),
          wifi_service_path_, NetworkConnectionHandler::kErrorDisconnectFailed,
          "" /* error_detail */);
    }
  }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(disconnection_result_);
    return result;
  }

  const std::vector<cryptauth::RemoteDevice> test_devices_;
  const base::MessageLoop message_loop_;

  std::unique_ptr<TestNetworkConnectionHandler>
      test_network_connection_handler_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  std::unique_ptr<FakeNetworkConfigurationRemover>
      fake_network_configuration_remover_;
  std::unique_ptr<TestTetherConnector> test_tether_connector_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;

  std::unique_ptr<FakeDisconnectTetheringOperationFactory>
      fake_operation_factory_;

  std::string wifi_service_path_;
  std::string disconnection_result_;
  bool should_disconnect_successfully_;

  std::unique_ptr<TetherDisconnector> tether_disconnector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherDisconnectorTest);
};

TEST_F(TetherDisconnectorTest, DisconnectWhenAlreadyDisconnected) {
  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());

  // Should still be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorTest, DisconnectWhenOtherDeviceConnected) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[1].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
      "otherWifiNetworkGuid");

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());

  // Should still be connected to the other host.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::CONNECTED,
            fake_active_host_->GetActiveHostStatus());
  EXPECT_EQ(test_devices_[1].GetDeviceId(),
            fake_active_host_->GetActiveHostDeviceId());
}

TEST_F(TetherDisconnectorTest, DisconnectWhenConnecting_CancelFails) {
  fake_active_host_->SetActiveHostConnecting(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  test_tether_connector_->set_should_cancel_successfully(false);

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorDisconnectFailed,
            GetResultAndReset());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            test_tether_connector_->last_canceled_tether_network_guid());

  // Note: This test does not check the active host's status because it will be
  // changed by TetherConnector.
}

TEST_F(TetherDisconnectorTest, DisconnectWhenConnecting_CancelSucceeds) {
  fake_active_host_->SetActiveHostConnecting(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  test_tether_connector_->set_should_cancel_successfully(true);

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_EQ(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
            test_tether_connector_->last_canceled_tether_network_guid());

  // Note: This test does not check the active host's status because it will be
  // changed by TetherConnector.
}

TEST_F(TetherDisconnectorTest, DisconnectWhenConnected_NotActuallyConnected) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
      "nonExistentWifiGuid");

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorDisconnectFailed,
            GetResultAndReset());

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorTest,
       DisconnectWhenConnected_WifiConnectionFails_CannotFetchHost) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  SimulateConnectionToWifiNetwork();

  // Remove hosts from |fake_tether_host_fetcher_|; this will cause the fetcher
  // to return a null RemoteDevice.
  fake_tether_host_fetcher_->SetTetherHosts(
      std::vector<cryptauth::RemoteDevice>());

  should_disconnect_successfully_ = false;

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorDisconnectFailed,
            GetResultAndReset());

  // The Wi-Fi network should still be connected since disconnection failed.
  EXPECT_EQ(
      shill::kStateOnline,
      GetServiceStringProperty(wifi_service_path_, shill::kStateProperty));

  // Should not have created any operations since the fetch failed.
  EXPECT_TRUE(fake_operation_factory_->created_operations().empty());

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorTest,
       DisconnectWhenConnected_WifiConnectionSucceeds_CannotFetchHost) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  SimulateConnectionToWifiNetwork();

  // Remove hosts from |fake_tether_host_fetcher_|; this will cause the fetcher
  // to return a null RemoteDevice.
  fake_tether_host_fetcher_->SetTetherHosts(
      std::vector<cryptauth::RemoteDevice>());

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  // The Wi-Fi network should be disconnected since disconnection failed.
  EXPECT_EQ(shill::kStateIdle, GetServiceStringProperty(wifi_service_path_,
                                                        shill::kStateProperty));

  // Should not have created any operations since the fetch failed.
  EXPECT_TRUE(fake_operation_factory_->created_operations().empty());

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorTest,
       DisconnectWhenConnected_WifiConnectionFails_OperationFails) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  SimulateConnectionToWifiNetwork();

  should_disconnect_successfully_ = false;

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorDisconnectFailed,
            GetResultAndReset());

  // The Wi-Fi network should still be connected since disconnection failed.
  EXPECT_EQ(
      shill::kStateOnline,
      GetServiceStringProperty(wifi_service_path_, shill::kStateProperty));

  // Fail the operation.
  ASSERT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(
      test_devices_[0],
      fake_operation_factory_->created_operations()[0]->GetRemoteDevice());
  fake_operation_factory_->created_operations()[0]->NotifyFinished(
      false /* success */);

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorTest,
       DisconnectWhenConnected_WifiConnectionSucceeds_OperationFails) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  SimulateConnectionToWifiNetwork();

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  // The Wi-Fi network should be disconnected since disconnection failed.
  EXPECT_EQ(shill::kStateIdle, GetServiceStringProperty(wifi_service_path_,
                                                        shill::kStateProperty));

  ASSERT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(
      test_devices_[0],
      fake_operation_factory_->created_operations()[0]->GetRemoteDevice());
  fake_operation_factory_->created_operations()[0]->NotifyFinished(
      false /* success */);

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorTest,
       DisconnectWhenConnected_WifiConnectionFails_OperationSucceeds) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  SimulateConnectionToWifiNetwork();

  should_disconnect_successfully_ = false;

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorDisconnectFailed,
            GetResultAndReset());

  // The Wi-Fi network should still be connected since disconnection failed.
  EXPECT_EQ(
      shill::kStateOnline,
      GetServiceStringProperty(wifi_service_path_, shill::kStateProperty));

  // Fail the operation.
  ASSERT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(
      test_devices_[0],
      fake_operation_factory_->created_operations()[0]->GetRemoteDevice());
  fake_operation_factory_->created_operations()[0]->NotifyFinished(
      true /* success */);

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorTest,
       DisconnectWhenConnected_WifiConnectionSucceeds_OperationSucceeds) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  SimulateConnectionToWifiNetwork();

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  // The Wi-Fi network should be disconnected since disconnection failed.
  EXPECT_EQ(shill::kStateIdle, GetServiceStringProperty(wifi_service_path_,
                                                        shill::kStateProperty));

  ASSERT_EQ(1u, fake_operation_factory_->created_operations().size());
  EXPECT_EQ(
      test_devices_[0],
      fake_operation_factory_->created_operations()[0]->GetRemoteDevice());
  fake_operation_factory_->created_operations()[0]->NotifyFinished(
      true /* success */);

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorTest,
       DisconnectWhenConnected_DestroyBeforeOperationComplete) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  SimulateConnectionToWifiNetwork();

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  // Stop the test here, before the operation responds in any way. This test
  // ensures that TetherDisconnector properly removes existing listeners if it
  // is destroyed while there are still active operations.
}

}  // namespace tether

}  // namespace chromeos
