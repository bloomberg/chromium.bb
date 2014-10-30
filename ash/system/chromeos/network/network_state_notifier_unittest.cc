// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_state_notifier.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/chromeos/network/network_connect.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/message_center.h"

namespace {

ash::SystemTray* GetSystemTray() {
  return ash::Shell::GetPrimaryRootWindowController()
      ->shelf()
      ->status_area_widget()
      ->system_tray();
}

}  // namespace

using chromeos::DBusThreadManager;
using chromeos::ShillDeviceClient;
using chromeos::ShillServiceClient;

namespace ash {
namespace test {

class NetworkConnectTestDelegate : public NetworkConnect::Delegate {
 public:
  NetworkConnectTestDelegate() {}
  ~NetworkConnectTestDelegate() override {}

  // NetworkConnect::Delegate
  void ShowNetworkConfigure(const std::string& network_id) override {}
  void ShowNetworkSettings(const std::string& network_id) override {}
  bool ShowEnrollNetwork(const std::string& network_id) override {
    return false;
  }
  void ShowMobileSimDialog() override {}
  void ShowMobileSetupDialog(const std::string& service_path) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectTestDelegate);
};

class NetworkStateNotifierTest : public AshTestBase {
 public:
  NetworkStateNotifierTest() {}
  ~NetworkStateNotifierTest() override {}

  void SetUp() override {
    DBusThreadManager::Initialize();
    chromeos::LoginState::Initialize();
    SetupDefaultShillState();
    chromeos::NetworkHandler::Initialize();
    RunAllPendingInMessageLoop();
    AshTestBase::SetUp();
    network_connect_delegate_.reset(new NetworkConnectTestDelegate);
    NetworkConnect::Initialize(network_connect_delegate_.get());
  }

  void TearDown() override {
    NetworkConnect::Shutdown();
    network_connect_delegate_.reset();
    AshTestBase::TearDown();
    chromeos::LoginState::Shutdown();
    chromeos::NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  void SetupDefaultShillState() {
    RunAllPendingInMessageLoop();
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
    service_test->AddService("/service/wifi1", "wifi1_guid", "wifi1",
                             shill::kTypeWifi, shill::kStateIdle,
                             add_to_visible);
    service_test->SetServiceProperty("wifi1", shill::kSecurityProperty,
                                     base::StringValue(shill::kSecurityWep));
    service_test->SetServiceProperty("wifi1", shill::kConnectableProperty,
                                     base::FundamentalValue(true));
    service_test->SetServiceProperty("wifi1", shill::kPassphraseProperty,
                                     base::StringValue("failure"));
    RunAllPendingInMessageLoop();
  }

  scoped_ptr<NetworkConnectTestDelegate> network_connect_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateNotifierTest);
};

TEST_F(NetworkStateNotifierTest, ConnectionFailure) {
  EXPECT_FALSE(GetSystemTray()->HasNotificationBubble());
  NetworkConnect::Get()->ConnectToNetwork("wifi1");
  RunAllPendingInMessageLoop();
  // Failure should spawn a notification.
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  EXPECT_TRUE(message_center->FindVisibleNotificationById(
      NetworkStateNotifier::kNetworkConnectNotificationId));
}

}  // namespace test
}  // namespace ash
