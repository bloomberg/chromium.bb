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
#include "chromeos/network/network_connection_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

ash::SystemTray* GetSystemTray() {
  return ash::Shell::GetPrimaryRootWindowController()->shelf()->
      status_area_widget()->system_tray();
}

}  // namespace

using chromeos::DBusThreadManager;
using chromeos::NetworkHandler;
using chromeos::NetworkConnectionHandler;
using chromeos::ShillDeviceClient;
using chromeos::ShillServiceClient;

namespace ash {
namespace test {

class NetworkStateNotifierTest : public AshTestBase {
 public:
  NetworkStateNotifierTest() {}
  virtual ~NetworkStateNotifierTest() {}

  virtual void SetUp() OVERRIDE {
    DBusThreadManager::InitializeWithStub();
    SetupDefaultShillState();
    NetworkHandler::Initialize();
    RunAllPendingInMessageLoop();
    AshTestBase::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    AshTestBase::TearDown();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  void SetupDefaultShillState() {
    RunAllPendingInMessageLoop();
    ShillDeviceClient::TestInterface* device_test =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    device_test->ClearDevices();
    device_test->AddDevice("/device/stub_wifi_device1",
                           flimflam::kTypeWifi, "stub_wifi_device1");
    device_test->AddDevice("/device/stub_cellular_device1",
                           flimflam::kTypeCellular, "stub_cellular_device1");

    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    const bool add_to_watchlist = true;
    const bool add_to_visible = true;
    // Create wifi and cellular networks and set to online.
    service_test->AddService("wifi1", "wifi1",
                             flimflam::kTypeWifi, flimflam::kStateIdle,
                             add_to_visible, add_to_watchlist);
    service_test->SetServiceProperty("wifi1",
                                     flimflam::kSecurityProperty,
                                     base::StringValue(flimflam::kSecurityWep));
    service_test->SetServiceProperty("wifi1",
                                     flimflam::kConnectableProperty,
                                     base::FundamentalValue(true));
    service_test->SetServiceProperty("wifi1",
                                     flimflam::kPassphraseProperty,
                                     base::StringValue("failure"));
    RunAllPendingInMessageLoop();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateNotifierTest);
};

TEST_F(NetworkStateNotifierTest, ConnectionFailure) {
  EXPECT_FALSE(GetSystemTray()->HasNotificationBubble());
  ash::network_connect::ConnectToNetwork("wifi1", NULL /* owning_window */);
  RunAllPendingInMessageLoop();
  // Failure should spawn a notification.
  EXPECT_TRUE(GetSystemTray()->CloseNotificationBubbleForTest());
}

}  // namespace test
}  // namespace ash
