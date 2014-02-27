// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_DEVICE_INFO_H_
#define DEVICE_HID_HID_DEVICE_INFO_H_

#include <stdint.h>

#include <string>

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <IOKit/hid/IOHIDDevice.h>
#endif

namespace device {

enum HidBusType {
  kHIDBusTypeUSB = 0,
  kHIDBusTypeBluetooth = 1,
};

#if defined(OS_MACOSX)
typedef IOHIDDeviceRef HidDeviceId;
const HidDeviceId kInvalidHidDeviceId = NULL;
#else
typedef std::string HidDeviceId;
extern const char kInvalidHidDeviceId[];
#endif

struct HidDeviceInfo {
  HidDeviceInfo();
  ~HidDeviceInfo();

  HidDeviceId device_id;

  HidBusType bus_type;
  uint16_t vendor_id;
  uint16_t product_id;

  int input_report_size;
  int output_report_size;
  int feature_report_size;

  uint16_t usage_page;
  uint16_t usage;
  bool has_report_id;

  std::string product_name;
  std::string serial_number;
};

}  // namespace device

#endif  // DEVICE_HID_HID_DEVICE_INFO_H_
