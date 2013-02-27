// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/test_storage_monitor.h"

namespace chrome {
namespace test {

TestStorageMonitor::TestStorageMonitor()
    : StorageMonitor() {}

TestStorageMonitor::~TestStorageMonitor() {}

TestStorageMonitor*
TestStorageMonitor::CreateForBrowserTests() {
  StorageMonitor::RemoveSingletonForTesting();
  return new TestStorageMonitor();
}

bool TestStorageMonitor::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  return false;
}

uint64 TestStorageMonitor::GetStorageSize(
    const base::FilePath::StringType& location) const {
  return 0;
}

#if defined(OS_WIN)
bool TestStorageMonitor::GetMTPStorageInfoFromDeviceId(
    const std::string& storage_device_id,
    string16* device_location,
    string16* storage_object_id) const {
  return false;
}
#endif

StorageMonitor::Receiver* TestStorageMonitor::receiver() const {
  return StorageMonitor::receiver();
}

void TestStorageMonitor::EjectDevice(
    const std::string& device_id,
    base::Callback<void(EjectStatus)> callback) {
  ejected_device_ = device_id;
  callback.Run(EJECT_OK);
}

}  // namespace test
}  // namespace chrome
