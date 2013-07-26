// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_storage/test_storage_info_provider.h"

#include "base/strings/utf_string_conversions.h"

namespace extensions {

using api::system_storage::ParseStorageUnitType;
using api::system_storage::StorageUnitInfo;
using systeminfo::kStorageTypeFixed;
using systeminfo::kStorageTypeRemovable;
using systeminfo::kStorageTypeUnknown;

// Watching interval for testing.
const size_t kTestingIntervalMS = 10;

TestStorageInfoProvider::TestStorageInfoProvider(
    const struct TestStorageUnitInfo* testing_data, size_t n)
      : testing_data_(testing_data, testing_data + n) {
    SetWatchingIntervalForTesting(kTestingIntervalMS);
}

TestStorageInfoProvider::~TestStorageInfoProvider() {
}

// static
chrome::StorageInfo TestStorageInfoProvider::BuildStorageInfo(
    const TestStorageUnitInfo& unit) {
  chrome::StorageInfo info(
      unit.device_id,
      UTF8ToUTF16(unit.name),
      base::FilePath::StringType(), /* no location */
      string16(), /* no storage label */
      string16(), /* no storage vendor */
      string16(), /* no storage model */
      unit.capacity);
  return info;
}

void TestStorageInfoProvider::GetAllStoragesIntoInfoList() {
  info_.clear();
  for (size_t i = 0; i < testing_data_.size(); ++i) {
    linked_ptr<StorageUnitInfo> unit(new StorageUnitInfo());
    unit->id = testing_data_[i].transient_id;
    unit->name = testing_data_[i].name;
    unit->type = ParseStorageUnitType(testing_data_[i].type);
    unit->capacity = testing_data_[i].capacity;
    info_.push_back(unit);
  }
}

std::vector<chrome::StorageInfo>
TestStorageInfoProvider::GetAllStorages() const {
  std::vector<chrome::StorageInfo> results;
  for (size_t i = 0; i < testing_data_.size(); ++i)
    results.push_back(BuildStorageInfo(testing_data_[i]));

  return results;
}

int64 TestStorageInfoProvider::GetStorageFreeSpaceFromTransientId(
    const std::string& transient_id) {
  int64 available_capacity = -1;
  for (size_t i = 0; i < testing_data_.size(); ++i) {
    if (testing_data_[i].transient_id == transient_id) {
      available_capacity = testing_data_[i].available_capacity;
      // We simulate free space change by increasing the |available_capacity|
      // with a fixed change step.
      testing_data_[i].available_capacity += testing_data_[i].change_step;
      break;
    }
  }
  return available_capacity;
}

std::string TestStorageInfoProvider::GetTransientIdForDeviceId(
    const std::string& device_id) const {
  for (size_t i = 0; i < testing_data_.size(); ++i) {
    if (testing_data_[i].device_id == device_id)
      return testing_data_[i].transient_id;
  }
  return std::string();
}

std::string TestStorageInfoProvider::GetDeviceIdForTransientId(
    const std::string& transient_id) const {
  for (size_t i = 0; i < testing_data_.size(); ++i) {
    if (testing_data_[i].transient_id == transient_id)
      return testing_data_[i].device_id;
  }
  return std::string();
}

}  // namespace extensions
