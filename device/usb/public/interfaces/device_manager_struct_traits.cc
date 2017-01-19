// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/public/interfaces/device_manager_struct_traits.h"

namespace mojo {

// static
bool StructTraits<device::usb::DeviceFilterDataView, device::UsbDeviceFilter>::
    Read(device::usb::DeviceFilterDataView input,
         device::UsbDeviceFilter* output) {
  if (input.has_vendor_id())
    output->vendor_id = input.vendor_id();
  if (input.has_product_id())
    output->product_id = input.product_id();
  if (input.has_class_code())
    output->interface_class = input.class_code();
  if (input.has_subclass_code())
    output->interface_subclass = input.subclass_code();
  if (input.has_protocol_code())
    output->interface_protocol = input.protocol_code();
  return input.ReadSerialNumber(&output->serial_number);
}

}  // namespace mojo
