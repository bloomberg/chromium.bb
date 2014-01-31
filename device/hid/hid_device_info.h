// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_DEVICE_INFO_H_
#define DEVICE_HID_HID_DEVICE_INFO_H_

#include <string>

#include "base/basictypes.h"

namespace device {

enum HidBusType {
  kHIDBusTypeUSB = 0,
  kHIDBusTypeBluetooth = 1,
};

struct HidDeviceInfo {
  HidDeviceInfo();
  ~HidDeviceInfo();

  std::string device_id;

  HidBusType bus_type;
  uint16 vendor_id;
  uint16 product_id;

  size_t input_report_size;
  size_t output_report_size;
  size_t feature_report_size;

  uint16 usage_page;
  uint16 usage;
  bool has_report_id;

  std::string product_name;
  std::string serial_number;
};

}  // namespace device

#endif  // DEVICE_HID_HID_DEVICE_INFO_H_
