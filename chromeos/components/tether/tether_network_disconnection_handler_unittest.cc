// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_network_disconnection_handler.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

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

}  // namespace

class TetherNetworkDisconnectionHandlerTest : public NetworkStateTest {
 protected:
  TetherNetworkDisconnectionHandlerTest() : NetworkStateTest() {}
  ~TetherNetworkDisconnectionHandlerTest() override {}

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    wifi_service_path_ =
        ConfigureService(CreateConnectedWifiConfigurationJsonString());

    fake_active_host_ = base::MakeUnique<FakeActiveHost>();

    handler_ = base::WrapUnique(new TetherNetworkDisconnectionHandler(
        fake_active_host_.get(), network_state_handler()));
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

  std::unique_ptr<TetherNetworkDisconnectionHandler> handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherNetworkDisconnectionHandlerTest);
};

TEST_F(TetherNetworkDisconnectionHandlerTest, TestConnectAndDisconnect) {
  // Connect to the network. |handler_| should start tracking the connection.
  fake_active_host_->SetActiveHostConnecting(kDeviceId, kTetherNetworkGuid);
  fake_active_host_->SetActiveHostConnected(kDeviceId, kTetherNetworkGuid,
                                            kWifiNetworkGuid);

  // Now, disconnect the Wi-Fi network. This should result in
  // |fake_active_host_| becoming disconnected.
  NotifyDisconnected();
  EXPECT_EQ(ActiveHost::ActiveHostStatus::DISCONNECTED,
            fake_active_host_->GetActiveHostStatus());
}

}  // namespace tether

}  // namespace cryptauth
