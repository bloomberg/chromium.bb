// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device.h"

#include "base/guid.h"
#include "device/usb/webusb_descriptors.h"

namespace device {

UsbDevice::UsbDevice(uint16_t vendor_id,
                     uint16_t product_id,
                     const base::string16& manufacturer_string,
                     const base::string16& product_string,
                     const base::string16& serial_number)
    : manufacturer_string_(manufacturer_string),
      product_string_(product_string),
      serial_number_(serial_number),
      guid_(base::GenerateGUID()),
      vendor_id_(vendor_id),
      product_id_(product_id) {}

UsbDevice::~UsbDevice() {
}

void UsbDevice::CheckUsbAccess(const ResultCallback& callback) {
  // By default assume that access to the device is allowed. This is implemented
  // on Chrome OS by checking with permission_broker.
  callback.Run(true);
}

}  // namespace device
