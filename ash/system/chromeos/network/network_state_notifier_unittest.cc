// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_state_notifier.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

ash::SystemTray* GetSystemTray() {
  return ash::Shell::GetPrimaryRootWindowController()->shelf()->
      status_area_widget()->system_tray();
}

}  // namespace

using chromeos::NetworkStateHandler;
using chromeos::DBusThreadManager;
using chromeos::ShillDeviceClient;
using chromeos::ShillServiceClient;

namespace ash {
namespace test {

using internal::NetworkStateNotifier;

class NetworkStateNotifierTest : public AshTestBase {
 public:
  NetworkStateNotifierTest() {}
  virtual ~NetworkStateNotifierTest() {}

  virtual void SetUp() OVERRIDE {
    DBusThreadManager::InitializeWithStub();
    SetupDefaultShillState();
    NetworkStateHandler::Initialize();
    RunAllPendingInMessageLoop();
    AshTestBase::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    AshTestBase::TearDown();
    NetworkStateHandler::Shutdown();
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
    // Create wifi and cellular networks and set to online.
    service_test->AddService("wifi1", "wifi1",
                             flimflam::kTypeWifi, flimflam::kStateOnline,
                             add_to_watchlist);
    RunAllPendingInMessageLoop();
  }

  void SetServiceState(const std::string& service_path,
                       const std::string& state) {
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->SetServiceProperty(service_path, flimflam::kStateProperty,
                                     base::StringValue(state));
    RunAllPendingInMessageLoop();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateNotifierTest);
};

TEST_F(NetworkStateNotifierTest, ConnectionFailure) {
  EXPECT_FALSE(GetSystemTray()->HasNotificationBubble());
  // State -> Failure for non connecting network should not spawn a notification
  SetServiceState("wifi1", flimflam::kStateFailure);
  EXPECT_FALSE(GetSystemTray()->CloseNotificationBubbleForTest());
  // State -> Failure for connecting network should spawn a notification
  SetServiceState("wifi1", flimflam::kStateAssociation);
  NetworkStateHandler::Get()->SetConnectingNetwork("wifi1");
  SetServiceState("wifi1", flimflam::kStateFailure);
  EXPECT_TRUE(GetSystemTray()->CloseNotificationBubbleForTest());
  // Failure -> Idle should not spawn a notification
  SetServiceState("wifi1", flimflam::kStateIdle);
  EXPECT_FALSE(GetSystemTray()->HasNotificationBubble());
  // Idle  -> Failure should also not spawn a notification
  SetServiceState("wifi1", flimflam::kStateFailure);
  EXPECT_FALSE(GetSystemTray()->HasNotificationBubble());
}

}  // namespace test
}  // namespace ash
