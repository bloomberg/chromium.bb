// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/bluetooth/tray_bluetooth_helper.h"

#include <vector>

#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"

using bluez::BluezDBusManager;
using bluez::FakeBluetoothAdapterClient;

namespace ash {
namespace {

using TrayBluetoothHelperTest = test::AshTestBase;

// Tests basic functionality like turning Bluetooth on and off.
TEST_F(TrayBluetoothHelperTest, Basics) {
  // Set Bluetooth discovery simulation delay to 0 so the test doesn't have to
  // wait or use timers.
  FakeBluetoothAdapterClient* adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          BluezDBusManager::Get()->GetBluetoothAdapterClient());
  adapter_client->SetSimulationIntervalMs(0);

  TrayBluetoothHelper helper;
  helper.Initialize();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(helper.GetAvailable());
  EXPECT_FALSE(helper.GetEnabled());
  EXPECT_FALSE(helper.HasDiscoverySession());
  EXPECT_FALSE(helper.IsDiscovering());

  std::vector<ash::BluetoothDeviceInfo> devices;
  helper.GetAvailableDevices(&devices);
  // The devices are fake in tests, so don't assume any particular number.
  EXPECT_FALSE(devices.empty());

  // Turn Bluetooth on.
  helper.ToggleEnabled();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(helper.GetEnabled());

  helper.StartDiscovering();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(helper.HasDiscoverySession());
  EXPECT_TRUE(helper.IsDiscovering());

  helper.StopDiscovering();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(helper.HasDiscoverySession());
  EXPECT_FALSE(helper.IsDiscovering());

  // Turn Bluetooth off.
  helper.ToggleEnabled();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(helper.GetEnabled());
}

}  // namespace
}  // namespace ash
