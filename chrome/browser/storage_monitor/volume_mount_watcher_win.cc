// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/volume_mount_watcher_win.h"

#include <windows.h>
#include <dbt.h>
#include <fileapi.h>

#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_device_notifications_utils.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const DWORD kMaxPathBufLen = MAX_PATH + 1;

bool IsRemovable(const string16& mount_point) {
  if (GetDriveType(mount_point.c_str()) != DRIVE_REMOVABLE)
    return false;

  // We don't consider floppy disks as removable, so check for that.
  string16 device = mount_point;
  if (EndsWith(mount_point, L"\\", false))
    device = mount_point.substr(0, device.length() - 1);
  string16 device_path;
  if (!QueryDosDevice(device.c_str(), WriteInto(&device_path, kMaxPathBufLen),
                      kMaxPathBufLen))
    return true;
  return device_path.find(L"Floppy") == string16::npos;
}

// Returns 0 if the devicetype is not volume.
uint32 GetVolumeBitMaskFromBroadcastHeader(LPARAM data) {
  DEV_BROADCAST_VOLUME* dev_broadcast_volume =
      reinterpret_cast<DEV_BROADCAST_VOLUME*>(data);
  if (dev_broadcast_volume->dbcv_devicetype == DBT_DEVTYP_VOLUME)
    return dev_broadcast_volume->dbcv_unitmask;
  return 0;
}

// Returns true if |data| represents a logical volume structure.
bool IsLogicalVolumeStructure(LPARAM data) {
  DEV_BROADCAST_HDR* broadcast_hdr =
      reinterpret_cast<DEV_BROADCAST_HDR*>(data);
  return broadcast_hdr != NULL &&
         broadcast_hdr->dbch_devicetype == DBT_DEVTYP_VOLUME;
}

// Gets the total volume of the |mount_point| in bytes.
uint64 GetVolumeSize(const string16& mount_point) {
  ULARGE_INTEGER total;
  if (!GetDiskFreeSpaceExW(mount_point.c_str(), NULL, &total, NULL))
    return 0;
  return total.QuadPart;
}

// Gets mass storage device information given a |device_path|. On success,
// returns true and fills in |device_location|, |unique_id|, |name|,
// |removable|, and |total_size_in_bytes|.
// The following msdn blog entry is helpful for understanding disk volumes
// and how they are treated in Windows:
// http://blogs.msdn.com/b/adioltean/archive/2005/04/16/408947.aspx.
bool GetDeviceDetails(const base::FilePath& device_path,
                      string16* device_location,
                      std::string* unique_id,
                      string16* name,
                      bool* removable,
                      uint64* volume_size) {
  string16 mount_point;
  if (!GetVolumePathName(device_path.value().c_str(),
                         WriteInto(&mount_point, kMaxPathBufLen),
                         kMaxPathBufLen)) {
    return false;
  }

  if (removable)
    *removable = IsRemovable(mount_point);

  if (device_location)
    *device_location = mount_point;

  if (volume_size)
    *volume_size = GetVolumeSize(mount_point);

  if (unique_id) {
    string16 guid;
    if (!GetVolumeNameForVolumeMountPoint(mount_point.c_str(),
                                          WriteInto(&guid, kMaxPathBufLen),
                                          kMaxPathBufLen)) {
      return false;
    }
    // In case it has two GUID's (see above mentioned blog), do it again.
    if (!GetVolumeNameForVolumeMountPoint(guid.c_str(),
                                          WriteInto(&guid, kMaxPathBufLen),
                                          kMaxPathBufLen)) {
      return false;
    }
    *unique_id = UTF16ToUTF8(guid);
  }

  if (name) {
    // NOTE: experimentally, this function returns false if there is no volume
    // name set.
    string16 volume_label;
    GetVolumeInformationW(device_path.value().c_str(),
                          WriteInto(&volume_label, kMaxPathBufLen),
                          kMaxPathBufLen, NULL, NULL, NULL, NULL, 0);

    // TODO(gbillock): if volume_label.empty(), get the vendor/model information
    // for the volume.

    *name = !volume_label.empty() ? volume_label
                                  : device_path.LossyDisplayName();
  }

  return true;
}

// Returns a vector of all the removable mass storage devices that are
// connected.
std::vector<base::FilePath> GetAttachedDevices() {
  std::vector<base::FilePath> result;
  string16 volume_name;
  HANDLE find_handle = FindFirstVolume(WriteInto(&volume_name, kMaxPathBufLen),
                                       kMaxPathBufLen);
  if (find_handle == INVALID_HANDLE_VALUE)
    return result;

  while (true) {
    string16 volume_path;
    DWORD return_count;
    if (GetVolumePathNamesForVolumeName(volume_name.c_str(),
                                        WriteInto(&volume_path, kMaxPathBufLen),
                                        kMaxPathBufLen, &return_count)) {
      if (IsRemovable(volume_path))
        result.push_back(base::FilePath(volume_path));
    } else {
      DPLOG(ERROR);
    }
    if (!FindNextVolume(find_handle, WriteInto(&volume_name, kMaxPathBufLen),
                        kMaxPathBufLen)) {
      if (GetLastError() != ERROR_NO_MORE_FILES)
        DPLOG(ERROR);
      break;
    }
  }

  FindVolumeClose(find_handle);
  return result;
}

}  // namespace

namespace chrome {

const int kWorkerPoolNumThreads = 3;
const char* kWorkerPoolNamePrefix = "DeviceInfoPool";

VolumeMountWatcherWin::VolumeMountWatcherWin()
    : device_info_worker_pool_(new base::SequencedWorkerPool(
          kWorkerPoolNumThreads, kWorkerPoolNamePrefix)),
      weak_factory_(this),
      notifications_(NULL) {
  get_attached_devices_callback_ = base::Bind(&GetAttachedDevices);
  get_device_details_callback_ = base::Bind(&GetDeviceDetails);
}

// static
base::FilePath VolumeMountWatcherWin::DriveNumberToFilePath(int drive_number) {
  if (drive_number < 0 || drive_number > 25)
    return base::FilePath();
  string16 path(L"_:\\");
  path[0] = L'A' + drive_number;
  return base::FilePath(path);
}

// In order to get all the weak pointers created on the UI thread, and doing
// synchronous Windows calls in the worker pool, this kicks off a chain of
// events which will
// a) Enumerate attached devices
// b) Create weak pointers for which to send completion signals from
// c) Retrieve metadata on the volumes and then
// d) Notify that metadata to listeners.
void VolumeMountWatcherWin::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // When VolumeMountWatcherWin is created, the message pumps are not running
  // so a posted task from the constructor would never run. Therefore, do all
  // the initializations here.
  device_info_worker_pool_->PostTask(FROM_HERE, base::Bind(
      &FindExistingDevicesAndAdd, get_attached_devices_callback_,
      weak_factory_.GetWeakPtr()));
}

// static
void VolumeMountWatcherWin::FindExistingDevicesAndAdd(
    base::Callback<std::vector<base::FilePath>(void)>
        get_attached_devices_callback,
    base::WeakPtr<chrome::VolumeMountWatcherWin> volume_watcher) {
  std::vector<base::FilePath> removable_devices =
      get_attached_devices_callback.Run();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &chrome::VolumeMountWatcherWin::AddDevicesOnUIThread,
      volume_watcher, removable_devices));
}

void VolumeMountWatcherWin::AddDevicesOnUIThread(
    std::vector<base::FilePath> removable_devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<base::TaskRunner> runner =
      device_info_worker_pool_->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  for (size_t i = 0; i < removable_devices.size(); i++) {
    if (ContainsKey(pending_device_checks_, removable_devices[i]))
      continue;
    pending_device_checks_.insert(removable_devices[i]);
    runner->PostTask(FROM_HERE,
                     base::Bind(&RetrieveInfoForDeviceAndAdd,
                                removable_devices[i],
                                get_device_details_callback_,
                                weak_factory_.GetWeakPtr()));
  }
}

// static
void VolumeMountWatcherWin::RetrieveInfoForDeviceAndAdd(
    const base::FilePath& device_path,
    base::Callback<bool(const base::FilePath&, string16*, std::string*,
                        string16*, bool*, uint64*)> get_device_details_callback,
    base::WeakPtr<chrome::VolumeMountWatcherWin> volume_watcher) {
  string16 device_location;
  std::string unique_id;
  string16 device_name;
  bool removable;
  uint64 total_size_in_bytes;
  if (!get_device_details_callback.Run(device_path, &device_location,
                                       &unique_id, &device_name, &removable,
                                       &total_size_in_bytes)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &chrome::VolumeMountWatcherWin::DeviceCheckComplete,
        volume_watcher, device_path));
    return;
  }

  chrome::MediaStorageUtil::Type type =
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
  if (chrome::IsMediaDevice(device_path.value()))
    type = chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM;
  std::string device_id =
      chrome::MediaStorageUtil::MakeDeviceId(type, unique_id);

  chrome::VolumeMountWatcherWin::MountPointInfo info;
  info.device_id = device_id;
  info.location = device_location;
  info.unique_id = unique_id;
  info.name = device_name;
  info.removable = removable;
  info.total_size_in_bytes = total_size_in_bytes;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &chrome::VolumeMountWatcherWin::HandleDeviceAttachEventOnUIThread,
      volume_watcher, device_path, info));
}

void VolumeMountWatcherWin::DeviceCheckComplete(
    const base::FilePath& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pending_device_checks_.erase(device_path);
}

bool VolumeMountWatcherWin::GetDeviceInfo(const base::FilePath& device_path,
    string16* device_location, std::string* unique_id, string16* name,
    bool* removable, uint64* total_size_in_bytes) const {
  base::FilePath path(device_path);
  MountPointDeviceMetadataMap::const_iterator iter =
      device_metadata_.find(path.value());
  while (iter == device_metadata_.end() && path.DirName() != path) {
    path = path.DirName();
    iter = device_metadata_.find(path.value());
  }

  // If the requested device hasn't been scanned yet,
  // synchronously get the device info.
  if (iter == device_metadata_.end()) {
    return get_device_details_callback_.Run(device_path, device_location,
                                            unique_id, name, removable,
                                            total_size_in_bytes);
  }

  if (device_location)
    *device_location = iter->second.location;
  if (unique_id)
    *unique_id = iter->second.unique_id;
  if (name)
    *name = iter->second.name;
  if (removable)
    *removable = iter->second.removable;
  if (total_size_in_bytes)
    *total_size_in_bytes = iter->second.total_size_in_bytes;

  return true;
}

uint64 VolumeMountWatcherWin::GetStorageSize(
    const base::FilePath::StringType& mount_point) const {
  MountPointDeviceMetadataMap::const_iterator iter =
      device_metadata_.find(mount_point);
  if (iter != device_metadata_.end())
    return iter->second.total_size_in_bytes;
  return 0;
}

void VolumeMountWatcherWin::OnWindowMessage(UINT event_type, LPARAM data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (event_type) {
    case DBT_DEVICEARRIVAL: {
      if (IsLogicalVolumeStructure(data)) {
        DWORD unitmask = GetVolumeBitMaskFromBroadcastHeader(data);
        std::vector<base::FilePath> paths;
        for (int i = 0; unitmask; ++i, unitmask >>= 1) {
          if (!(unitmask & 0x01))
            continue;
          paths.push_back(DriveNumberToFilePath(i));
        }
        AddDevicesOnUIThread(paths);
      }
      break;
    }
    case DBT_DEVICEREMOVECOMPLETE: {
      if (IsLogicalVolumeStructure(data)) {
        DWORD unitmask = GetVolumeBitMaskFromBroadcastHeader(data);
        for (int i = 0; unitmask; ++i, unitmask >>= 1) {
          if (!(unitmask & 0x01))
            continue;
          HandleDeviceDetachEventOnUIThread(DriveNumberToFilePath(i).value());
        }
      }
      break;
    }
  }
}

void VolumeMountWatcherWin::SetNotifications(
    StorageMonitor::Receiver* notifications) {
  notifications_ = notifications;
}

VolumeMountWatcherWin::~VolumeMountWatcherWin() {
  weak_factory_.InvalidateWeakPtrs();
}

void VolumeMountWatcherWin::HandleDeviceAttachEventOnUIThread(
    const base::FilePath& device_path,
    const MountPointInfo& info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  device_metadata_[device_path.value()] = info;

  DeviceCheckComplete(device_path);

  // Don't call removable storage observers for fixed volumes.
  if (!info.removable)
    return;

  if (notifications_) {
    string16 display_name =
        GetDisplayNameForDevice(info.total_size_in_bytes, info.name);
    notifications_->ProcessAttach(StorageMonitor::StorageInfo(
        info.device_id, display_name, device_path.value()));
  }
}

void VolumeMountWatcherWin::HandleDeviceDetachEventOnUIThread(
    const string16& device_location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MountPointDeviceMetadataMap::const_iterator device_info =
      device_metadata_.find(device_location);
  // If the device isn't type removable (like a CD), it won't be there.
  if (device_info == device_metadata_.end())
    return;

  if (notifications_)
    notifications_->ProcessDetach(device_info->second.device_id);
  device_metadata_.erase(device_info);
}

}  // namespace chrome
