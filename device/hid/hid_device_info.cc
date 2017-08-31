// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_device_info.h"

#include "base/guid.h"
#include "build/build_config.h"
#include "device/hid/hid_report_descriptor.h"

namespace device {

HidDeviceInfo::HidDeviceInfo(const HidPlatformDeviceId& platform_device_id,
                             uint16_t vendor_id,
                             uint16_t product_id,
                             const std::string& product_name,
                             const std::string& serial_number,
                             device::mojom::HidBusType bus_type,
                             const std::vector<uint8_t> report_descriptor,
                             std::string device_node)
    : platform_device_id_(platform_device_id) {
  std::vector<HidCollectionInfo> collections;
  bool has_report_id;
  size_t max_input_report_size;
  size_t max_output_report_size;
  size_t max_feature_report_size;

  HidReportDescriptor descriptor_parser(report_descriptor);
  descriptor_parser.GetDetails(&collections, &has_report_id,
                               &max_input_report_size, &max_output_report_size,
                               &max_feature_report_size);

  device_ = device::mojom::HidDeviceInfo::New(
      base::GenerateGUID(), vendor_id, product_id, product_name, serial_number,
      bus_type, report_descriptor, collections, has_report_id,
      max_input_report_size, max_output_report_size, max_feature_report_size,
      device_node);
}

HidDeviceInfo::HidDeviceInfo(const HidPlatformDeviceId& platform_device_id,
                             uint16_t vendor_id,
                             uint16_t product_id,
                             const std::string& product_name,
                             const std::string& serial_number,
                             device::mojom::HidBusType bus_type,
                             const HidCollectionInfo& collection,
                             size_t max_input_report_size,
                             size_t max_output_report_size,
                             size_t max_feature_report_size)
    : platform_device_id_(platform_device_id) {
  std::vector<HidCollectionInfo> collections;
  collections.push_back(collection);
  bool has_report_id = !collection.report_ids.empty();

  std::vector<uint8_t> report_descriptor;
  device_ = device::mojom::HidDeviceInfo::New(
      base::GenerateGUID(), vendor_id, product_id, product_name, serial_number,
      bus_type, report_descriptor, collections, has_report_id,
      max_input_report_size, max_output_report_size, max_feature_report_size,
      "");
}

HidDeviceInfo::~HidDeviceInfo() {}

}  // namespace device
