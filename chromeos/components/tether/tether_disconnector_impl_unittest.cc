// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_disconnector_impl.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/components/tether/connect_tethering_operation.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/fake_network_configuration_remover.h"
#include "chromeos/components/tether/fake_tether_connector.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/components/tether/fake_wifi_hotspot_connector.h"
#include "chromeos/components/tether/mock_tether_host_response_recorder.h"
#include "chromeos/components/tether/pref_names.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace tether {

namespace {

const char kSuccessResult[] = "success";

const char kWifiNetworkGuid[] = "wifiNetworkGuid";

std::string CreateConnectedWifiConfigurationJsonString(
    const std::string& wifi_network_guid) {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << wifi_network_guid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateOnline << "\""
     << "}";
  return ss.str();
}

class TestNetworkConnectionHandler : public NetworkConnectionHandler {
 public:
  explicit TestNetworkConnectionHandler(base::Closure disconnect_callback)
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

}  // namespace

class TetherDisconnectorImplTest : public NetworkStateTest {
 public:
  TetherDisconnectorImplTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(2u)) {}
  ~TetherDisconnectorImplTest() override {}

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    should_run_disconnect_callbacks_synchronously_ = true;
    should_disconnect_successfully_ = true;

    test_network_connection_handler_ =
        base::WrapUnique(new TestNetworkConnectionHandler(base::Bind(
            &TetherDisconnectorImplTest::OnNetworkConnectionManagerDisconnect,
            base::Unretained(this))));
    fake_active_host_ = base::MakeUnique<FakeActiveHost>();
    fake_network_configuration_remover_ =
        base::MakeUnique<FakeNetworkConfigurationRemover>();
    fake_tether_connector_ = base::MakeUnique<FakeTetherConnector>();
    device_id_tether_network_guid_map_ =
        base::MakeUnique<DeviceIdTetherNetworkGuidMap>();
    test_pref_service_ = base::MakeUnique<TestingPrefServiceSimple>();

    fake_disconnect_tethering_request_sender_ =
        base::MakeUnique<FakeDisconnectTetheringRequestSender>();

    SetUpTetherNetworks();

    TetherDisconnectorImpl::RegisterPrefs(test_pref_service_->registry());
    tether_disconnector_ = base::MakeUnique<TetherDisconnectorImpl>(
        test_network_connection_handler_.get(), network_state_handler(),
        fake_active_host_.get(),
        fake_disconnect_tethering_request_sender_.get(),
        fake_network_configuration_remover_.get(), fake_tether_connector_.get(),
        device_id_tether_network_guid_map_.get(), test_pref_service_.get());
  }

  void TearDown() override {
    tether_disconnector_.reset();
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
    // TetherDisconnectorImpl::DisconnectFromNetwork() is called.
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
    wifi_service_path_ = ConfigureService(
        CreateConnectedWifiConfigurationJsonString(kWifiNetworkGuid));
    EXPECT_FALSE(wifi_service_path_.empty());
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

  // This function is called by
  // TestNetworkConnectionHandler::DisconnectFromNetwork().
  void OnNetworkConnectionManagerDisconnect() {
    EXPECT_EQ(wifi_service_path_,
              test_network_connection_handler_->last_disconnect_service_path());

    if (should_disconnect_successfully_) {
      SetServiceProperty(wifi_service_path_, shill::kStateProperty,
                         base::Value(shill::kStateIdle));
    }

    if (should_run_disconnect_callbacks_synchronously_) {
      // Before the callbacks are invoked, the network configuration should not
      // yet have been cleared, and the disconnecting GUID should still be in
      // prefs.
      EXPECT_TRUE(
          fake_network_configuration_remover_->last_removed_wifi_network_guid()
              .empty());
      EXPECT_FALSE(GetDisconnectingWifiGuidFromPrefs().empty());

      if (should_disconnect_successfully_) {
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
            wifi_service_path_,
            NetworkConnectionHandler::kErrorDisconnectFailed,
            "" /* error_detail */);
      }

      // Now that the callbacks have been invoked, both the network
      // configuration and the disconnecting GUID should have cleared.
      EXPECT_FALSE(
          fake_network_configuration_remover_->last_removed_wifi_network_guid()
              .empty());
      EXPECT_TRUE(GetDisconnectingWifiGuidFromPrefs().empty());
    }
  }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(disconnection_result_);
    return result;
  }

  std::string GetDisconnectingWifiGuidFromPrefs() {
    return test_pref_service_->GetString(prefs::kDisconnectingWifiNetworkGuid);
  }

  void VerifyNoDisconnectTetheringRequestSent() {
    EXPECT_TRUE(
        fake_disconnect_tethering_request_sender_->device_ids_sent_requests()
            .empty());
  }

  const std::vector<cryptauth::RemoteDevice> test_devices_;
  const base::MessageLoop message_loop_;

  std::unique_ptr<TestNetworkConnectionHandler>
      test_network_connection_handler_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeNetworkConfigurationRemover>
      fake_network_configuration_remover_;
  std::unique_ptr<FakeTetherConnector> fake_tether_connector_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;

  std::unique_ptr<FakeDisconnectTetheringRequestSender>
      fake_disconnect_tethering_request_sender_;

  std::string wifi_service_path_;
  std::string disconnection_result_;
  bool should_run_disconnect_callbacks_synchronously_;
  bool should_disconnect_successfully_;

  std::unique_ptr<TetherDisconnectorImpl> tether_disconnector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherDisconnectorImplTest);
};

TEST_F(TetherDisconnectorImplTest, DisconnectWhenAlreadyDisconnected) {
  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());

  // No DisconnectTetheringRequest should be sent.
  VerifyNoDisconnectTetheringRequestSent();

  // Should still be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorImplTest, DisconnectWhenOtherDeviceConnected) {
  wifi_service_path_ = ConfigureService(
      CreateConnectedWifiConfigurationJsonString("otherWifiNetworkGuid"));
  fake_active_host_->SetActiveHostConnected(
      test_devices_[1].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[1].GetDeviceId()),
      "otherWifiNetworkGuid");

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());

  // No DisconnectTetheringRequest should be sent.
  VerifyNoDisconnectTetheringRequestSent();

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

  // No DisconnectTetheringRequest should be sent.
  VerifyNoDisconnectTetheringRequestSent();

  // Note: This test does not check the active host's status because it will be
  // changed by TetherConnector.
}

TEST_F(TetherDisconnectorImplTest,
       DisconnectWhenConnected_NotActuallyConnected) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()),
      "nonExistentWifiGuid");

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(NetworkConnectionHandler::kErrorDisconnectFailed,
            GetResultAndReset());

  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorImplTest,
       DisconnectWhenConnected_WifiDisconnectionFails) {
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

  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorImplTest,
       DisconnectWhenConnected_WifiDisconnectionSucceeds) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  SimulateConnectionToWifiNetwork();

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  // The Wi-Fi network should be disconnected.
  EXPECT_EQ(shill::kStateIdle, GetServiceStringProperty(wifi_service_path_,
                                                        shill::kStateProperty));

  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

TEST_F(TetherDisconnectorImplTest,
       DisconnectWhenConnected_DestroyBeforeOperationComplete) {
  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  SimulateConnectionToWifiNetwork();

  CallDisconnect(GetTetherNetworkGuid(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());

  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());

  // Stop the test here, before the operation responds in any way. This test
  // ensures that TetherDisconnectorImpl properly removes existing listeners
  // if it is destroyed while there are still active operations.
}

TEST_F(TetherDisconnectorImplTest, DisconnectsWhenDestructorCalled) {
  // For this test, do not synchronously reply with results. This echos what
  // actually happens when a TetherDisconnectorImpl is deleted.
  should_run_disconnect_callbacks_synchronously_ = false;

  fake_active_host_->SetActiveHostConnected(
      test_devices_[0].GetDeviceId(),
      GetTetherNetworkGuid(test_devices_[0].GetDeviceId()), kWifiNetworkGuid);
  SimulateConnectionToWifiNetwork();

  // Destroy the object, which should result in a disconnection.
  tether_disconnector_.reset();

  // Because the object is destroyed before the disconnection callback occurs,
  // no result should have been able to be set.
  EXPECT_EQ(std::string(), GetResultAndReset());

  // The Wi-Fi network should be disconnected.
  EXPECT_EQ(shill::kStateIdle, GetServiceStringProperty(wifi_service_path_,
                                                        shill::kStateProperty));

  EXPECT_EQ(
      std::vector<std::string>{test_devices_[0].GetDeviceId()},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());

  // Should be disconnected.
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());

  // Because the disconnection did not fully complete, the configuration should
  // not yet have been cleaned up, and prefs should still contain the
  // disconnecting Wi-Fi GUID.
  EXPECT_TRUE(
      fake_network_configuration_remover_->last_removed_wifi_network_guid()
          .empty());
  EXPECT_EQ(kWifiNetworkGuid, GetDisconnectingWifiGuidFromPrefs());

  // Now, create a new TetherDisconnectorImpl instance. This should clean up
  // the previous disconnection attempt.
  tether_disconnector_ = base::MakeUnique<TetherDisconnectorImpl>(
      test_network_connection_handler_.get(), network_state_handler(),
      fake_active_host_.get(), fake_disconnect_tethering_request_sender_.get(),
      fake_network_configuration_remover_.get(), fake_tether_connector_.get(),
      device_id_tether_network_guid_map_.get(), test_pref_service_.get());
  EXPECT_EQ(
      kWifiNetworkGuid,
      fake_network_configuration_remover_->last_removed_wifi_network_guid());
  EXPECT_TRUE(GetDisconnectingWifiGuidFromPrefs().empty());
}

}  // namespace tether

}  // namespace chromeos
