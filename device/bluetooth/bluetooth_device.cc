// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "grit/device_bluetooth_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {

BluetoothDevice::BluetoothDevice() {
}

BluetoothDevice::~BluetoothDevice() {
}

base::string16 BluetoothDevice::GetName() const {
  std::string name = GetDeviceName();
  if (!name.empty()) {
    return base::UTF8ToUTF16(name);
  } else {
    return GetAddressWithLocalizedDeviceTypeName();
  }
}

base::string16 BluetoothDevice::GetAddressWithLocalizedDeviceTypeName() const {
  base::string16 address_utf16 = base::UTF8ToUTF16(GetAddress());
  BluetoothDevice::DeviceType device_type = GetDeviceType();
  switch (device_type) {
    case DEVICE_COMPUTER:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_COMPUTER,
                                        address_utf16);
    case DEVICE_PHONE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_PHONE,
                                        address_utf16);
    case DEVICE_MODEM:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MODEM,
                                        address_utf16);
    case DEVICE_AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_AUDIO,
                                        address_utf16);
    case DEVICE_CAR_AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CAR_AUDIO,
                                        address_utf16);
    case DEVICE_VIDEO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_VIDEO,
                                        address_utf16);
    case DEVICE_JOYSTICK:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_JOYSTICK,
                                        address_utf16);
    case DEVICE_GAMEPAD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_GAMEPAD,
                                        address_utf16);
    case DEVICE_KEYBOARD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_KEYBOARD,
                                        address_utf16);
    case DEVICE_MOUSE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MOUSE,
                                        address_utf16);
    case DEVICE_TABLET:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_TABLET,
                                        address_utf16);
    case DEVICE_KEYBOARD_MOUSE_COMBO:
      return l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_DEVICE_KEYBOARD_MOUSE_COMBO, address_utf16);
    default:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_UNKNOWN,
                                        address_utf16);
  }
}

BluetoothDevice::DeviceType BluetoothDevice::GetDeviceType() const {
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32 bluetooth_class = GetBluetoothClass();
  switch ((bluetooth_class & 0x1f00) >> 8) {
    case 0x01:
      // Computer major device class.
      return DEVICE_COMPUTER;
    case 0x02:
      // Phone major device class.
      switch ((bluetooth_class & 0xfc) >> 2) {
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
      switch ((bluetooth_class & 0xfc) >> 2) {
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
      switch ((bluetooth_class & 0xc0) >> 6) {
        case 0x00:
          // "Not a keyboard or pointing device."
          switch ((bluetooth_class & 0x01e) >> 2) {
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
          switch ((bluetooth_class & 0x01e) >> 2) {
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

bool BluetoothDevice::IsPairable() const {
  DeviceType type = GetDeviceType();

  // Get the vendor part of the address: "00:11:22" for "00:11:22:33:44:55"
  std::string vendor = GetAddress().substr(0, 8);

  // Verbatim "Bluetooth Mouse", model 96674
  if ((type == DEVICE_MOUSE && vendor == "00:12:A1") ||
  // Microsoft "Microsoft Bluetooth Notebook Mouse 5000", model X807028-001
      (type == DEVICE_MOUSE && vendor == "7C:ED:8D"))
      return false;
  // TODO: Move this database into a config file.

  return true;
}

}  // namespace device
