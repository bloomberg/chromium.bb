// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_state_notifier.h"

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/message_center.h"

namespace chromeos {
namespace test {

namespace {

const char kWiFi1ServicePath[] = "/service/wifi1";
const char kWiFi1Guid[] = "wifi1_guid";

}  // namespace

class NetworkConnectTestDelegate : public NetworkConnect::Delegate {
 public:
  NetworkConnectTestDelegate()
      : network_state_notifier_(new NetworkStateNotifier()) {}
  ~NetworkConnectTestDelegate() override {}

  // NetworkConnect::Delegate
  void ShowNetworkConfigure(const std::string& network_id) override {}
  void ShowNetworkSettings(const std::string& network_id) override {}
  bool ShowEnrollNetwork(const std::string& network_id) override {
    return false;
  }
  void ShowMobileSimDialog() override {}
  void ShowMobileSetupDialog(const std::string& service_path) override {}
  void ShowNetworkConnectError(const std::string& error_name,
                               const std::string& network_id) override {
    network_state_notifier_->ShowNetworkConnectError(error_name, network_id);
  }
  void ShowMobileActivationError(const std::string& network_id) override {}

 private:
  std::unique_ptr<NetworkStateNotifier> network_state_notifier_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectTestDelegate);
};

class NetworkStateNotifierTest : public testing::Test {
 public:
  NetworkStateNotifierTest() {}
  ~NetworkStateNotifierTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    DBusThreadManager::Initialize();
    LoginState::Initialize();
    SetupDefaultShillState();
    NetworkHandler::Initialize();
    message_center::MessageCenter::Initialize();
    base::RunLoop().RunUntilIdle();
    network_connect_delegate_.reset(new NetworkConnectTestDelegate);
    NetworkConnect::Initialize(network_connect_delegate_.get());
  }

  void TearDown() override {
    NetworkConnect::Shutdown();
    network_connect_delegate_.reset();
    message_center::MessageCenter::Shutdown();
    LoginState::Shutdown();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
    testing::Test::TearDown();
  }

 protected:
  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();
    ShillDeviceClient::TestInterface* device_test =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice("/device/stub_wifi_device1", shill::kTypeWifi,
                           "stub_wifi_device1");
    device_test->AddDevice("/device/stub_cellular_device1",
                           shill::kTypeCellular, "stub_cellular_device1");

    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    const bool add_to_visible = true;
    // Create a wifi network and set to online.
    service_test->AddService(kWiFi1ServicePath, kWiFi1Guid, "wifi1",
                             shill::kTypeWifi, shill::kStateIdle,
                             add_to_visible);
    service_test->SetServiceProperty(kWiFi1ServicePath,
                                     shill::kSecurityClassProperty,
                                     base::Value(shill::kSecurityWep));
    service_test->SetServiceProperty(
        kWiFi1ServicePath, shill::kConnectableProperty, base::Value(true));
    service_test->SetServiceProperty(
        kWiFi1ServicePath, shill::kPassphraseProperty, base::Value("failure"));
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<NetworkConnectTestDelegate> network_connect_delegate_;
  base::MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateNotifierTest);
};

TEST_F(NetworkStateNotifierTest, ConnectionFailure) {
  NetworkConnect::Get()->ConnectToNetworkId(kWiFi1Guid);
  base::RunLoop().RunUntilIdle();
  // Failure should spawn a notification.
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
      NetworkStateNotifier::kNetworkConnectNotificationId));
}

}  // namespace test
}  // namespace chromeos
