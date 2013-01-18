// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/volume_mount_watcher_win.h"

#include <windows.h>
#include <dbt.h>
#include <fileapi.h>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/media_device_notifications_utils.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "content/public/browser/browser_thread.h"

using base::SystemMonitor;
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

// Gets mass storage device information given a |device_path|. On success,
// returns true and fills in |device_location|, |unique_id|, |name|, and
// |removable|.
// The following msdn blog entry is helpful for understanding disk volumes
// and how they are treated in Windows:
// http://blogs.msdn.com/b/adioltean/archive/2005/04/16/408947.aspx.
bool GetDeviceDetails(const FilePath& device_path, string16* device_location,
    std::string* unique_id, string16* name, bool* removable) {
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

  if (name)
    *name = device_path.LossyDisplayName();

  return true;
}

}  // namespace

namespace chrome {

VolumeMountWatcherWin::VolumeMountWatcherWin() {
}

// static
FilePath VolumeMountWatcherWin::DriveNumberToFilePath(int drive_number) {
  if (drive_number < 0 || drive_number > 25)
    return FilePath();
  string16 path(L"_:\\");
  path[0] = L'A' + drive_number;
  return FilePath(path);
}

void VolumeMountWatcherWin::Init() {
  // When VolumeMountWatcherWin is created, the message pumps are not running
  // so a posted task from the constructor would never run. Therefore, do all
  // the initializations here.
  //
  // This should call AddExistingDevicesOnFileThread. The call is disabled
  // until a fix for http://crbug.com/155910 can land.
  // BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
  //     &VolumeMountWatcherWin::AddExistingDevicesOnFileThread, this));
}

bool VolumeMountWatcherWin::GetDeviceInfo(const FilePath& device_path,
    string16* device_location, std::string* unique_id, string16* name,
    bool* removable) {
  return GetDeviceDetails(device_path, device_location, unique_id, name,
                          removable);
}

void VolumeMountWatcherWin::OnWindowMessage(UINT event_type, LPARAM data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (event_type) {
    case DBT_DEVICEARRIVAL: {
      if (IsLogicalVolumeStructure(data)) {
        DWORD unitmask = GetVolumeBitMaskFromBroadcastHeader(data);
        for (int i = 0; unitmask; ++i, unitmask >>= 1) {
          if (!(unitmask & 0x01))
            continue;
          AddNewDevice(DriveNumberToFilePath(i));
        }
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

VolumeMountWatcherWin::~VolumeMountWatcherWin() {
}

std::vector<FilePath> VolumeMountWatcherWin::GetAttachedDevices() {
  std::vector<FilePath> result;
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
        result.push_back(FilePath(volume_path));
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

void VolumeMountWatcherWin::AddNewDevice(const FilePath& device_path) {
  std::string unique_id;
  string16 device_name;
  bool removable;
  if (!GetDeviceInfo(device_path, NULL, &unique_id, &device_name, &removable))
    return;

  if (!removable)
    return;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&VolumeMountWatcherWin::CheckDeviceTypeOnFileThread, this,
                 unique_id, device_name, device_path));
}

void VolumeMountWatcherWin::AddExistingDevicesOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<FilePath> removable_devices = GetAttachedDevices();
  for (size_t i = 0; i < removable_devices.size(); i++)
    AddNewDevice(removable_devices[i]);
}

void VolumeMountWatcherWin::CheckDeviceTypeOnFileThread(
    const std::string& unique_id, const string16& device_name,
    const FilePath& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  MediaStorageUtil::Type type =
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
  if (IsMediaDevice(device_path.value()))
    type = MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM;
  std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &VolumeMountWatcherWin::HandleDeviceAttachEventOnUIThread, this,
      device_id, device_name, device_path));
}

void VolumeMountWatcherWin::HandleDeviceAttachEventOnUIThread(
    const std::string& device_id, const string16& device_name,
    const FilePath& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Remember metadata for this device.
  MountPointInfo info;
  info.device_id = device_id;
  device_metadata_[device_path.value()] = info;

  SystemMonitor* monitor = SystemMonitor::Get();
  if (monitor) {
    string16 display_name = GetDisplayNameForDevice(0, device_name);
    monitor->ProcessRemovableStorageAttached(device_id, display_name,
                                             device_path.value());
  }
}

void VolumeMountWatcherWin::HandleDeviceDetachEventOnUIThread(
    const string16& device_location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MountPointDeviceMetadataMap::const_iterator device_info =
      device_metadata_.find(device_location);
  // If the devices isn't type removable (like a CD), it won't be there.
  if (device_info == device_metadata_.end())
    return;

  SystemMonitor* monitor = SystemMonitor::Get();
  if (monitor)
    monitor->ProcessRemovableStorageDetached(device_info->second.device_id);
  device_metadata_.erase(device_info);
}

}  // namespace chrome
