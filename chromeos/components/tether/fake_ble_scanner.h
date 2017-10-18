// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_BLE_SCANNER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_BLE_SCANNER_H_

#include <vector>

#include "base/macros.h"
#include "chromeos/components/tether/ble_scanner.h"
#include "components/cryptauth/remote_device.h"

namespace device {
class BluetoothDevice;
}

namespace chromeos {

namespace tether {

// Test double for BleScanner.
class FakeBleScanner : public BleScanner {
 public:
  // If |automatically_update_discovery_session| is true,
  // IsDiscoverySessionActive() will simply return whether at least once device
  // is registered; otherwise, that value must be determined manually via
  // set_is_discovery_session_active().
  FakeBleScanner(bool automatically_update_discovery_session);
  ~FakeBleScanner() override;

  const std::vector<cryptauth::RemoteDevice>& registered_devices() {
    return registered_devices_;
  }

  void set_should_fail_to_register(bool should_fail_to_register) {
    should_fail_to_register_ = should_fail_to_register;
  }

  void set_is_discovery_session_active(bool is_discovery_session_active) {
    is_discovery_session_active_ = is_discovery_session_active;
  }

  void NotifyReceivedAdvertisementFromDevice(
      const cryptauth::RemoteDevice& remote_device,
      device::BluetoothDevice* bluetooth_device);
  void NotifyDiscoverySessionStateChanged(bool discovery_session_active);

  // BleScanner:
  bool RegisterScanFilterForDevice(
      const cryptauth::RemoteDevice& remote_device) override;
  bool UnregisterScanFilterForDevice(
      const cryptauth::RemoteDevice& remote_device) override;
  bool ShouldDiscoverySessionBeActive() override;
  bool IsDiscoverySessionActive() override;

 private:
  const bool automatically_update_discovery_session_;

  bool should_fail_to_register_ = false;
  bool is_discovery_session_active_ = false;
  std::vector<cryptauth::RemoteDevice> registered_devices_;

  DISALLOW_COPY_AND_ASSIGN(FakeBleScanner);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_BLE_SCANNER_H_
