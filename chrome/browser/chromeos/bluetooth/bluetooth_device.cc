// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"

#include "base/logging.h"
#include "base/values.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class BluetoothDeviceImpl : public BluetoothDevice {
 public:
  BluetoothDeviceImpl(const std::string& address,
                      uint32 bluetooth_class,
                      const std::string& icon,
                      const std::string& name,
                      bool paired,
                      const base::DictionaryValue& properties)
      : address_(address),
        bluetooth_class_(bluetooth_class),
        icon_(icon),
        name_(name),
        paired_(paired),
        dictionary_rep_(properties.DeepCopy()) {
    DCHECK(dictionary_rep_);
  }

  virtual ~BluetoothDeviceImpl() {
    delete dictionary_rep_;
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

  virtual const base::DictionaryValue& AsDictionary() const {
    DCHECK(dictionary_rep_);
    return *dictionary_rep_;
  }

 private:
  const std::string address_;
  uint32 bluetooth_class_;
  const std::string icon_;
  const std::string name_;
  bool paired_;
  base::DictionaryValue* dictionary_rep_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceImpl);
};

BluetoothDevice::BluetoothDevice() {
}

BluetoothDevice::~BluetoothDevice() {
}

// static
BluetoothDevice* BluetoothDevice::Create(
    const base::DictionaryValue& properties) {
  std::string address;
  uint32 bluetooth_class = 0;
  std::string icon;
  std::string name;
  bool paired = false;

  if (!properties.GetString(bluetooth_device::kAddressProperty, &address)) {
    LOG(ERROR) << "No address property";
    return NULL;  // mandatory
  }
  VLOG(1) << "address = " << address;

  // Note: DictionaryValue is Javascript-oriented and doesn't support unsigned
  // integers.
  if (!properties.GetInteger(bluetooth_device::kClassProperty,
                             reinterpret_cast<int*>(&bluetooth_class))) {
    LOG(ERROR) << "No bluetooth_class property";
    return NULL;  // mandatory
  }
  VLOG(1) << "bluetooth_class = 0x" << std::hex << bluetooth_class;

  if (!properties.GetString(bluetooth_device::kIconProperty, &icon)) {
    LOG(WARNING) << "No icon property";
  }
  VLOG(1) << "icon = " << icon;

  if (!properties.GetString(bluetooth_device::kNameProperty, &name)) {
    LOG(WARNING) << "No name property. Substituting MAC address.";
    // Populate name with mac address so that UI doesn't display a blank name.
    name = address;
  }
  VLOG(1) << "name = " << name;

  if (!properties.GetBoolean(bluetooth_device::kPairedProperty, &paired)) {
    LOG(ERROR) << "No paired property";
    return NULL;  // mandatory
  }

  return new BluetoothDeviceImpl(address, bluetooth_class, icon, name, paired,
                                 properties);
}

}  // namespace chromeos
