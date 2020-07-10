// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_TEST_PAIRING_DELEGATE_H_
#define DEVICE_BLUETOOTH_TEST_TEST_PAIRING_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "base/run_loop.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class TestPairingDelegate : public BluetoothDevice::PairingDelegate {
 public:
  TestPairingDelegate();
  ~TestPairingDelegate() override;

  // BluetoothDevice::PairingDelegate:
  void RequestPinCode(BluetoothDevice* device) override;
  void RequestPasskey(BluetoothDevice* device) override;
  void DisplayPinCode(BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(BluetoothDevice* device, uint32_t passkey) override;
  void KeysEntered(BluetoothDevice* device, uint32_t entered) override;
  void ConfirmPasskey(BluetoothDevice* device, uint32_t passkey) override;
  void AuthorizePairing(BluetoothDevice* device) override;

  int call_count_ = 0;
  int request_pincode_count_ = 0;
  int request_passkey_count_ = 0;
  int display_pincode_count_ = 0;
  int display_passkey_count_ = 0;
  int keys_entered_count_ = 0;
  int confirm_passkey_count_ = 0;
  int authorize_pairing_count_ = 0;
  uint32_t last_passkey_ = 999999u;
  uint32_t last_entered_ = 999u;
  std::string last_pincode_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_TEST_PAIRING_DELEGATE_H_
