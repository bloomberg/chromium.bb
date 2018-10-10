// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_EXPERIMENTAL_H_
#define ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_EXPERIMENTAL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "base/macros.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace ash {

// Implementation of TrayBluetoothHelper on top of the BluetoothSystem Mojo
// interface.
class TrayBluetoothHelperExperimental : public TrayBluetoothHelper {
 public:
  TrayBluetoothHelperExperimental();
  ~TrayBluetoothHelperExperimental() override;

  // TrayBluetoothHelper:
  void Initialize() override;
  BluetoothDeviceList GetAvailableBluetoothDevices() const override;
  void StartBluetoothDiscovering() override;
  void StopBluetoothDiscovering() override;
  void ConnectToBluetoothDevice(const std::string& address) override;
  device::mojom::BluetoothSystem::State GetBluetoothState() override;
  void SetBluetoothEnabled(bool enabled) override;
  bool HasBluetoothDiscoverySession() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayBluetoothHelperExperimental);
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_EXPERIMENTAL_H_
