// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_PAIRING_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_PAIRING_DELEGATE_H_

#include <string>

#include "chrome/common/extensions/api/bluetooth_private.h"
#include "device/bluetooth/bluetooth_device.h"

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
  virtual ~BluetoothApiPairingDelegate();

  // device::PairingDelegate overrides:
  virtual void RequestPinCode(device::BluetoothDevice* device) OVERRIDE;
  virtual void RequestPasskey(device::BluetoothDevice* device) OVERRIDE;
  virtual void DisplayPinCode(device::BluetoothDevice* device,
                              const std::string& pincode) OVERRIDE;
  virtual void DisplayPasskey(device::BluetoothDevice* device,
                              uint32 passkey) OVERRIDE;
  virtual void KeysEntered(device::BluetoothDevice* device,
                           uint32 entered) OVERRIDE;
  virtual void ConfirmPasskey(device::BluetoothDevice* device,
                              uint32 passkey) OVERRIDE;
  virtual void AuthorizePairing(device::BluetoothDevice* device) OVERRIDE;

 private:
  // Dispatches a pairing event to the extension.
  void DispatchPairingEvent(
      const api::bluetooth_private::PairingEvent& pairing_event);

  std::string extension_id_;
  content::BrowserContext* browser_context_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_PAIRING_DELEGATE_H_
