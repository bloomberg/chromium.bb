// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_monitor_chromeos.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/media_transfer_protocol_device_observer_chromeos.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "content/public/browser/browser_thread.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

using chromeos::disks::DiskMountManager;

namespace storage_monitor {

namespace {

// Constructs a device id using uuid or manufacturer (vendor and product) id
// details.
std::string MakeDeviceUniqueId(const DiskMountManager::Disk& disk) {
  std::string uuid = disk.fs_uuid();
  if (!uuid.empty())
    return kFSUniqueIdPrefix + uuid;

  // If one of the vendor or product information is missing, its value in the
  // string is empty.
  // Format: VendorModelSerial:VendorInfo:ModelInfo:SerialInfo
  // TODO(kmadhusu) Extract serial information for the disks and append it to
  // the device unique id.
  const std::string& vendor = disk.vendor_id();
  const std::string& product = disk.product_id();
  if (vendor.empty() && product.empty())
    return std::string();
  return kVendorModelSerialPrefix + vendor + ":" + product + ":";
}

// Returns whether the requested device is valid. On success |info| will contain
// device information.
bool GetDeviceInfo(const DiskMountManager::MountPointInfo& mount_info,
                   bool has_dcim,
                   StorageInfo* info) {
  DCHECK(info);
  std::string source_path = mount_info.source_path;

  const DiskMountManager::Disk* disk =
      DiskMountManager::GetInstance()->FindDiskBySourcePath(source_path);
  if (!disk || disk->device_type() == chromeos::DEVICE_TYPE_UNKNOWN)
    return false;

  std::string unique_id = MakeDeviceUniqueId(*disk);
  if (unique_id.empty())
    return false;

  StorageInfo::Type type = has_dcim ?
      StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM :
      StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM;

  *info = StorageInfo(
      StorageInfo::MakeDeviceId(type, unique_id), mount_info.mount_path,
      base::UTF8ToUTF16(disk->device_label()),
      base::UTF8ToUTF16(disk->vendor_name()),
      base::UTF8ToUTF16(disk->product_name()), disk->total_size_in_bytes());
  return true;
}

// Returns whether the requested device is valid. On success |info| will contain
// fixed storage device information.
bool GetFixedStorageInfo(const DiskMountManager::Disk& disk,
                         StorageInfo* info) {
  DCHECK(info);

  std::string unique_id = MakeDeviceUniqueId(disk);
  if (unique_id.empty())
    return false;

  *info = StorageInfo(
      StorageInfo::MakeDeviceId(StorageInfo::FIXED_MASS_STORAGE, unique_id),
      disk.mount_path(), base::UTF8ToUTF16(disk.device_label()),
      base::UTF8ToUTF16(disk.vendor_name()),
      base::UTF8ToUTF16(disk.product_name()), disk.total_size_in_bytes());
  return true;
}

}  // namespace

StorageMonitorCros::StorageMonitorCros() : weak_ptr_factory_(this) {}

StorageMonitorCros::~StorageMonitorCros() {
  DiskMountManager* manager = DiskMountManager::GetInstance();
  if (manager) {
    manager->RemoveObserver(this);
  }
}

void StorageMonitorCros::Init() {
  DCHECK(DiskMountManager::GetInstance());
  DiskMountManager::GetInstance()->AddObserver(this);
  CheckExistingMountPoints();

  // Tests may have already set a MTP manager.
  if (!media_transfer_protocol_manager_) {
    media_transfer_protocol_manager_ =
        device::MediaTransferProtocolManager::Initialize();
  }
  media_transfer_protocol_device_observer_ =
      std::make_unique<MediaTransferProtocolDeviceObserverChromeOS>(
          receiver(), media_transfer_protocol_manager_.get());
}

void StorageMonitorCros::CheckExistingMountPoints() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& it : DiskMountManager::GetInstance()->disks()) {
    if (it.second->IsStatefulPartition()) {
      AddFixedStorageDisk(*it.second);
      break;
    }
  }

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND});

  for (const auto& it : DiskMountManager::GetInstance()->mount_points()) {
    base::PostTaskAndReplyWithResult(
        blocking_task_runner.get(), FROM_HERE,
        base::Bind(&MediaStorageUtil::HasDcim,
                   base::FilePath(it.second.mount_path)),
        base::Bind(&StorageMonitorCros::AddMountedPath,
                   weak_ptr_factory_.GetWeakPtr(), it.second));
  }

  // Note: Relies on scheduled tasks on the |blocking_task_runner| being
  // sequential. This block needs to follow the for loop, so that the DoNothing
  // call on the |blocking_task_runner| happens after the scheduled metadata
  // retrievals, meaning that the reply callback will then happen after all the
  // AddMountedPath calls.

  blocking_task_runner->PostTaskAndReply(
      FROM_HERE, base::Bind(&base::DoNothing),
      base::Bind(&StorageMonitorCros::MarkInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

void StorageMonitorCros::OnAutoMountableDiskEvent(
    DiskMountManager::DiskEvent event,
    const DiskMountManager::Disk& disk) {}

void StorageMonitorCros::OnBootDeviceDiskEvent(
    DiskMountManager::DiskEvent event,
    const DiskMountManager::Disk& disk) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!disk.IsStatefulPartition())
    return;

  switch (event) {
    case DiskMountManager::DiskEvent::DISK_ADDED: {
      AddFixedStorageDisk(disk);
      break;
    }
    case DiskMountManager::DiskEvent::DISK_REMOVED: {
      RemoveFixedStorageDisk(disk);
      break;
    }
    case DiskMountManager::DiskEvent::DISK_CHANGED: {
      NOTREACHED() << "DiskMountManager::DiskEvent::DISK_CHANGED should not "
                      "occur for disks on boot device";
      break;
    }
  }
}

void StorageMonitorCros::OnDeviceEvent(DiskMountManager::DeviceEvent event,
                                       const std::string& device_path) {}

void StorageMonitorCros::OnMountEvent(
    DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Ignore mount points that are not devices.
  if (mount_info.mount_type != chromeos::MOUNT_TYPE_DEVICE)
    return;
  // Ignore errors.
  if (error_code != chromeos::MOUNT_ERROR_NONE)
    return;
  if (mount_info.mount_condition != chromeos::disks::MOUNT_CONDITION_NONE)
    return;

  switch (event) {
    case DiskMountManager::MOUNTING: {
      if (base::ContainsKey(mount_map_, mount_info.mount_path)) {
        NOTREACHED();
        return;
      }

      base::PostTaskWithTraitsAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
          base::Bind(&MediaStorageUtil::HasDcim,
                     base::FilePath(mount_info.mount_path)),
          base::Bind(&StorageMonitorCros::AddMountedPath,
                     weak_ptr_factory_.GetWeakPtr(), mount_info));
      break;
    }
    case DiskMountManager::UNMOUNTING: {
      MountMap::iterator it = mount_map_.find(mount_info.mount_path);
      if (it == mount_map_.end())
        return;
      receiver()->ProcessDetach(it->second.device_id());
      mount_map_.erase(it);
      break;
    }
  }
}

void StorageMonitorCros::OnFormatEvent(DiskMountManager::FormatEvent event,
                                       chromeos::FormatError error_code,
                                       const std::string& device_path) {}

void StorageMonitorCros::OnRenameEvent(DiskMountManager::RenameEvent event,
                                       chromeos::RenameError error_code,
                                       const std::string& device_path) {}

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
  while (!base::ContainsKey(mount_map_, current.value()) &&
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
    base::Callback<void(StorageMonitor::EjectStatus)> callback,
    chromeos::MountError error_code) {
  if (error_code == chromeos::MOUNT_ERROR_NONE)
    callback.Run(StorageMonitor::EJECT_OK);
  else
    callback.Run(StorageMonitor::EJECT_FAILURE);
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

  DiskMountManager* manager = DiskMountManager::GetInstance();
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
    const DiskMountManager::MountPointInfo& mount_info,
    bool has_dcim) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::ContainsKey(mount_map_, mount_info.mount_path)) {
    // CheckExistingMountPoints() added the mount point information in the map
    // before the device attached handler is called. Therefore, an entry for
    // the device already exists in the map.
    return;
  }

  // Get the media device uuid and label if exists.
  StorageInfo info;
  if (!GetDeviceInfo(mount_info, has_dcim, &info))
    return;

  if (info.device_id().empty())
    return;

  mount_map_.insert(std::make_pair(mount_info.mount_path, info));

  receiver()->ProcessAttach(info);
}

void StorageMonitorCros::AddFixedStorageDisk(
    const DiskMountManager::Disk& disk) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(disk.IsStatefulPartition());

  StorageInfo info;
  if (!GetFixedStorageInfo(disk, &info))
    return;

  if (base::ContainsKey(mount_map_, disk.mount_path()))
    return;

  mount_map_.insert(std::make_pair(disk.mount_path(), info));
  receiver()->ProcessAttach(info);
}

void StorageMonitorCros::RemoveFixedStorageDisk(
    const DiskMountManager::Disk& disk) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(disk.IsStatefulPartition());

  StorageInfo info;
  if (!GetFixedStorageInfo(disk, &info))
    return;

  size_t erased_count = mount_map_.erase(disk.mount_path());
  if (!erased_count)
    return;

  receiver()->ProcessDetach((info.device_id()));
}

StorageMonitor* StorageMonitor::CreateInternal() {
  return new StorageMonitorCros();
}

}  // namespace storage_monitor
