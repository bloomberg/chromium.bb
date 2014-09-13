// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_descriptors.h"

namespace device {

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

}  // namespace device
