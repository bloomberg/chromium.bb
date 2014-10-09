// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_DEVICE_INFO_H_
#define DEVICE_HID_HID_DEVICE_INFO_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "device/hid/hid_collection_info.h"

namespace device {

enum HidBusType {
  kHIDBusTypeUSB = 0,
  kHIDBusTypeBluetooth = 1,
};

typedef std::string HidDeviceId;
extern const char kInvalidHidDeviceId[];

struct HidDeviceInfo {
  HidDeviceInfo();
  ~HidDeviceInfo();

  // Device identification.
  HidDeviceId device_id;
  uint16_t vendor_id;
  uint16_t product_id;
  std::string product_name;
  std::string serial_number;
  HidBusType bus_type;

  // Top-Level Collections information.
  std::vector<HidCollectionInfo> collections;
  bool has_report_id;
  uint16_t max_input_report_size;
  uint16_t max_output_report_size;
  uint16_t max_feature_report_size;
};

}  // namespace device

#endif  // DEVICE_HID_HID_DEVICE_INFO_H_
