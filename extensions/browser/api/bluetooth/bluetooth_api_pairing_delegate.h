// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_PAIRING_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_PAIRING_DELEGATE_H_

#include <string>

#include "device/bluetooth/bluetooth_device.h"
#include "extensions/common/api/bluetooth_private.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A pairing delegate to dispatch incoming Bluetooth pairing events to the API
// event router.
class BluetoothApiPairingDelegate
    : public device::BluetoothDevice::PairingDelegate {
 public:
  BluetoothApiPairingDelegate(const std::string& extension_id,
                              content::BrowserContext* browser_context);
  ~BluetoothApiPairingDelegate() override;

  // device::PairingDelegate overrides:
  void RequestPinCode(device::BluetoothDevice* device) override;
  void RequestPasskey(device::BluetoothDevice* device) override;
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(device::BluetoothDevice* device, uint32 passkey) override;
  void KeysEntered(device::BluetoothDevice* device, uint32 entered) override;
  void ConfirmPasskey(device::BluetoothDevice* device, uint32 passkey) override;
  void AuthorizePairing(device::BluetoothDevice* device) override;

 private:
  // Dispatches a pairing event to the extension.
  void DispatchPairingEvent(
      const core_api::bluetooth_private::PairingEvent& pairing_event);

  std::string extension_id_;
  content::BrowserContext* browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_PAIRING_DELEGATE_H_
