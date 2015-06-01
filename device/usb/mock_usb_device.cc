// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/mock_usb_device.h"

#include "base/strings/utf_string_conversions.h"

namespace device {

MockUsbDevice::MockUsbDevice(uint16 vendor_id, uint16 product_id)
    : MockUsbDevice(vendor_id, product_id, "", "", "") {
}

MockUsbDevice::MockUsbDevice(uint16 vendor_id,
                             uint16 product_id,
                             const std::string& manufacturer_string,
                             const std::string& product_string,
                             const std::string& serial_number)
    : UsbDevice(vendor_id,
                product_id,
                base::UTF8ToUTF16(manufacturer_string),
                base::UTF8ToUTF16(product_string),
                base::UTF8ToUTF16(serial_number)) {
}

MockUsbDevice::~MockUsbDevice() {
}

}  // namespace device
