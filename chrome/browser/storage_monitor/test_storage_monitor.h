// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_
#define CHROME_BROWSER_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_

#include <string>

#include "chrome/browser/storage_monitor/storage_monitor.h"

namespace chrome {
namespace test {

class TestStorageMonitor : public chrome::StorageMonitor {
 public:
  TestStorageMonitor();
  virtual ~TestStorageMonitor();

  virtual void Init() OVERRIDE;

  void MarkInitialized();

  // Create and initialize a new TestStorageMonitor and install it
  // in the TestingBrowserProcess.
  static TestStorageMonitor* CreateAndInstall();

  // Create and initialize a new TestStorageMonitor, and install it
  // in the BrowserProcessImpl. (Browser tests use the production browser
  // process implementation.)
  static TestStorageMonitor* CreateForBrowserTests();

  // Remove the singleton StorageMonitor from the TestingBrowserProcess.
  static void RemoveSingleton();

  // Synchronously initialize the current storage monitor.
  static void SyncInitialize();

  virtual bool GetStorageInfoForPath(
      const base::FilePath& path,
      StorageInfo* device_info) const OVERRIDE;

#if defined(OS_WIN)
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const OVERRIDE;
#endif

#if defined(OS_LINUX)
  virtual device::MediaTransferProtocolManager*
      media_transfer_protocol_manager() OVERRIDE;
#endif

  virtual Receiver* receiver() const OVERRIDE;

  virtual void EjectDevice(
      const std::string& device_id,
      base::Callback<void(StorageMonitor::EjectStatus)> callback)
      OVERRIDE;

  const std::string& ejected_device() const { return ejected_device_; }

  bool init_called_;

 private:
  std::string ejected_device_;

#if defined(OS_LINUX)
  scoped_ptr<device::MediaTransferProtocolManager>
      media_transfer_protocol_manager_;
#endif
};

}  // namespace test
}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_
