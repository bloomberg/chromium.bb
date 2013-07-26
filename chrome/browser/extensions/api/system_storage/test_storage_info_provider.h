// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_TEST_STORAGE_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_TEST_STORAGE_INFO_PROVIDER_H_

#include <vector>

#include "chrome/browser/extensions/api/system_storage/storage_info_provider.h"
#include "chrome/browser/storage_monitor/storage_info.h"

namespace extensions {

struct TestStorageUnitInfo {
  const char* device_id;
  const char* transient_id;
  const char* name;
  const char* type;
  // Total amount of the storage device space, in bytes.
  double capacity;
  // The available amount of the storage space, in bytes.
  double available_capacity;
  // The change step of available capacity for simulating the free space change.
  // Each querying operation will increase the |available_capacity| with this
  // value.
  int change_step;
};

// StorageInfoProvider for unit_tests and browser_tests.
// Override related methods to avoid calling out to the actual system.
//
// TODO(Haojian) : Improve StorageInfoProvider test. Create a interface that
// represents system storage functions, with a googlemock implementation.
class TestStorageInfoProvider : public extensions::StorageInfoProvider {
 public:
  TestStorageInfoProvider(const struct TestStorageUnitInfo* testing_data,
                          size_t n);

  static chrome::StorageInfo BuildStorageInfo(const TestStorageUnitInfo& unit);

  virtual std::string GetTransientIdForDeviceId(
      const std::string& device_id) const OVERRIDE;
  virtual std::string GetDeviceIdForTransientId(
      const std::string& transient_id) const OVERRIDE;

 protected:
  virtual ~TestStorageInfoProvider();

  // StorageInfoProvider implementations.
  virtual void GetAllStoragesIntoInfoList() OVERRIDE;
  virtual std::vector<chrome::StorageInfo> GetAllStorages() const OVERRIDE;
  virtual int64 GetStorageFreeSpaceFromTransientId(
      const std::string& transient_id) OVERRIDE;

  std::vector<struct TestStorageUnitInfo> testing_data_;
};

}  // namespace extensions
#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_STORAGE_TEST_STORAGE_INFO_PROVIDER_H_

