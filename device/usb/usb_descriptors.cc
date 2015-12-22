// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_descriptors.h"

#include <stddef.h>

#include <algorithm>

namespace device {

namespace {
const uint8_t kStringDescriptorType = 0x03;
}

UsbEndpointDescriptor::UsbEndpointDescriptor()
    : address(0),
      direction(USB_DIRECTION_INBOUND),
      maximum_packet_size(0),
      synchronization_type(USB_SYNCHRONIZATION_NONE),
      transfer_type(USB_TRANSFER_CONTROL),
      usage_type(USB_USAGE_DATA),
      polling_interval(0) {
}

UsbEndpointDescriptor::~UsbEndpointDescriptor() {
}

UsbInterfaceDescriptor::UsbInterfaceDescriptor()
    : interface_number(0),
      alternate_setting(0),
      interface_class(0),
      interface_subclass(0),
      interface_protocol(0) {
}

UsbInterfaceDescriptor::~UsbInterfaceDescriptor() {
}

UsbConfigDescriptor::UsbConfigDescriptor()
    : configuration_value(0),
      self_powered(false),
      remote_wakeup(false),
      maximum_power(0) {
}

UsbConfigDescriptor::~UsbConfigDescriptor() {
}

bool ParseUsbStringDescriptor(const std::vector<uint8_t>& descriptor,
                              base::string16* output) {
  if (descriptor.size() < 2 || descriptor[1] != kStringDescriptorType) {
    return false;
  }
  // Let the device return a buffer larger than the actual string but prefer the
  // length reported inside the descriptor.
  size_t length = descriptor[0];
  length = std::min(length, descriptor.size());
  if (length < 2) {
    return false;
  } else if (length == 2) {
    // Special case to avoid indexing beyond the end of |descriptor|.
    *output = base::string16();
  } else {
    *output = base::string16(
        reinterpret_cast<const base::char16*>(&descriptor[2]), length / 2 - 1);
  }
  return true;
}

}  // namespace device
