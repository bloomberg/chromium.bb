// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::MediaDeviceNotifications implementation.

#include "chrome/browser/media_gallery/media_device_notifications_chromeos.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_gallery/media_device_notifications_utils.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

using content::BrowserThread;

MediaDeviceNotifications::MediaDeviceNotifications()
    : current_device_id_(0) {
  DCHECK(disks::DiskMountManager::GetInstance());
  disks::DiskMountManager::GetInstance()->AddObserver(this);
}

MediaDeviceNotifications::~MediaDeviceNotifications() {
  disks::DiskMountManager* manager = disks::DiskMountManager::GetInstance();
  if (manager) {
    manager->RemoveObserver(this);
  }
}

void MediaDeviceNotifications::DiskChanged(
    disks::DiskMountManagerEventType event,
    const disks::DiskMountManager::Disk* disk) {
}

void MediaDeviceNotifications::DeviceChanged(
    disks::DiskMountManagerEventType event,
    const std::string& device_path) {
}

void MediaDeviceNotifications::MountCompleted(
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
          base::Bind(&MediaDeviceNotifications::CheckMountedPathOnFileThread,
                     this, mount_info));
      break;
    }
    case disks::DiskMountManager::UNMOUNTING: {
      MountMap::iterator it = mount_map_.find(mount_info.mount_path);
      if (it == mount_map_.end())
        return;
      base::SystemMonitor::Get()->ProcessMediaDeviceDetached(it->second);
      mount_map_.erase(it);
      break;
    }
  }
}

void MediaDeviceNotifications::CheckMountedPathOnFileThread(
    const disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!chrome::IsMediaDevice(mount_info.mount_path))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaDeviceNotifications::AddMountedPathOnUIThread,
                 this, mount_info));
}

void MediaDeviceNotifications::AddMountedPathOnUIThread(
    const disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (ContainsKey(mount_map_, mount_info.mount_path)) {
    NOTREACHED();
    return;
  }
  const std::string device_id_str = base::IntToString(current_device_id_++);
  mount_map_.insert(std::make_pair(mount_info.mount_path, device_id_str));
  base::SystemMonitor::Get()->ProcessMediaDeviceAttached(
      device_id_str,
      UTF8ToUTF16(FilePath(mount_info.source_path).BaseName().value()),
      base::SystemMonitor::TYPE_PATH,
      mount_info.mount_path);
}

}  // namespace chrome
