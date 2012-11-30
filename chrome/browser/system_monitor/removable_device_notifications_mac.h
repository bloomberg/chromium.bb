// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_MAC_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_MAC_H_

#include <DiskArbitration/DiskArbitration.h>
#include <map>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/system_monitor/disk_info_mac.h"

namespace chrome {

class RemovableDeviceNotificationsMac;
typedef RemovableDeviceNotificationsMac RemovableDeviceNotifications;

// This class posts notifications to base::SystemMonitor when a new disk
// is attached, removed, or changed.
class RemovableDeviceNotificationsMac :
    public base::RefCountedThreadSafe<RemovableDeviceNotificationsMac> {
 public:
  enum UpdateType {
    UPDATE_DEVICE_ADDED,
    UPDATE_DEVICE_CHANGED,
    UPDATE_DEVICE_REMOVED,
  };

  // Should only be called by browser start up code.  Use GetInstance() instead.
  RemovableDeviceNotificationsMac();

  static RemovableDeviceNotificationsMac* GetInstance();

  void UpdateDisk(const DiskInfoMac& info, UpdateType update_type);

  bool GetDeviceInfoForPath(
      const FilePath& path,
      base::SystemMonitor::RemovableStorageInfo* device_info) const;

  // Returns the storage size of the device present at |location|. If the
  // device information is unavailable, returns zero. |location| must be a
  // top-level mount point.
  uint64 GetStorageSize(const std::string& location) const;

 private:
  friend class base::RefCountedThreadSafe<RemovableDeviceNotificationsMac>;
  virtual ~RemovableDeviceNotificationsMac();

  static void DiskAppearedCallback(DADiskRef disk, void* context);
  static void DiskDisappearedCallback(DADiskRef disk, void* context);
  static void DiskDescriptionChangedCallback(DADiskRef disk,
                                             CFArrayRef keys,
                                             void *context);

  bool ShouldPostNotificationForDisk(const DiskInfoMac& info) const;
  bool FindDiskWithMountPoint(const FilePath& mount_point,
                              DiskInfoMac* info) const;

  base::mac::ScopedCFTypeRef<DASessionRef> session_;
  // Maps disk bsd names to disk info objects. This map tracks all mountable
  // devices on the system though only notifications for removable devices are
  // posted.
  std::map<std::string, DiskInfoMac> disk_info_map_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationsMac);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_MAC_H_
