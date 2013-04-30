// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::StorageMonitorCros listens for mount point changes and notifies
// listeners about the addition and deletion of media devices.
// This class lives on the UI thread.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_CHROMEOS_H_
#define CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_CHROMEOS_H_

#if !defined(OS_CHROMEOS)
#error "Should only be used on ChromeOS."
#endif

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace chrome {
class MediaTransferProtocolDeviceObserverLinux;
}

namespace chromeos {

class StorageMonitorCros : public chrome::StorageMonitor,
                           public disks::DiskMountManager::Observer {
 public:
  // Should only be called by browser start up code.
  // Use StorageMonitor::GetInstance() instead.
  StorageMonitorCros();
  virtual ~StorageMonitorCros();

  // Sets up disk listeners and issues notifications for any discovered
  // mount points. Sets up MTP manager and listeners.
  virtual void Init() OVERRIDE;

 protected:
  void SetMediaTransferProtocolManagerForTest(
      device::MediaTransferProtocolManager* test_manager);

  // disks::DiskMountManager::Observer implementation.
  virtual void OnDiskEvent(disks::DiskMountManager::DiskEvent event,
                           const disks::DiskMountManager::Disk* disk) OVERRIDE;
  virtual void OnDeviceEvent(disks::DiskMountManager::DeviceEvent event,
                             const std::string& device_path) OVERRIDE;
  virtual void OnMountEvent(
      disks::DiskMountManager::MountEvent event,
      MountError error_code,
      const disks::DiskMountManager::MountPointInfo& mount_info) OVERRIDE;
  virtual void OnFormatEvent(disks::DiskMountManager::FormatEvent event,
                             FormatError error_code,
                             const std::string& device_path) OVERRIDE;

  // StorageMonitor implementation.
  virtual bool GetStorageInfoForPath(
      const base::FilePath& path,
      chrome::StorageInfo* device_info) const OVERRIDE;
  virtual void EjectDevice(
      const std::string& device_id,
      base::Callback<void(EjectStatus)> callback) OVERRIDE;
  virtual device::MediaTransferProtocolManager*
      media_transfer_protocol_manager() OVERRIDE;

 private:
  // Mapping of mount path to removable mass storage info.
  typedef std::map<std::string, chrome::StorageInfo> MountMap;

  // Helper method that checks existing mount points to see if they are media
  // devices. Eventually calls AddMountedPath for all mount points.
  void CheckExistingMountPoints();

  // Adds the mount point in |mount_info| to |mount_map_| and send a media
  // device attach notification. |has_dcim| is true if the attached device has
  // a DCIM folder.
  void AddMountedPath(const disks::DiskMountManager::MountPointInfo& mount_info,
                      bool has_dcim);

  // Mapping of relevant mount points and their corresponding mount devices.
  MountMap mount_map_;

  scoped_ptr<device::MediaTransferProtocolManager>
      media_transfer_protocol_manager_;
  scoped_ptr<chrome::MediaTransferProtocolDeviceObserverLinux>
      media_transfer_protocol_device_observer_;

  base::WeakPtrFactory<StorageMonitorCros> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StorageMonitorCros);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_CHROMEOS_H_
