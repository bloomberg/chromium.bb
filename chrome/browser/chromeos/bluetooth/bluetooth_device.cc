// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/dbus/bluetooth_device_client.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "dbus/object_path.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

BluetoothDevice::BluetoothDevice() : bluetooth_class_(0),
                                     paired_(false),
                                     connected_(false) {
}

BluetoothDevice::~BluetoothDevice() {
}

void BluetoothDevice::SetObjectPath(const dbus::ObjectPath& object_path) {
  DCHECK(object_path_ == dbus::ObjectPath(""));
  object_path_ = object_path;
}

void BluetoothDevice::Update(
    const BluetoothDeviceClient::Properties* properties,
    bool update_state) {
  address_ = properties->address.value();
  name_ = properties->name.value();
  bluetooth_class_ = properties->bluetooth_class.value();

  if (update_state) {
    paired_ = properties->paired.value();
    connected_ = properties->connected.value();
  }
}

string16 BluetoothDevice::GetName() const {
  if (!name_.empty()) {
    return UTF8ToUTF16(name_);
  } else {
    return GetAddressWithLocalizedDeviceTypeName();
  }
}

BluetoothDevice::DeviceType BluetoothDevice::GetDeviceType() const {
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  switch ((bluetooth_class_ & 0x1f00) >> 8) {
    case 0x01:
      // Computer major device class.
      return DEVICE_COMPUTER;
    case 0x02:
      // Phone major device class.
      switch ((bluetooth_class_ & 0xfc) >> 2) {
        case 0x01:
        case 0x02:
        case 0x03:
          // Cellular, cordless and smart phones.
          return DEVICE_PHONE;
        case 0x04:
        case 0x05:
          // Modems: wired or voice gateway and common ISDN access.
          return DEVICE_MODEM;
      }
      break;
    case 0x05:
      // Peripheral major device class.
      switch ((bluetooth_class_ & 0xc0) >> 6) {
        case 0x00:
          // "Not a keyboard or pointing device."
          return DEVICE_PERIPHERAL;
        case 0x01:
          // Keyboard.
          return DEVICE_KEYBOARD;
        case 0x02:
          // Pointing device.
          switch ((bluetooth_class_ & 0x01e) >> 2) {
            case 0x05:
              // Digitizer tablet.
              return DEVICE_TABLET;
            default:
              // Mouse.
              return DEVICE_MOUSE;
          }
          break;
        case 0x03:
          // Combo device.
          return DEVICE_KEYBOARD_MOUSE_COMBO;
      }
      break;
  }

  return DEVICE_UNKNOWN;
}

bool BluetoothDevice::IsSupported() const {
  DeviceType device_type = GetDeviceType();
  return (device_type == DEVICE_COMPUTER ||
          device_type == DEVICE_PHONE ||
          device_type == DEVICE_KEYBOARD ||
          device_type == DEVICE_MOUSE ||
          device_type == DEVICE_TABLET ||
          device_type == DEVICE_KEYBOARD_MOUSE_COMBO);
}

string16 BluetoothDevice::GetAddressWithLocalizedDeviceTypeName() const {
  string16 address = UTF8ToUTF16(address_);
  DeviceType device_type = GetDeviceType();
  switch (device_type) {
    case DEVICE_COMPUTER:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_COMPUTER,
                                        address);
    case DEVICE_PHONE:
          return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_PHONE,
                                            address);
    case DEVICE_MODEM:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MODEM,
                                        address);
    case DEVICE_KEYBOARD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_KEYBOARD,
                                        address);
    case DEVICE_MOUSE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MOUSE,
                                        address);
    case DEVICE_TABLET:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_TABLET,
                                        address);
    case DEVICE_KEYBOARD_MOUSE_COMBO:
      return l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_DEVICE_KEYBOARD_MOUSE_COMBO, address);
    default:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_UNKNOWN, address);
  }
}

bool BluetoothDevice::WasDiscovered() const {
  return object_path_.value().empty();
}

bool BluetoothDevice::IsConnected() const {
  // TODO(keybuk): examine protocol-specific connected state, such as Input
  return connected_;
}


// static
BluetoothDevice* BluetoothDevice::CreateBound(
    const dbus::ObjectPath& object_path,
    const BluetoothDeviceClient::Properties* properties) {
  BluetoothDevice* device = new BluetoothDevice;
  device->SetObjectPath(object_path);
  device->Update(properties, true);
  return device;
}

// static
BluetoothDevice* BluetoothDevice::CreateUnbound(
    const BluetoothDeviceClient::Properties* properties) {
  BluetoothDevice* device = new BluetoothDevice;
  device->Update(properties, false);
  return device;
}

}  // namespace chromeos
