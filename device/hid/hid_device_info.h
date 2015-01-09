// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_HID_HID_DEVICE_INFO_H_
#define DEVICE_HID_HID_DEVICE_INFO_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "device/hid/hid_collection_info.h"

namespace device {

enum HidBusType {
  kHIDBusTypeUSB = 0,
  kHIDBusTypeBluetooth = 1,
};

typedef std::string HidDeviceId;
extern const char kInvalidHidDeviceId[];

class HidDeviceInfo : public base::RefCountedThreadSafe<HidDeviceInfo> {
 public:
  HidDeviceInfo();

  // Device identification.
  const HidDeviceId& device_id() const { return device_id_; }
  uint16_t vendor_id() const { return vendor_id_; }
  uint16_t product_id() const { return product_id_; }
  const std::string& product_name() const { return product_name_; }
  const std::string& serial_number() const { return serial_number_; }
  HidBusType bus_type() const { return bus_type_; }

  // Top-Level Collections information.
  const std::vector<HidCollectionInfo>& collections() const {
    return collections_;
  }
  bool has_report_id() const { return has_report_id_; };
  size_t max_input_report_size() const { return max_input_report_size_; }
  size_t max_output_report_size() const { return max_output_report_size_; }
  size_t max_feature_report_size() const { return max_feature_report_size_; }

#if defined(OS_LINUX)
  const std::string& device_node() const { return device_node_; }
#endif

 private:
  friend class base::RefCountedThreadSafe<HidDeviceInfo>;

  // TODO(reillyg): Define public constructors that make some of these
  // declarations unnecessary.
  friend class HidServiceLinux;
  friend class HidServiceMac;
  friend class HidServiceWin;
  friend class MockHidService;
  friend class HidFilterTest;

  ~HidDeviceInfo();

  // Device identification.
  HidDeviceId device_id_;
  uint16_t vendor_id_;
  uint16_t product_id_;
  std::string product_name_;
  std::string serial_number_;
  HidBusType bus_type_;

  // Top-Level Collections information.
  std::vector<HidCollectionInfo> collections_;
  bool has_report_id_;
  size_t max_input_report_size_;
  size_t max_output_report_size_;
  size_t max_feature_report_size_;

#if defined(OS_LINUX)
  std::string device_node_;
#endif

  DISALLOW_COPY_AND_ASSIGN(HidDeviceInfo);
};

}  // namespace device

#endif  // DEVICE_HID_HID_DEVICE_INFO_H_
