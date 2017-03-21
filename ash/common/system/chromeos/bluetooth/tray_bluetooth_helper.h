// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_BLUETOOTH_TRAY_BLUETOOTH_HELPER_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_BLUETOOTH_TRAY_BLUETOOTH_HELPER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {
class BluetoothDiscoverySession;
}

namespace ash {

struct BluetoothDeviceInfo;

// Maps UI concepts from the Bluetooth system tray (e.g. "Bluetooth is on") into
// device concepts ("Bluetooth adapter enabled"). Note that most Bluetooth
// device operations are asynchronous, hence the two step initialization.
// Exported for test.
class ASH_EXPORT TrayBluetoothHelper
    : public device::BluetoothAdapter::Observer {
 public:
  TrayBluetoothHelper();
  ~TrayBluetoothHelper() override;

  // Initializes and gets the adapter asynchronously.
  void Initialize();

  // Completes initialization after the Bluetooth adapter is ready.
  void InitializeOnAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Returns a list of available bluetooth devices.
  // TODO(jamescook): Just return the list.
  void GetAvailableDevices(std::vector<BluetoothDeviceInfo>* list);

  // Requests bluetooth start discovering devices, which happens asynchronously.
  void StartDiscovering();

  // Requests bluetooth stop discovering devices.
  void StopDiscovering();

  // Connect to a specific bluetooth device.
  void ConnectToDevice(const std::string& address);

  // Returns true if bluetooth adapter is discovering bluetooth devices.
  bool IsDiscovering() const;

  // Toggles whether bluetooth is enabled.
  void ToggleEnabled();

  // Returns whether bluetooth capability is available (e.g. the device has
  // hardware support).
  bool GetAvailable();

  // Returns whether bluetooth is enabled.
  bool GetEnabled();

  // Returns whether the delegate has initiated a bluetooth discovery session.
  // TODO(jamescook): Why do we need both this and IsDiscovering()?
  bool HasDiscoverySession();

  // BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                 bool discovering) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 private:
  void OnStartDiscoverySession(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);

  bool should_run_discovery_ = false;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // Object could be deleted during a prolonged Bluetooth operation.
  base::WeakPtrFactory<TrayBluetoothHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TrayBluetoothHelper);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_BLUETOOTH_TRAY_BLUETOOTH_HELPER_H_
