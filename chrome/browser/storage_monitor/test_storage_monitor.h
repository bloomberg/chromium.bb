// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_
#define CHROME_BROWSER_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_

#include "chrome/browser/storage_monitor/storage_monitor.h"

namespace chrome {
namespace test {

class TestStorageMonitor : public chrome::StorageMonitor {
 public:
  TestStorageMonitor();
  virtual ~TestStorageMonitor();

  // Will create a new testing implementation for browser tests,
  // taking care to deal with the existing singleton correctly.
  static TestStorageMonitor* CreateForBrowserTests();

  virtual bool GetStorageInfoForPath(
      const base::FilePath& path,
      StorageInfo* device_info) const OVERRIDE;

  virtual uint64 GetStorageSize(
      const base::FilePath::StringType& location) const OVERRIDE;

#if defined(OS_WIN)
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const OVERRIDE;
#endif

  virtual Receiver* receiver() const OVERRIDE;

  virtual void EjectDevice(
      const std::string& device_id,
      base::Callback<void(StorageMonitor::EjectStatus)> callback)
      OVERRIDE;

  const std::string& ejected_device() const { return ejected_device_; }

 private:
  std::string ejected_device_;
};

}  // namespace test
}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_
