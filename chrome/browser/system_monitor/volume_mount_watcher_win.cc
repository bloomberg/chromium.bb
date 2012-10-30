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

bool IsRemovable(const char16* mount_point) {
  if (GetDriveType(mount_point) != DRIVE_REMOVABLE)
    return false;

  // We don't consider floppy disks as removable, so check for that.
  string16 device = mount_point;
  if (EndsWith(device, L"\\", false))
    device = device.substr(0, device.length() - 1);
  char16 device_path[kMaxPathBufLen];
  if (!QueryDosDevice(device.c_str(), device_path, kMaxPathBufLen))
    return true;
  return string16(device_path).find(L"Floppy") == string16::npos;
}

// Returns 0 if the devicetype is not volume.
uint32 GetVolumeBitMaskFromBroadcastHeader(LPARAM data) {
  DEV_BROADCAST_VOLUME* dev_broadcast_volume =
      reinterpret_cast<DEV_BROADCAST_VOLUME*>(data);
  if (dev_broadcast_volume->dbcv_devicetype == DBT_DEVTYP_VOLUME)
    return dev_broadcast_volume->dbcv_unitmask;
  return 0;
}

FilePath DriveNumberToFilePath(int drive_number) {
  DCHECK_LT(drive_number, 26);
  string16 path(L"_:\\");
  path[0] = L'A' + drive_number;
  return FilePath(path);
}

// Returns true if |data| represents a logical volume structure.
bool IsLogicalVolumeStructure(LPARAM data) {
  DEV_BROADCAST_HDR* broadcast_hdr =
      reinterpret_cast<DEV_BROADCAST_HDR*>(data);
  return broadcast_hdr != NULL &&
         broadcast_hdr->dbch_devicetype == DBT_DEVTYP_VOLUME;
}

// Gets mass storage device information given a |device_path|. On success,
// returns true and fills in |device_location|, |unique_id|, |name| and
// |removable|.
// The following msdn blog entry is helpful for understanding disk volumes
// and how they are treated in Windows:
// http://blogs.msdn.com/b/adioltean/archive/2005/04/16/408947.aspx.
bool GetDeviceDetails(const FilePath& device_path, string16* device_location,
    std::string* unique_id, string16* name, bool* removable) {
  char16 mount_point[kMaxPathBufLen];
  if (!GetVolumePathName(device_path.value().c_str(), mount_point,
                         kMaxPathBufLen)) {
    return false;
  }
  if (device_location)
    *device_location = string16(mount_point);

  if (unique_id) {
    char16 guid[kMaxPathBufLen];
    if (!GetVolumeNameForVolumeMountPoint(mount_point, guid, kMaxPathBufLen))
      return false;
    // In case it has two GUID's (see above mentioned blog), do it again.
    if (!GetVolumeNameForVolumeMountPoint(guid, guid, kMaxPathBufLen))
      return false;
    WideToUTF8(guid, wcslen(guid), unique_id);
  }

  if (name)
    *name = device_path.LossyDisplayName();

  if (removable)
    *removable = IsRemovable(mount_point);

  return true;
}

}  // namespace

namespace chrome {

VolumeMountWatcherWin::VolumeMountWatcherWin() {
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
  char16 volume_name[kMaxPathBufLen];
  HANDLE find_handle = FindFirstVolume(volume_name, kMaxPathBufLen);
  if (find_handle == INVALID_HANDLE_VALUE)
    return result;

  while (true) {
    char16 volume_path[kMaxPathBufLen];
    DWORD return_count;
    if (GetVolumePathNamesForVolumeName(volume_name, volume_path,
                                        kMaxPathBufLen, &return_count)) {
      if (IsRemovable(volume_path))
        result.push_back(FilePath(volume_path));
    } else {
      DPLOG(ERROR);
    }
    if (!FindNextVolume(find_handle, volume_name, kMaxPathBufLen)) {
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
    const FilePath& device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  MediaStorageUtil::Type type =
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
  if (IsMediaDevice(device.value()))
    type = MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM;
  std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &VolumeMountWatcherWin::HandleDeviceAttachEventOnUIThread, this,
      device_id, device_name, device.value()));
}

void VolumeMountWatcherWin::HandleDeviceAttachEventOnUIThread(
    const std::string& device_id, const string16& device_name,
    const string16& device_location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  device_ids_[device_location] = device_id;
  SystemMonitor* monitor = SystemMonitor::Get();
  if (monitor) {
    monitor->ProcessRemovableStorageAttached(device_id, device_name,
                                             device_location);
  }
}

void VolumeMountWatcherWin::HandleDeviceDetachEventOnUIThread(
    const string16& device_location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MountPointDeviceIdMap::const_iterator device_info =
      device_ids_.find(device_location);
  // If the devices isn't type removable (like a CD), it won't be there.
  if (device_info == device_ids_.end())
    return;

  SystemMonitor* monitor = SystemMonitor::Get();
  if (monitor)
    monitor->ProcessRemovableStorageDetached(device_info->second);
  device_ids_.erase(device_info);
}

}  // namespace chrome
