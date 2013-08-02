// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_storage/test_storage_info_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"

namespace extensions {
namespace test {

using api::system_storage::ParseStorageUnitType;
using api::system_storage::StorageUnitInfo;

// Watching interval for testing.
const size_t kTestingIntervalMS = 10;

const struct TestStorageUnitInfo kRemovableStorageData = {
    "dcim:device:001", "/media/usb1", 4098, 1000, 1
};

chrome::StorageInfo BuildStorageInfoFromTestStorageUnitInfo(
    const TestStorageUnitInfo& unit) {
  return chrome::StorageInfo(
      unit.device_id,
      UTF8ToUTF16(unit.name),
      base::FilePath::StringType(), /* no location */
      string16(), /* no storage label */
      string16(), /* no storage vendor */
      string16(), /* no storage model */
      unit.capacity);
}

TestStorageInfoProvider::TestStorageInfoProvider(
    const struct TestStorageUnitInfo* testing_data, size_t n)
        : StorageInfoProvider(kTestingIntervalMS),
          testing_data_(testing_data, testing_data + n) {
}

TestStorageInfoProvider::~TestStorageInfoProvider() {
}

int64 TestStorageInfoProvider::GetStorageFreeSpaceFromTransientId(
    const std::string& transient_id) {
  int64 available_capacity = -1;
  std::string device_id =
      chrome::StorageMonitor::GetInstance()->GetDeviceIdForTransientId(
          transient_id);
  for (size_t i = 0; i < testing_data_.size(); ++i) {
    if (testing_data_[i].device_id == device_id) {
      available_capacity = testing_data_[i].available_capacity;
      // We simulate free space change by increasing the |available_capacity|
      // with a fixed change step.
      testing_data_[i].available_capacity += testing_data_[i].change_step;
      break;
    }
  }
  return available_capacity;
}

}  // namespace test
}  // namespace extensions
