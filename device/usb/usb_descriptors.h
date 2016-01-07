// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DESCRIPTORS_H_
#define DEVICE_USB_USB_DESCRIPTORS_H_

#include <stdint.h>
#include <vector>

#include "base/strings/string16.h"

namespace device {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.device.usb
enum UsbTransferType {
  USB_TRANSFER_CONTROL = 0,
  USB_TRANSFER_ISOCHRONOUS,
  USB_TRANSFER_BULK,
  USB_TRANSFER_INTERRUPT,
};

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.device.usb
enum UsbEndpointDirection {
  USB_DIRECTION_INBOUND = 0,
  USB_DIRECTION_OUTBOUND,
};

enum UsbSynchronizationType {
  USB_SYNCHRONIZATION_NONE = 0,
  USB_SYNCHRONIZATION_ASYNCHRONOUS,
  USB_SYNCHRONIZATION_ADAPTIVE,
  USB_SYNCHRONIZATION_SYNCHRONOUS,
};

enum UsbUsageType {
  USB_USAGE_DATA = 0,
  USB_USAGE_FEEDBACK,
  USB_USAGE_EXPLICIT_FEEDBACK
};

struct UsbEndpointDescriptor {
  UsbEndpointDescriptor();
  ~UsbEndpointDescriptor();

  uint8_t address;
  UsbEndpointDirection direction;
  uint16_t maximum_packet_size;
  UsbSynchronizationType synchronization_type;
  UsbTransferType transfer_type;
  UsbUsageType usage_type;
  uint16_t polling_interval;
  std::vector<uint8_t> extra_data;
};

struct UsbInterfaceDescriptor {
  UsbInterfaceDescriptor();
  ~UsbInterfaceDescriptor();

  uint8_t interface_number;
  uint8_t alternate_setting;
  uint8_t interface_class;
  uint8_t interface_subclass;
  uint8_t interface_protocol;
  std::vector<UsbEndpointDescriptor> endpoints;
  std::vector<uint8_t> extra_data;
};

struct UsbConfigDescriptor {
  UsbConfigDescriptor();
  ~UsbConfigDescriptor();

  uint8_t configuration_value;
  bool self_powered;
  bool remote_wakeup;
  uint16_t maximum_power;
  std::vector<UsbInterfaceDescriptor> interfaces;
  std::vector<uint8_t> extra_data;
};

bool ParseUsbStringDescriptor(const std::vector<uint8_t>& descriptor,
                              base::string16* output);

}  // namespace device

#endif  // DEVICE_USB_USB_DESCRIPTORS_H_
