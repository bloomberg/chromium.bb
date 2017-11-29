// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_network_disconnection_handler.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/network_configuration_remover.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using testing::_;
using testing::NiceMock;

namespace chromeos {

namespace tether {

namespace {

const char kDeviceId[] = "deviceId";
const char kWifiNetworkGuid[] = "wifiNetworkGuid";
const char kTetherNetworkGuid[] = "tetherNetworkGuid";

std::string CreateConnectedWifiConfigurationJsonString() {
  std::stringstream ss;
  ss << "{"
     << "  \"GUID\": \"" << kWifiNetworkGuid << "\","
     << "  \"Type\": \"" << shill::kTypeWifi << "\","
     << "  \"State\": \"" << shill::kStateReady << "\""
     << "}";
  return ss.str();
}

class MockNetworkConfigurationRemover : public NetworkConfigurationRemover {
 public:
  MockNetworkConfigurationRemover()
      : NetworkConfigurationRemover(nullptr, nullptr) {}
  ~MockNetworkConfigurationRemover() override = default;

  MOCK_METHOD1(RemoveNetworkConfiguration, void(const std::string&));
};

}  // namespace

class TetherNetworkDisconnectionHandlerTest : public NetworkStateTest {
 protected:
  TetherNetworkDisconnectionHandlerTest() : NetworkStateTest() {}
  ~TetherNetworkDisconnectionHandlerTest() override = default;

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    wifi_service_path_ =
        ConfigureService(CreateConnectedWifiConfigurationJsonString());

    fake_active_host_ = base::MakeUnique<FakeActiveHost>();
    fake_disconnect_tethering_request_sender_ =
        base::MakeUnique<FakeDisconnectTetheringRequestSender>();
    mock_network_configuration_remover_ =
        base::WrapUnique(new NiceMock<MockNetworkConfigurationRemover>);

    handler_ = base::WrapUnique(new TetherNetworkDisconnectionHandler(
        fake_active_host_.get(), network_state_handler(),
        mock_network_configuration_remover_.get(),
        fake_disconnect_tethering_request_sender_.get()));
  }

  void TearDown() override {
    // Delete handler before the NetworkStateHandler and |fake_active_host_|.
    handler_.reset();
    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  void NotifyDisconnected() {
    SetServiceProperty(wifi_service_path_, std::string(shill::kStateProperty),
                       base::Value(shill::kStateIdle));
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::string wifi_service_path_;

  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeDisconnectTetheringRequestSender>
      fake_disconnect_tethering_request_sender_;
  std::unique_ptr<MockNetworkConfigurationRemover>
      mock_network_configuration_remover_;

  std::unique_ptr<TetherNetworkDisconnectionHandler> handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherNetworkDisconnectionHandlerTest);
};

TEST_F(TetherNetworkDisconnectionHandlerTest, TestConnectAndDisconnect) {
  EXPECT_CALL(*mock_network_configuration_remover_,
              RemoveNetworkConfiguration(kWifiNetworkGuid));

  // Connect to the network. |handler_| should start tracking the connection.
  fake_active_host_->SetActiveHostConnecting(kDeviceId, kTetherNetworkGuid);
  fake_active_host_->SetActiveHostConnected(kDeviceId, kTetherNetworkGuid,
                                            kWifiNetworkGuid);

  // Now, disconnect the Wi-Fi network. This should result in
  // |fake_active_host_| becoming disconnected.
  NotifyDisconnected();

  EXPECT_EQ(
      std::vector<std::string>{kDeviceId},
      fake_disconnect_tethering_request_sender_->device_ids_sent_requests());

  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

}  // namespace tether

}  // namespace chromeos
