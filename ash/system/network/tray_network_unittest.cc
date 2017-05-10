// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network.h"

#include "ash/login_status.h"
#include "ash/system/network/network_list.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;

namespace ash {
namespace {

class TrayNetworkTest : public test::AshTestBase {
 public:
  TrayNetworkTest() = default;
  ~TrayNetworkTest() override = default;

  // testing::Test:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    // Initializing NetworkHandler before ash is more like production.
    chromeos::NetworkHandler::Initialize();
    test::AshTestBase::SetUp();
    chromeos::NetworkHandler::Get()->InitializePrefServices(&profile_prefs_,
                                                            &local_state_);
    // Networking stubs may have asynchronous initialization.
    RunAllPendingInMessageLoop();
  }

  void TearDown() override {
    // This roughly matches production shutdown order.
    chromeos::NetworkHandler::Get()->ShutdownPrefServices();
    test::AshTestBase::TearDown();
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

 private:
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetworkTest);
};

// Verifies that the network views can be created.
TEST_F(TrayNetworkTest, Basics) {
  // Open the system tray menu.
  SystemTray* system_tray = GetPrimarySystemTray();
  system_tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  RunAllPendingInMessageLoop();

  // Show network details.
  TrayNetwork* tray_network = system_tray->GetTrayNetworkForTesting();
  const int close_delay_in_seconds = 0;
  bool activate = true;
  system_tray->ShowDetailedView(tray_network, close_delay_in_seconds, activate,
                                BUBBLE_USE_EXISTING);
  RunAllPendingInMessageLoop();

  // Network details view was created.
  ASSERT_TRUE(tray_network->detailed());
  EXPECT_TRUE(tray_network->detailed()->visible());
}

// Verifies that toggling Wi-Fi (usually via keyboard) shows a notification.
TEST_F(TrayNetworkTest, ToggleWifi) {
  TrayNetwork* tray_network =
      GetPrimarySystemTray()->GetTrayNetworkForTesting();

  // No notifications at startup.
  ASSERT_EQ(0u, MessageCenter::Get()->NotificationCount());

  // Simulate a user action to toggle Wi-Fi.
  tray_network->RequestToggleWifi();

  // Notification was shown.
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());
  EXPECT_TRUE(MessageCenter::Get()->HasPopupNotifications());
  EXPECT_TRUE(MessageCenter::Get()->FindVisibleNotificationById("wifi-toggle"));
}

}  // namespace
}  // namespace ash
