// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_PUBLIC_INTERFACES_DEVICE_MANAGER_STRUCT_TRAITS_H_
#define DEVICE_USB_PUBLIC_INTERFACES_DEVICE_MANAGER_STRUCT_TRAITS_H_

#include "device/usb/public/interfaces/device_manager.mojom.h"
#include "device/usb/usb_device_filter.h"

namespace mojo {

template <>
struct StructTraits<device::usb::DeviceFilterDataView,
                    device::UsbDeviceFilter> {
  static bool has_vendor_id(const device::UsbDeviceFilter& filter) {
    return filter.vendor_id.has_value();
  }

  static uint16_t vendor_id(const device::UsbDeviceFilter& filter) {
    return filter.vendor_id.value_or(0);
  }

  static bool has_product_id(const device::UsbDeviceFilter& filter) {
    return filter.product_id.has_value();
  }

  static uint16_t product_id(const device::UsbDeviceFilter& filter) {
    return filter.product_id.value_or(0);
  }

  static bool has_class_code(const device::UsbDeviceFilter& filter) {
    return filter.interface_class.has_value();
  }

  static uint8_t class_code(const device::UsbDeviceFilter& filter) {
    return filter.interface_class.value_or(0);
  }

  static bool has_subclass_code(const device::UsbDeviceFilter& filter) {
    return filter.interface_subclass.has_value();
  }

  static uint8_t subclass_code(const device::UsbDeviceFilter& filter) {
    return filter.interface_subclass.value_or(0);
  }

  static bool has_protocol_code(const device::UsbDeviceFilter& filter) {
    return filter.interface_protocol.has_value();
  }

  static uint8_t protocol_code(const device::UsbDeviceFilter& filter) {
    return filter.interface_protocol.value_or(0);
  }

  static const base::Optional<std::string>& serial_number(
      const device::UsbDeviceFilter& filter) {
    return filter.serial_number;
  }

  static bool Read(device::usb::DeviceFilterDataView input,
                   device::UsbDeviceFilter* output);
};

}  // namespace mojo

#endif  // DEVICE_USB_PUBLIC_INTERFACES_DEVICE_MANAGER_STRUCT_TRAITS_H_
