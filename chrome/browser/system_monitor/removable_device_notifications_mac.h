// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_MAC_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_MAC_H_

#include <DiskArbitration/DiskArbitration.h>
#include <map>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/weak_ptr.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/system_monitor/disk_info_mac.h"

namespace chrome {

// This class posts notifications to base::SystemMonitor when a new disk
// is attached, removed, or changed.
class RemovableDeviceNotificationsMac :
    public base::SupportsWeakPtr<RemovableDeviceNotificationsMac> {
 public:
  enum UpdateType {
    UPDATE_DEVICE_ADDED,
    UPDATE_DEVICE_CHANGED,
    UPDATE_DEVICE_REMOVED,
  };

  RemovableDeviceNotificationsMac();
  virtual ~RemovableDeviceNotificationsMac();

  void UpdateDisk(const DiskInfoMac& info, UpdateType update_type);

  bool GetDeviceInfoForPath(
      const FilePath& path,
      base::SystemMonitor::RemovableStorageInfo* device_info) const;

 private:
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
