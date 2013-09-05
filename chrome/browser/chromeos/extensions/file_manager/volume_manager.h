// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_VOLUME_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_VOLUME_MANAGER_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace file_manager {

class VolumeManagerObserver;

// This manager manages "Drive" and "Downloads" in addition to disks managed
// by DiskMountManager.
enum VolumeType {
  VOLUME_TYPE_GOOGLE_DRIVE,
  VOLUME_TYPE_DOWNLOADS_DIRECTORY,
  VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
  VOLUME_TYPE_MOUNTED_ARCHIVE_FILE,
};

struct VolumeInfo {
  // The type of mounted volume.
  VolumeType type;

  // The source path of the volume.
  // E.g.:
  // - /home/chronos/user/Downloads/zipfile_path.zip
  base::FilePath source_path;

  // The mount path of the volume.
  // E.g.:
  // - /home/chronos/user/Downloads
  // - /media/removable/usb1
  // - /media/archive/zip1
  base::FilePath mount_path;

  // The mounting condition. See the enum for the details.
  chromeos::disks::MountCondition mount_condition;
};

// Manages "Volume"s for file manager. Here are "Volume"s.
// - Drive File System (not yet supported).
// - Downloads directory.
// - Removable disks (volume will be created for each partition, not only one
//   for a device).
// - Mounted zip archives.
class VolumeManager : public BrowserContextKeyedService,
                      public chromeos::disks::DiskMountManager::Observer {
 public:
  VolumeManager(Profile* profile,
                chromeos::disks::DiskMountManager* disk_mount_manager);
  virtual ~VolumeManager();

  // Returns the instance corresponding to the |context|.
  static VolumeManager* Get(content::BrowserContext* context);

  // Intializes this instance.
  void Initialize();

  // Disposes this instance.
  virtual void Shutdown() OVERRIDE;

  // Adds an observer.
  void AddObserver(VolumeManagerObserver* observer);

  // Removes the observer.
  void RemoveObserver(VolumeManagerObserver* observer);

  // Returns the information about all volumes currently mounted.
  // TODO(hidehiko): make this just an accessor.
  std::vector<VolumeInfo> GetVolumeInfoList() const;

  // chromeos::disks::DiskMountManager::Observer overrides.
  virtual void OnDiskEvent(
      chromeos::disks::DiskMountManager::DiskEvent event,
      const chromeos::disks::DiskMountManager::Disk* disk) OVERRIDE;
  virtual void OnDeviceEvent(
      chromeos::disks::DiskMountManager::DeviceEvent event,
      const std::string& device_path) OVERRIDE;
  virtual void OnMountEvent(
      chromeos::disks::DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info)
      OVERRIDE;
  virtual void OnFormatEvent(
      chromeos::disks::DiskMountManager::FormatEvent event,
      chromeos::FormatError error_code,
      const std::string& device_path) OVERRIDE;

 private:
  Profile* profile_;
  chromeos::disks::DiskMountManager* disk_mount_manager_;
  ObserverList<VolumeManagerObserver> observers_;
  DISALLOW_COPY_AND_ASSIGN(VolumeManager);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_VOLUME_MANAGER_H_
