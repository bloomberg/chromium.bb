// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_

#include "base/file_path.h"
#include "base/system_monitor/system_monitor.h"

namespace chrome {

// Base class for platform-specific instances watching for removable storage
// attachments/detachments.
class RemovableStorageNotifications {
 public:
  virtual ~RemovableStorageNotifications() {}

  // Returns a pointer to an object owned by the BrowserMainParts, with lifetime
  // somewhat shorter than a process Singleton.
  static RemovableStorageNotifications* GetInstance();

  // Finds the device that contains |path| and populates |device_info|.
  // Should be able to handle any path on the local system, not just removable
  // storage. Returns false if unable to find the device.
  virtual bool GetDeviceInfoForPath(
      const FilePath& path,
      base::SystemMonitor::RemovableStorageInfo* device_info) const = 0;

  // Returns the storage size of the device present at |location|. If the
  // device information is unavailable, returns zero.
  virtual uint64 GetStorageSize(const std::string& location) const = 0;
};

} // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_STORAGE_NOTIFICATIONS_H_
