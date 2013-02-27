// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_MAC_H_
#define CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_MAC_H_

#include <DiskArbitration/DiskArbitration.h>
#include <map>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/storage_monitor/disk_info_mac.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"

namespace chrome {

// This class posts notifications to listeners when a new disk
// is attached, removed, or changed.
class RemovableDeviceNotificationsMac
    : public StorageMonitor,
      public base::RefCountedThreadSafe<RemovableDeviceNotificationsMac> {
 public:
  enum UpdateType {
    UPDATE_DEVICE_ADDED,
    UPDATE_DEVICE_CHANGED,
    UPDATE_DEVICE_REMOVED,
  };

  // Should only be called by browser start up code.  Use GetInstance() instead.
  RemovableDeviceNotificationsMac();

  void UpdateDisk(const DiskInfoMac& info, UpdateType update_type);

  virtual bool GetStorageInfoForPath(
      const base::FilePath& path,
      StorageInfo* device_info) const OVERRIDE;

  // Returns the storage size of the device present at |location|. If the
  // device information is unavailable, returns zero. |location| must be a
  // top-level mount point.
  virtual uint64 GetStorageSize(const std::string& location) const OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<RemovableDeviceNotificationsMac>;
  virtual ~RemovableDeviceNotificationsMac();

  static void DiskAppearedCallback(DADiskRef disk, void* context);
  static void DiskDisappearedCallback(DADiskRef disk, void* context);
  static void DiskDescriptionChangedCallback(DADiskRef disk,
                                             CFArrayRef keys,
                                             void *context);

  bool ShouldPostNotificationForDisk(const DiskInfoMac& info) const;
  bool FindDiskWithMountPoint(const base::FilePath& mount_point,
                              DiskInfoMac* info) const;

  base::mac::ScopedCFTypeRef<DASessionRef> session_;
  // Maps disk bsd names to disk info objects. This map tracks all mountable
  // devices on the system though only notifications for removable devices are
  // posted.
  std::map<std::string, DiskInfoMac> disk_info_map_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationsMac);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_MAC_H_
