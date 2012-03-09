// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/dbus/bluetooth_device_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class BluetoothDeviceImpl : public BluetoothDevice {
 public:
  BluetoothDeviceImpl(const std::string& address,
                      uint32 bluetooth_class,
                      const std::string& icon,
                      const std::string& name,
                      bool paired,
                      bool connected)
      : address_(address),
        bluetooth_class_(bluetooth_class),
        icon_(icon),
        name_(name),
        paired_(paired),
        connected_(connected) {
  }

  virtual ~BluetoothDeviceImpl() {
  }

  virtual const std::string& GetAddress() const {
    return address_;
  }

  virtual uint32 GetBluetoothClass() const {
    return bluetooth_class_;
  }

  virtual const std::string& GetIcon() const {
    return icon_;
  }

  virtual const std::string& GetName() const {
    return name_;
  }

  virtual bool IsPaired() const {
    return paired_;
  }

  virtual bool IsConnected() const {
    return connected_;
  }

 private:
  const std::string address_;
  uint32 bluetooth_class_;
  const std::string icon_;
  const std::string name_;
  bool paired_;
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceImpl);
};

BluetoothDevice::BluetoothDevice() {
}

BluetoothDevice::~BluetoothDevice() {
}

// static
BluetoothDevice* BluetoothDevice::Create(const std::string& address,
                                         uint32 bluetooth_class,
                                         const std::string& icon,
                                         const std::string& name,
                                         bool paired,
                                         bool connected) {
  return new BluetoothDeviceImpl(address, bluetooth_class, icon,
                                 name, paired, connected);
}

}  // namespace chromeos
