// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_CHROMEOS_H_

#include "base/macros.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_le_advertisement_service_provider.h"

namespace bluez {
class BluetoothLEAdvertisementServiceProvider;
}

namespace chromeos {

class BluetoothAdapterChromeOS;

// The BluetoothAdvertisementChromeOS class implements BluetoothAdvertisement
// for the Chrome OS platform.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisementChromeOS
    : public device::BluetoothAdvertisement,
      public bluez::BluetoothLEAdvertisementServiceProvider::Delegate {
 public:
  BluetoothAdvertisementChromeOS(
      scoped_ptr<device::BluetoothAdvertisement::Data> data,
      scoped_refptr<BluetoothAdapterChromeOS> adapter);

  // BluetoothAdvertisement overrides:
  void Unregister(const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) override;

  // bluez::BluetoothLEAdvertisementServiceProvider::Delegate overrides:
  void Released() override;

  void Register(
      const base::Closure& success_callback,
      const device::BluetoothAdapter::CreateAdvertisementErrorCallback&
          error_callback);

  // Used from tests to be able to trigger events on the fake advertisement
  // provider.
  bluez::BluetoothLEAdvertisementServiceProvider* provider() {
    return provider_.get();
  }

 private:
  ~BluetoothAdvertisementChromeOS() override;

  // Adapter this advertisement is advertising on.
  scoped_refptr<BluetoothAdapterChromeOS> adapter_;
  scoped_ptr<bluez::BluetoothLEAdvertisementServiceProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdvertisementChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_CHROMEOS_H_
