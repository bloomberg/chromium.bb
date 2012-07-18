// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::MediaDeviceNotifications listens for mount point changes and
// notifies the SystemMonitor about the addition and deletion of media devices.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_NOTIFICATIONS_CHROMEOS_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_NOTIFICATIONS_CHROMEOS_H_

#if !defined(OS_CHROMEOS)
#error "Should only be used on ChromeOS."
#endif

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/chromeos/disks/disk_mount_manager.h"

namespace chromeos {

class MediaDeviceNotifications
    : public base::RefCountedThreadSafe<MediaDeviceNotifications>,
      public disks::DiskMountManager::Observer {
 public:
  MediaDeviceNotifications();

  virtual void DiskChanged(disks::DiskMountManagerEventType event,
                           const disks::DiskMountManager::Disk* disk) OVERRIDE;
  virtual void DeviceChanged(disks::DiskMountManagerEventType event,
                             const std::string& device_path) OVERRIDE;
  virtual void MountCompleted(
      disks::DiskMountManager::MountEvent event_type,
      MountError error_code,
      const disks::DiskMountManager::MountPointInfo& mount_info) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<MediaDeviceNotifications>;

  // Mapping of mount points to mount device IDs.
  typedef std::map<std::string, base::SystemMonitor::DeviceIdType> MountMap;

  // Private to avoid code deleting the object.
  virtual ~MediaDeviceNotifications();

  // Checks if the mount point in |mount_info| is a media device. If it is,
  // then continue with AddMountedPathOnUIThread() below.
  void CheckMountedPathOnFileThread(
      const disks::DiskMountManager::MountPointInfo& mount_info);

  // Adds the mount point in |mount_info| to |mount_map_| and send a media
  // device attach notification.
  void AddMountedPathOnUIThread(
      const disks::DiskMountManager::MountPointInfo& mount_info);

  // The lowest available device id number.
  // Only accessed on the UI thread.
  base::SystemMonitor::DeviceIdType current_device_id_;

  // Mapping of relevant mount points and their corresponding mount devices.
  // Only accessed on the UI thread.
  MountMap mount_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaDeviceNotifications);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_DEVICE_NOTIFICATIONS_CHROMEOS_H_
