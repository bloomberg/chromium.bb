// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::RemovableDeviceNotificationsCros implementation.

#include "chrome/browser/system_monitor/removable_device_notifications_chromeos.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/media_device_notifications_utils.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/browser/system_monitor/removable_device_constants.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

// Construct a device name using label or manufacturer (vendor and product) name
// details.
string16 GetDeviceName(const disks::DiskMountManager::Disk& disk) {
  std::string device_name = disk.device_label();
  if (device_name.empty()) {
    device_name = disk.vendor_name();
    const std::string& product_name = disk.product_name();
    if (!product_name.empty()) {
      if (!device_name.empty())
        device_name += chrome::kSpaceDelim;
      device_name += product_name;
    }
  }
  return UTF8ToUTF16(device_name);
}

// Construct a device id using uuid or manufacturer (vendor and product) id
// details.
std::string MakeDeviceUniqueId(const disks::DiskMountManager::Disk& disk) {
  std::string uuid = disk.fs_uuid();
  if (!uuid.empty())
    return chrome::kFSUniqueIdPrefix + uuid;

  // If one of the vendor or product information is missing, its value in the
  // string is empty.
  // Format: VendorModelSerial:VendorInfo:ModelInfo:SerialInfo
  // TODO(kmadhusu) Extract serial information for the disks and append it to
  // the device unique id.
  const std::string& vendor = disk.vendor_id();
  const std::string& product = disk.product_id();
  if (vendor.empty() && product.empty())
    return std::string();
  return base::StringPrintf("%s%s%s%s%s",
                            chrome::kVendorModelSerialPrefix,
                            vendor.c_str(), chrome::kNonSpaceDelim,
                            product.c_str(), chrome::kNonSpaceDelim);
}

// Returns true if the requested device is valid, else false. On success, fills
// in |unique_id| and |device_label|
bool GetDeviceInfo(const std::string& source_path, std::string* unique_id,
                   string16* device_label) {
  // Get the media device uuid and label if exists.
  const disks::DiskMountManager::Disk* disk =
      disks::DiskMountManager::GetInstance()->FindDiskBySourcePath(source_path);
  if (!disk || disk->device_type() == DEVICE_TYPE_UNKNOWN)
    return false;

  if (unique_id)
    *unique_id = MakeDeviceUniqueId(*disk);

  if (device_label)
    *device_label = GetDeviceName(*disk);
  return true;
}

}  // namespace

using chrome::MediaStorageUtil;
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

  // Keep track of device uuid and label, to see how often we receive empty
  // values.
  MediaStorageUtil::RecordDeviceInfoHistogram(true, unique_id, device_label);
  if (unique_id.empty() || device_label.empty())
    return;

  MediaStorageUtil::Type type = has_dcim ?
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM :
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;

  std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);
  mount_map_.insert(std::make_pair(mount_info.mount_path, device_id));
  base::SystemMonitor::Get()->ProcessRemovableStorageAttached(
      device_id,
      device_label,
      mount_info.mount_path);
}

}  // namespace chrome
