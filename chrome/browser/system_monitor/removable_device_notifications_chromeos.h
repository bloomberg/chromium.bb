// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::RemovableDeviceNotificationsCros listens for mount point changes
// and notifies the SystemMonitor about the addition and deletion of media
// devices.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_CHROMEOS_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_CHROMEOS_H_

#if !defined(OS_CHROMEOS)
#error "Should only be used on ChromeOS."
#endif

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/system_monitor/system_monitor.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace chromeos {

class RemovableDeviceNotificationsCros
    : public base::RefCountedThreadSafe<RemovableDeviceNotificationsCros>,
      public disks::DiskMountManager::Observer {
 public:
  RemovableDeviceNotificationsCros();

  virtual void DiskChanged(disks::DiskMountManagerEventType event,
                           const disks::DiskMountManager::Disk* disk) OVERRIDE;
  virtual void DeviceChanged(disks::DiskMountManagerEventType event,
                             const std::string& device_path) OVERRIDE;
  virtual void MountCompleted(
      disks::DiskMountManager::MountEvent event_type,
      MountError error_code,
      const disks::DiskMountManager::MountPointInfo& mount_info) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<RemovableDeviceNotificationsCros>;

  // Mapping of mount points to mount device IDs.
  typedef std::map<std::string, std::string> MountMap;

  // Private to avoid code deleting the object.
  virtual ~RemovableDeviceNotificationsCros();

  // Checks existing mount points map for media devices. For each mount point,
  // call CheckMountedPathOnFileThread() below.
  void CheckExistingMountPointsOnUIThread();

  // Checks if the mount point in |mount_info| is a media device. If it is,
  // then continue with AddMountedPathOnUIThread() below.
  void CheckMountedPathOnFileThread(
      const disks::DiskMountManager::MountPointInfo& mount_info);

  // Adds the mount point in |mount_info| to |mount_map_| and send a media
  // device attach notification. |has_dcim| is true if the attached device has
  // a DCIM folder.
  void AddMountedPathOnUIThread(
      const disks::DiskMountManager::MountPointInfo& mount_info,
      bool has_dcim);

  // Mapping of relevant mount points and their corresponding mount devices.
  // Only accessed on the UI thread.
  MountMap mount_map_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationsCros);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_CHROMEOS_H_
