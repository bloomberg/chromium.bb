// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_CHROMEOS_H_

#include "base/macros.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace device {

class MockBluetoothAdvertisement : public device::BluetoothAdvertisement {
 public:
  MockBluetoothAdvertisement();

  // BluetoothAdvertisement overrides:
  void Unregister(const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) override;

 private:
  ~MockBluetoothAdvertisement() override;

  DISALLOW_COPY_AND_ASSIGN(MockBluetoothAdvertisement);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_CHROMEOS_H_
