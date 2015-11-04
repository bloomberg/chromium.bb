// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_BLUETOOTH_BLUETOOTH_SCAN_FILTER_H_
#define CONTENT_COMMON_BLUETOOTH_BLUETOOTH_SCAN_FILTER_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace content {

// Data sent over IPC representing a filter on Bluetooth scans, corresponding to
// blink::WebBluetoothScanFilter.
struct CONTENT_EXPORT BluetoothScanFilter {
  BluetoothScanFilter();
  ~BluetoothScanFilter();

  std::vector<device::BluetoothUUID> services;
  std::string name;
  std::string namePrefix;
};

}  // namespace content

#endif  // CONTENT_COMMON_BLUETOOTH_BLUETOOTH_DEVICE_H_
