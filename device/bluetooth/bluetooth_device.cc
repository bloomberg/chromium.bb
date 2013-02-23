// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device.h"

#include "base/utf_string_conversions.h"
#include "grit/device_bluetooth_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {

BluetoothDevice::BluetoothDevice()
    : bluetooth_class_(0),
      visible_(false),
      bonded_(false),
      connected_(false),
      connectable_(true) {
}

BluetoothDevice::~BluetoothDevice() {
}

const std::string& BluetoothDevice::address() const {
  return address_;
}

string16 BluetoothDevice::GetName() const {
  if (!name_.empty()) {
    return UTF8ToUTF16(name_);
  } else {
    return GetAddressWithLocalizedDeviceTypeName();
  }
}

string16 BluetoothDevice::GetAddressWithLocalizedDeviceTypeName() const {
  string16 address = UTF8ToUTF16(address_);
  BluetoothDevice::DeviceType device_type = GetDeviceType();
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
    case DEVICE_AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_AUDIO,
                                        address);
    case DEVICE_CAR_AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CAR_AUDIO,
                                        address);
    case DEVICE_VIDEO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_VIDEO,
                                        address);
    case DEVICE_JOYSTICK:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_JOYSTICK,
                                        address);
    case DEVICE_GAMEPAD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_GAMEPAD,
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
    case 0x04:
      // Audio major device class.
      switch ((bluetooth_class_ & 0xfc) >> 2) {
        case 0x08:
          // Car audio.
          return DEVICE_CAR_AUDIO;
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x010:
          // Video devices.
          return DEVICE_VIDEO;
        default:
          return DEVICE_AUDIO;
      }
      break;
    case 0x05:
      // Peripheral major device class.
      switch ((bluetooth_class_ & 0xc0) >> 6) {
        case 0x00:
          // "Not a keyboard or pointing device."
          switch ((bluetooth_class_ & 0x01e) >> 2) {
            case 0x01:
              // Joystick.
              return DEVICE_JOYSTICK;
            case 0x02:
              // Gamepad.
              return DEVICE_GAMEPAD;
            default:
              return DEVICE_PERIPHERAL;
          }
          break;
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

bool BluetoothDevice::IsVisible() const {
  return visible_;
}

bool BluetoothDevice::IsBonded() const {
  return bonded_;
}

bool BluetoothDevice::IsConnected() const {
  return connected_;
}

bool BluetoothDevice::IsConnectable() const {
  return connectable_;
}

}  // namespace device
