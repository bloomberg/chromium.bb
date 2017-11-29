// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/network_configuration_remover.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/mock_managed_network_configuration_handler.h"
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

const char kWifiNetworkGuid[] = "wifiNetworkGuid";

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

class NetworkConfigurationRemoverTest : public NetworkStateTest {
 protected:
  NetworkConfigurationRemoverTest() : NetworkStateTest() {}
  ~NetworkConfigurationRemoverTest() override = default;

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    wifi_service_path_ =
        ConfigureService(CreateConnectedWifiConfigurationJsonString());

    mock_managed_network_configuration_manager_ =
        base::WrapUnique(new NiceMock<MockManagedNetworkConfigurationHandler>);

    network_configuration_remover_ =
        base::WrapUnique(new NetworkConfigurationRemover(
            network_state_handler(),
            mock_managed_network_configuration_manager_.get()));
  }

  void TearDown() override {
    // Delete manager before the NetworkStateHandler.
    network_configuration_remover_.reset();
    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::string wifi_service_path_;

  std::unique_ptr<MockManagedNetworkConfigurationHandler>
      mock_managed_network_configuration_manager_;

  std::unique_ptr<NetworkConfigurationRemover> network_configuration_remover_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationRemoverTest);
};

TEST_F(NetworkConfigurationRemoverTest, TestRemoveNetworkConfiguration) {
  EXPECT_CALL(*mock_managed_network_configuration_manager_,
              RemoveConfiguration(wifi_service_path_, _, _))
      .Times(1);

  network_configuration_remover_->RemoveNetworkConfiguration(kWifiNetworkGuid);
}

}  // namespace tether

}  // namespace chromeos
