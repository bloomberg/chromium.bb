// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::StorageMonitorCros implementation.

#include "chrome/browser/storage_monitor/storage_monitor_chromeos.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/media_transfer_protocol_device_observer_linux.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "content/public/browser/browser_thread.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

namespace chromeos {

namespace {

// Constructs a device id using uuid or manufacturer (vendor and product) id
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
  return chrome::kVendorModelSerialPrefix + vendor + ":" + product + ":";
}

// Returns true if the requested device is valid, else false. On success, fills
// in |info|.
bool GetDeviceInfo(const disks::DiskMountManager::MountPointInfo& mount_info,
                   bool has_dcim,
                   chrome::StorageInfo* info) {
  DCHECK(info);
  std::string source_path = mount_info.source_path;

  const disks::DiskMountManager::Disk* disk =
      disks::DiskMountManager::GetInstance()->FindDiskBySourcePath(source_path);
  if (!disk || disk->device_type() == DEVICE_TYPE_UNKNOWN)
    return false;

  std::string unique_id = MakeDeviceUniqueId(*disk);
  // Keep track of device uuid and label, to see how often we receive empty
  // values.
  string16 device_label = UTF8ToUTF16(disk->device_label());
  chrome::MediaStorageUtil::RecordDeviceInfoHistogram(true, unique_id,
                                                      device_label);
  if (unique_id.empty())
    return false;

  chrome::StorageInfo::Type type = has_dcim ?
      chrome::StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM :
      chrome::StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM;

  *info = chrome::StorageInfo(
      chrome::StorageInfo::MakeDeviceId(type, unique_id),
      string16(),
      mount_info.mount_path,
      device_label,
      UTF8ToUTF16(disk->vendor_name()),
      UTF8ToUTF16(disk->product_name()),
      disk->total_size_in_bytes());
  return true;
}

// Returns whether the mount point in |mount_info| is a media device or not.
bool CheckMountedPathOnFileThread(
    const disks::DiskMountManager::MountPointInfo& mount_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  return chrome::MediaStorageUtil::HasDcim(
      base::FilePath(mount_info.mount_path));
}

}  // namespace

using content::BrowserThread;
using chrome::StorageInfo;

StorageMonitorCros::StorageMonitorCros()
    : weak_ptr_factory_(this) {
}

StorageMonitorCros::~StorageMonitorCros() {
  disks::DiskMountManager* manager = disks::DiskMountManager::GetInstance();
  if (manager) {
    manager->RemoveObserver(this);
  }
}

void StorageMonitorCros::Init() {
  DCHECK(disks::DiskMountManager::GetInstance());
  disks::DiskMountManager::GetInstance()->AddObserver(this);
  CheckExistingMountPoints();

  if (!media_transfer_protocol_manager_) {
    scoped_refptr<base::MessageLoopProxy> loop_proxy;
    media_transfer_protocol_manager_.reset(
        device::MediaTransferProtocolManager::Initialize(loop_proxy));
  }

  media_transfer_protocol_device_observer_.reset(
      new chrome::MediaTransferProtocolDeviceObserverLinux(
          receiver(), media_transfer_protocol_manager_.get()));
}

void StorageMonitorCros::CheckExistingMountPoints() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const disks::DiskMountManager::MountPointMap& mount_point_map =
      disks::DiskMountManager::GetInstance()->mount_points();
  for (disks::DiskMountManager::MountPointMap::const_iterator it =
           mount_point_map.begin(); it != mount_point_map.end(); ++it) {
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CheckMountedPathOnFileThread, it->second),
        base::Bind(&StorageMonitorCros::AddMountedPath,
                   weak_ptr_factory_.GetWeakPtr(), it->second));
  }

  // Note: relies on scheduled tasks on the file thread being sequential. This
  // block needs to follow the for loop, so that the DoNothing call on the FILE
  // thread happens after the scheduled metadata retrievals, meaning that the
  // reply callback will then happen after all the AddNewMount calls.
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&base::DoNothing),
      base::Bind(&StorageMonitorCros::MarkInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

void StorageMonitorCros::OnDiskEvent(
    disks::DiskMountManager::DiskEvent event,
    const disks::DiskMountManager::Disk* disk) {
}

void StorageMonitorCros::OnDeviceEvent(
    disks::DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
}

void StorageMonitorCros::OnMountEvent(
    disks::DiskMountManager::MountEvent event,
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

  switch (event) {
    case disks::DiskMountManager::MOUNTING: {
      if (ContainsKey(mount_map_, mount_info.mount_path)) {
        NOTREACHED();
        return;
      }

      BrowserThread::PostTaskAndReplyWithResult(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&CheckMountedPathOnFileThread, mount_info),
          base::Bind(&StorageMonitorCros::AddMountedPath,
                     weak_ptr_factory_.GetWeakPtr(), mount_info));
      break;
    }
    case disks::DiskMountManager::UNMOUNTING: {
      MountMap::iterator it = mount_map_.find(mount_info.mount_path);
      if (it == mount_map_.end())
        return;
      receiver()->ProcessDetach(it->second.device_id());
      mount_map_.erase(it);
      break;
    }
  }
}

void StorageMonitorCros::OnFormatEvent(
    disks::DiskMountManager::FormatEvent event,
    FormatError error_code,
    const std::string& device_path) {
}

void StorageMonitorCros::SetMediaTransferProtocolManagerForTest(
    device::MediaTransferProtocolManager* test_manager) {
  DCHECK(!media_transfer_protocol_manager_);
  media_transfer_protocol_manager_.reset(test_manager);
}


bool StorageMonitorCros::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  DCHECK(device_info);

  if (media_transfer_protocol_device_observer_->GetStorageInfoForPath(
          path, device_info)) {
    return true;
  }

  if (!path.IsAbsolute())
    return false;

  base::FilePath current = path;
  while (!ContainsKey(mount_map_, current.value()) &&
         current != current.DirName()) {
    current = current.DirName();
  }

  MountMap::const_iterator info_it = mount_map_.find(current.value());
  if (info_it == mount_map_.end())
    return false;

  *device_info = info_it->second;
  return true;
}

// Callback executed when the unmount call is run by DiskMountManager.
// Forwards result to |EjectDevice| caller.
void NotifyUnmountResult(
    base::Callback<void(chrome::StorageMonitor::EjectStatus)> callback,
    chromeos::MountError error_code) {
  if (error_code == MOUNT_ERROR_NONE)
    callback.Run(chrome::StorageMonitor::EJECT_OK);
  else
    callback.Run(chrome::StorageMonitor::EJECT_FAILURE);
}

void StorageMonitorCros::EjectDevice(
    const std::string& device_id,
    base::Callback<void(EjectStatus)> callback) {
  StorageInfo::Type type;
  if (!StorageInfo::CrackDeviceId(device_id, &type, NULL)) {
    callback.Run(EJECT_FAILURE);
    return;
  }

  if (type == StorageInfo::MTP_OR_PTP) {
    media_transfer_protocol_device_observer_->EjectDevice(device_id, callback);
    return;
  }

  std::string mount_path;
  for (MountMap::const_iterator info_it = mount_map_.begin();
       info_it != mount_map_.end(); ++info_it) {
    if (info_it->second.device_id() == device_id)
      mount_path = info_it->first;
  }

  if (mount_path.empty()) {
    callback.Run(EJECT_NO_SUCH_DEVICE);
    return;
  }

  disks::DiskMountManager* manager = disks::DiskMountManager::GetInstance();
  if (!manager) {
    callback.Run(EJECT_FAILURE);
    return;
  }

  manager->UnmountPath(mount_path, chromeos::UNMOUNT_OPTIONS_NONE,
                       base::Bind(NotifyUnmountResult, callback));
}

device::MediaTransferProtocolManager*
StorageMonitorCros::media_transfer_protocol_manager() {
  return media_transfer_protocol_manager_.get();
}

void StorageMonitorCros::AddMountedPath(
    const disks::DiskMountManager::MountPointInfo& mount_info, bool has_dcim) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (ContainsKey(mount_map_, mount_info.mount_path)) {
    // CheckExistingMountPointsOnUIThread() added the mount point information
    // in the map before the device attached handler is called. Therefore, an
    // entry for the device already exists in the map.
    return;
  }

  // Get the media device uuid and label if exists.
  chrome::StorageInfo info;
  if (!GetDeviceInfo(mount_info, has_dcim, &info))
    return;

  if (info.device_id().empty())
    return;

  mount_map_.insert(std::make_pair(mount_info.mount_path, info));

  receiver()->ProcessAttach(info);
}

}  // namespace chromeos

namespace chrome {

StorageMonitor* StorageMonitor::Create() {
  return new chromeos::StorageMonitorCros();
}

}  // namespace chrome
