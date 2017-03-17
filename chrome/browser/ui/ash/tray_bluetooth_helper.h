// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TRAY_BLUETOOTH_HELPER_H_
#define CHROME_BROWSER_UI_ASH_TRAY_BLUETOOTH_HELPER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
struct BluetoothDeviceInfo;
}

namespace chromeos {
class SystemTrayDelegateChromeOS;
}

namespace device {
class BluetoothDiscoverySession;
}

// Maps UI concepts from the Bluetooth system tray (e.g. "Bluetooth is on") into
// device concepts ("Bluetooth adapter enabled"). Note that most Bluetooth
// device operations are asynchronous, hence the two step initialization.
// TODO(jamescook): Move into //ash/system next to TrayBluetooth. This isn't
// named "delegate" because long-term there should not be any delegation.
class TrayBluetoothHelper : public device::BluetoothAdapter::Observer {
 public:
  explicit TrayBluetoothHelper(
      chromeos::SystemTrayDelegateChromeOS* system_tray_delegate);
  ~TrayBluetoothHelper() override;

  // Called after SystemTray has been instantiated.
  void Initialize();

  // Completes initialization after the Bluetooth adapter is ready.
  void InitializeOnAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Returns a list of available bluetooth devices.
  // TODO(jamescook): Just return the list.
  void GetAvailableDevices(std::vector<ash::BluetoothDeviceInfo>* list);

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

  // TODO: Eliminate this after verifying the initialization order of
  // SystemTrayDelegateChromeOS can be changed.
  chromeos::SystemTrayDelegateChromeOS* const system_tray_delegate_;

  bool should_run_discovery_ = false;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // Object could be deleted during a prolonged Bluetooth operation.
  base::WeakPtrFactory<TrayBluetoothHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TrayBluetoothHelper);
};

#endif  // CHROME_BROWSER_UI_ASH_TRAY_BLUETOOTH_HELPER_H_
