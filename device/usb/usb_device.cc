// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device.h"

namespace device {

UsbDevice::UsbDevice(uint16 vendor_id, uint16 product_id, uint32 unique_id)
    : vendor_id_(vendor_id), product_id_(product_id), unique_id_(unique_id) {
}

UsbDevice::~UsbDevice() {
}

void UsbDevice::CheckUsbAccess(const ResultCallback& callback) {
  callback.Run(true);
}

// Like CheckUsbAccess but actually changes the ownership of the device node.
void UsbDevice::RequestUsbAccess(int interface_id,
                                 const ResultCallback& callback) {
  callback.Run(true);
}

}  // namespace device
