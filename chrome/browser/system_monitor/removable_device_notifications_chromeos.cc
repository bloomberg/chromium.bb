// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::RemovableDeviceNotificationsCros implementation.

#include "chrome/browser/system_monitor/removable_device_notifications_chromeos.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/media_device_notifications_utils.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

// Returns true if the requested device is valid, else false. On success, fills
// in |unique_id| and |device_label|
bool GetDeviceInfo(const std::string& source_path, std::string* unique_id,
                   string16* device_label) {
  // Get the media device uuid and label if exists.
  const disks::DiskMountManager::Disk* disk =
      disks::DiskMountManager::GetInstance()->FindDiskBySourcePath(source_path);
  if (!disk || disk->device_type() == DEVICE_TYPE_UNKNOWN)
    return false;

  *unique_id = disk->fs_uuid();

  // TODO(kmadhusu): If device label is empty, extract vendor and model details
  // and use them as device_label.
  *device_label = UTF8ToUTF16(disk->device_label().empty() ?
                              FilePath(source_path).BaseName().value() :
                              disk->device_label());
  return true;
}

}  // namespace

using content::BrowserThread;

RemovableDeviceNotificationsCros::RemovableDeviceNotificationsCros() {
  DCHECK(disks::DiskMountManager::GetInstance());
  disks::DiskMountManager::GetInstance()->AddObserver(this);
  CheckExistingMountPointsOnUIThread();
}

RemovableDeviceNotificationsCros::~RemovableDeviceNotificationsCros() {
  disks::DiskMountManager* manager = disks::DiskMountManager::GetInstance();
  if (manager) {
    manager->RemoveObserver(this);
  }
}

void RemovableDeviceNotificationsCros::CheckExistingMountPointsOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const disks::DiskMountManager::MountPointMap& mount_point_map =
      disks::DiskMountManager::GetInstance()->mount_points();
  for (disks::DiskMountManager::MountPointMap::const_iterator it =
           mount_point_map.begin(); it != mount_point_map.end(); ++it) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(
            &RemovableDeviceNotificationsCros::CheckMountedPathOnFileThread,
            this, it->second));
  }
}

void RemovableDeviceNotificationsCros::DiskChanged(
    disks::DiskMountManagerEventType event,
    const disks::DiskMountManager::Disk* disk) {
}

void RemovableDeviceNotificationsCros::DeviceChanged(
    disks::DiskMountManagerEventType event,
    const std::string& device_path) {
}

void RemovableDeviceNotificationsCros::MountCompleted(
    disks::DiskMountManager::MountEvent event_type,
    MountError error_code,
    const disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ignore mount points that are not devices.
  if (mount_info.mount_type != MOUNT_TYPE_DEVICE)
    return;
  // Ignore errors.
  if (error_code != MOUNT_ERROR_NONE)
    return;
  if (mount_info.mount_condition != disks::MOUNT_CONDITION_NONE)
    return;

  switch (event_type) {
    case disks::DiskMountManager::MOUNTING: {
      if (ContainsKey(mount_map_, mount_info.mount_path)) {
        NOTREACHED();
        return;
      }

      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(
              &RemovableDeviceNotificationsCros::CheckMountedPathOnFileThread,
              this, mount_info));
      break;
    }
    case disks::DiskMountManager::UNMOUNTING: {
      MountMap::iterator it = mount_map_.find(mount_info.mount_path);
      if (it == mount_map_.end())
        return;
      base::SystemMonitor::Get()->ProcessRemovableStorageDetached(it->second);
      mount_map_.erase(it);
      break;
    }
  }
}

void RemovableDeviceNotificationsCros::CheckMountedPathOnFileThread(
    const disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bool has_dcim = chrome::IsMediaDevice(mount_info.mount_path);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RemovableDeviceNotificationsCros::AddMountedPathOnUIThread,
                 this, mount_info, has_dcim));
}

void RemovableDeviceNotificationsCros::AddMountedPathOnUIThread(
    const disks::DiskMountManager::MountPointInfo& mount_info, bool has_dcim) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (ContainsKey(mount_map_, mount_info.mount_path)) {
    NOTREACHED();
    return;
  }

  // Get the media device uuid and label if exists.
  std::string unique_id;
  string16 device_label;
  if (!GetDeviceInfo(mount_info.source_path, &unique_id, &device_label))
    return;

  // Keep track of device uuid, to see how often we receive empty uuid values.
  UMA_HISTOGRAM_BOOLEAN("MediaDeviceNotification.DeviceUUIDAvailable",
                        !unique_id.empty());
  if (unique_id.empty())
    return;

  chrome::MediaStorageUtil::Type type = has_dcim ?
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM :
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;

  std::string device_id = chrome::MediaStorageUtil::MakeDeviceId(type,
                                                                 unique_id);
  mount_map_.insert(std::make_pair(mount_info.mount_path, device_id));
  base::SystemMonitor::Get()->ProcessRemovableStorageAttached(
      device_id,
      device_label,
      mount_info.mount_path);
}

}  // namespace chrome
