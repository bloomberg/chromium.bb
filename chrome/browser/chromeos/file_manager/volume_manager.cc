// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/volume_manager.h"

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager {
namespace {

VolumeType MountTypeToVolumeType(
    chromeos::MountType type) {
  switch (type) {
    case chromeos::MOUNT_TYPE_DEVICE:
      return VOLUME_TYPE_REMOVABLE_DISK_PARTITION;
    case chromeos::MOUNT_TYPE_ARCHIVE:
      return VOLUME_TYPE_MOUNTED_ARCHIVE_FILE;
    default:
      NOTREACHED();
  }

  return VOLUME_TYPE_DOWNLOADS_DIRECTORY;
}

VolumeInfo CreateDownloadsVolumeInfo(
    const base::FilePath& downloads_path) {
  VolumeInfo volume_info;
  volume_info.type = VOLUME_TYPE_DOWNLOADS_DIRECTORY;
  // Keep source_path empty.
  volume_info.mount_path = downloads_path;
  volume_info.mount_condition = chromeos::disks::MOUNT_CONDITION_NONE;
  return volume_info;
}

VolumeInfo CreateVolumeInfoFromMountPointInfo(
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_point) {
  VolumeInfo volume_info;
  volume_info.type = MountTypeToVolumeType(mount_point.mount_type);
  volume_info.source_path = base::FilePath(mount_point.source_path);
  volume_info.mount_path = base::FilePath(mount_point.mount_path);
  volume_info.mount_condition = mount_point.mount_condition;
  return volume_info;
}

}  // namespace

VolumeManager::VolumeManager(
    Profile* profile,
    chromeos::disks::DiskMountManager* disk_mount_manager)
    : profile_(profile),
      disk_mount_manager_(disk_mount_manager) {
  DCHECK(disk_mount_manager);
}

VolumeManager::~VolumeManager() {
}

VolumeManager* VolumeManager::Get(content::BrowserContext* context) {
  return VolumeManagerFactory::Get(context);
}

void VolumeManager::Initialize() {
  disk_mount_manager_->AddObserver(this);
  disk_mount_manager_->RequestMountInfoRefresh();
}

void VolumeManager::Shutdown() {
  disk_mount_manager_->RemoveObserver(this);
}

void VolumeManager::AddObserver(VolumeManagerObserver* observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void VolumeManager::RemoveObserver(VolumeManagerObserver* observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::vector<VolumeInfo> VolumeManager::GetVolumeInfoList() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::vector<VolumeInfo> result;

  // TODO(hidehiko): Adds Drive if available.

  // Adds "Downloads".
  base::FilePath home_path;
  if (PathService::Get(base::DIR_HOME, &home_path)) {
    result.push_back(
        CreateDownloadsVolumeInfo(home_path.AppendASCII("Downloads")));
  }

  // Adds disks (both removable disks and zip archives).
  const chromeos::disks::DiskMountManager::MountPointMap& mount_points =
      disk_mount_manager_->mount_points();
  for (chromeos::disks::DiskMountManager::MountPointMap::const_iterator it =
           mount_points.begin();
       it != mount_points.end(); ++it) {
    if (it->second.mount_type == chromeos::MOUNT_TYPE_DEVICE ||
        it->second.mount_type == chromeos::MOUNT_TYPE_ARCHIVE)
      result.push_back(CreateVolumeInfoFromMountPointInfo(it->second));
  }

  return result;
}

void VolumeManager::OnDiskEvent(
    chromeos::disks::DiskMountManager::DiskEvent event,
    const chromeos::disks::DiskMountManager::Disk* disk) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Disregard hidden devices.
  if (disk->is_hidden())
    return;

  switch (event) {
    case chromeos::disks::DiskMountManager::DISK_ADDED: {
      if (disk->device_path().empty()) {
        DVLOG(1) << "Empty system path for " << disk->device_path();
        return;
      }

      bool mounting = false;
      if (disk->mount_path().empty() && disk->has_media() &&
          !profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
        // If disk is not mounted yet and it has media and there is no policy
        // forbidding external storage, give it a try.
        // Initiate disk mount operation. MountPath auto-detects the filesystem
        // format if the second argument is empty. The third argument (mount
        // label) is not used in a disk mount operation.
        disk_mount_manager_->MountPath(
            disk->device_path(), std::string(), std::string(),
            chromeos::MOUNT_TYPE_DEVICE);
        mounting = true;
      }

      // Notify to observers.
      FOR_EACH_OBSERVER(VolumeManagerObserver, observers_,
                        OnDiskAdded(*disk, mounting));
      return;
    }

    case chromeos::disks::DiskMountManager::DISK_REMOVED:
      // If the disk is already mounted, unmount it.
      if (!disk->mount_path().empty()) {
        disk_mount_manager_->UnmountPath(
            disk->mount_path(),
            chromeos::UNMOUNT_OPTIONS_LAZY,
            chromeos::disks::DiskMountManager::UnmountPathCallback());
      }

      // Notify to observers.
      FOR_EACH_OBSERVER(VolumeManagerObserver, observers_,
                        OnDiskRemoved(*disk));
      return;

    case chromeos::disks::DiskMountManager::DISK_CHANGED:
      DVLOG(1) << "Ignore CHANGED event.";
      return;
  }
  NOTREACHED();
}

void VolumeManager::OnDeviceEvent(
    chromeos::disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DVLOG(1) << "OnDeviceEvent: " << event << ", " << device_path;

  switch (event) {
    case chromeos::disks::DiskMountManager::DEVICE_ADDED:
      FOR_EACH_OBSERVER(VolumeManagerObserver, observers_,
                        OnDeviceAdded(device_path));
      return;
    case chromeos::disks::DiskMountManager::DEVICE_REMOVED:
      FOR_EACH_OBSERVER(VolumeManagerObserver, observers_,
                        OnDeviceRemoved(device_path));
      return;
    case chromeos::disks::DiskMountManager::DEVICE_SCANNED:
      DVLOG(1) << "Ignore SCANNED event: " << device_path;
      return;
  }
  NOTREACHED();
}

void VolumeManager::OnMountEvent(
    chromeos::disks::DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
  // TODO(hidehiko): Move the implementation from EventRouter.
}

void VolumeManager::OnFormatEvent(
    chromeos::disks::DiskMountManager::FormatEvent event,
    chromeos::FormatError error_code,
    const std::string& device_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DVLOG(1) << "OnDeviceEvent: " << event << ", " << error_code
           << ", " << device_path;

  switch (event) {
    case chromeos::disks::DiskMountManager::FORMAT_STARTED:
      FOR_EACH_OBSERVER(
          VolumeManagerObserver, observers_,
          OnFormatStarted(device_path,
                          error_code == chromeos::FORMAT_ERROR_NONE));
      return;
    case chromeos::disks::DiskMountManager::FORMAT_COMPLETED:
      if (error_code == chromeos::FORMAT_ERROR_NONE) {
        // If format is completed successfully, try to mount the device.
        // MountPath auto-detects filesystem format if second argument is
        // empty. The third argument (mount label) is not used in a disk mount
        // operation.
        disk_mount_manager_->MountPath(
            device_path, std::string(), std::string(),
            chromeos::MOUNT_TYPE_DEVICE);
      }

      FOR_EACH_OBSERVER(
          VolumeManagerObserver, observers_,
          OnFormatCompleted(device_path,
                            error_code == chromeos::FORMAT_ERROR_NONE));

      return;
  }
  NOTREACHED();
}

}  // namespace file_manager
