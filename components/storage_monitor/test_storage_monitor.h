// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_
#define COMPONENTS_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_

#include <string>

#include "components/storage_monitor/storage_monitor.h"

namespace storage_monitor {

class TestStorageMonitor : public StorageMonitor {
 public:
  TestStorageMonitor();
  virtual ~TestStorageMonitor();

  virtual void Init() OVERRIDE;

  void MarkInitialized();

  // Create and initialize a new TestStorageMonitor and install it
  // as the StorageMonitor singleton. If there is a StorageMonitor instance
  // already in place, NULL is returned, otherwise the TestStorageMonitor
  // instance is returned. Use |Destroy| to delete the singleton.
  static TestStorageMonitor* CreateAndInstall();

  // Create and initialize a new TestStorageMonitor and install it
  // as the StorageMonitor singleton. TestStorageMonitor is returned for
  // convenience. Use |Destroy| to delete the singleton.
  static TestStorageMonitor* CreateForBrowserTests();

  // Synchronously initialize the current storage monitor.
  static void SyncInitialize();

  virtual bool GetStorageInfoForPath(
      const base::FilePath& path,
      StorageInfo* device_info) const OVERRIDE;

#if defined(OS_WIN)
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      base::string16* device_location,
      base::string16* storage_object_id) const OVERRIDE;
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

  bool init_called() const { return init_called_; }

 private:
  // Whether TestStorageMonitor::Init() has been called for not.
  bool init_called_;

  // The last device to be ejected.
  std::string ejected_device_;

#if defined(OS_LINUX)
  scoped_ptr<device::MediaTransferProtocolManager>
      media_transfer_protocol_manager_;
#endif
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_
