// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/removable_device_notifications_window_win.h"

#include <windows.h>
#include <dbt.h>
#include <fileapi.h>

#include "base/file_path.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "base/win/wrapped_window_proc.h"
#include "chrome/browser/system_monitor/media_device_notifications_utils.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "content/public/browser/browser_thread.h"

using base::SystemMonitor;
using base::win::WrappedWindowProc;
using content::BrowserThread;

namespace {

const DWORD kMaxPathBufLen = MAX_PATH + 1;

const char16 kWindowClassName[] = L"Chrome_RemovableDeviceNotificationWindow";

static chrome::RemovableDeviceNotificationsWindowWin*
    g_removable_device_notifications_window_win = NULL;

// The following msdn blog entry is helpful for understanding disk volumes
// and how they are treated in Windows:
// http://blogs.msdn.com/b/adioltean/archive/2005/04/16/408947.aspx
bool GetDeviceInfo(const FilePath& device_path, string16* device_location,
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

  if (name) {
    char16 volume_name[kMaxPathBufLen];
    if (!GetVolumeInformation(mount_point, volume_name, kMaxPathBufLen, NULL,
                              NULL, NULL, NULL, 0)) {
      return false;
    }
    if (wcslen(volume_name) > 0) {
      *name = string16(volume_name);
    } else {
      *name = device_path.LossyDisplayName();
    }
  }

  if (removable)
    *removable = (GetDriveType(mount_point) == DRIVE_REMOVABLE);

  return true;
}

std::vector<FilePath> GetAttachedDevices() {
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
      if (GetDriveType(volume_path) == DRIVE_REMOVABLE)
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

// Returns 0 if the devicetype is not volume.
uint32 GetVolumeBitMaskFromBroadcastHeader(LPARAM data) {
  DEV_BROADCAST_VOLUME* dev_broadcast_volume =
      reinterpret_cast<DEV_BROADCAST_VOLUME*>(data);
  if (dev_broadcast_volume->dbcv_devicetype == DBT_DEVTYP_VOLUME)
    return dev_broadcast_volume->dbcv_unitmask;
  return 0;
}

FilePath DriveNumberToFilePath(int drive_number) {
  string16 path(L"_:\\");
  path[0] = L'A' + drive_number;
  return FilePath(path);
}

}  // namespace

namespace chrome {

RemovableDeviceNotificationsWindowWin::RemovableDeviceNotificationsWindowWin()
    : window_class_(0),
      instance_(NULL),
      window_(NULL),
      get_device_info_func_(&GetDeviceInfo) {
  DCHECK(!g_removable_device_notifications_window_win);
  g_removable_device_notifications_window_win = this;
}

// static
RemovableDeviceNotificationsWindowWin*
RemovableDeviceNotificationsWindowWin::GetInstance() {
  DCHECK(g_removable_device_notifications_window_win);
  return g_removable_device_notifications_window_win;
}

void RemovableDeviceNotificationsWindowWin::Init() {
  DoInit(&GetAttachedDevices);
}

bool RemovableDeviceNotificationsWindowWin::GetDeviceInfoForPath(
    const FilePath& path,
    base::SystemMonitor::RemovableStorageInfo* device_info) {
  string16 location;
  std::string unique_id;
  string16 name;
  bool removable;
  if (!get_device_info_func_(path, &location, &unique_id, &name, &removable))
    return false;

  // To compute the device id, the device type is needed.  For removable
  // devices, that requires knowing if there's a DCIM directory, which would
  // require bouncing over to the file thread.  Instead, just iterate the
  // devices in SystemMonitor.
  std::string device_id;
  if (removable) {
    std::vector<SystemMonitor::RemovableStorageInfo> attached_devices =
        SystemMonitor::Get()->GetAttachedRemovableStorage();
    bool found = false;
    for (size_t i = 0; i < attached_devices.size(); i++) {
      MediaStorageUtil::Type type;
      std::string id;
      MediaStorageUtil::CrackDeviceId(attached_devices[i].device_id, &type,
                                      &id);
      if (id == unique_id) {
        found = true;
        device_id = attached_devices[i].device_id;
        break;
      }
    }
    if (!found)
      return false;
  } else {
    device_id = MediaStorageUtil::MakeDeviceId(
        MediaStorageUtil::FIXED_MASS_STORAGE, unique_id);
  }

  if (device_info) {
    device_info->device_id = device_id;
    device_info->name = name;
    device_info->location = location;
  }
  return true;
}

void RemovableDeviceNotificationsWindowWin::InitForTest(
    GetDeviceInfoFunc get_device_info_func,
    GetAttachedDevicesFunc get_attached_devices_func) {
  get_device_info_func_ = get_device_info_func;
  DoInit(get_attached_devices_func);
}

void RemovableDeviceNotificationsWindowWin::OnDeviceChange(UINT event_type,
                                                           LPARAM data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (event_type) {
    case DBT_DEVICEARRIVAL: {
      DWORD unitmask = GetVolumeBitMaskFromBroadcastHeader(data);
      for (int i = 0; unitmask; ++i, unitmask >>= 1) {
        if (!(unitmask & 0x01))
          continue;
        AddNewDevice(DriveNumberToFilePath(i));
      }
      break;
    }
    case DBT_DEVICEREMOVECOMPLETE: {
      DWORD unitmask = GetVolumeBitMaskFromBroadcastHeader(data);
      for (int i = 0; unitmask; ++i, unitmask >>= 1) {
        if (!(unitmask & 0x01))
          continue;

        FilePath device = DriveNumberToFilePath(i);
        MountPointDeviceIdMap::const_iterator device_info =
            device_ids_.find(device);
        // If the devices isn't type removable (like a CD), it won't be there.
        if (device_info == device_ids_.end())
          continue;

        SystemMonitor::Get()->ProcessRemovableStorageDetached(
            device_info->second);
        device_ids_.erase(device_info);
      }
      break;
    }
  }
}

RemovableDeviceNotificationsWindowWin::
    ~RemovableDeviceNotificationsWindowWin() {
  if (window_)
    DestroyWindow(window_);

  if (window_class_)
    UnregisterClass(MAKEINTATOM(window_class_), instance_);

  DCHECK_EQ(this, g_removable_device_notifications_window_win);
  g_removable_device_notifications_window_win = NULL;
}

// static
LRESULT CALLBACK RemovableDeviceNotificationsWindowWin::WndProcThunk(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  RemovableDeviceNotificationsWindowWin* msg_wnd =
      reinterpret_cast<RemovableDeviceNotificationsWindowWin*>(
          GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (msg_wnd)
    return msg_wnd->WndProc(hwnd, message, wparam, lparam);
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK RemovableDeviceNotificationsWindowWin::WndProc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_DEVICECHANGE:
      OnDeviceChange(static_cast<UINT>(wparam), lparam);
      return TRUE;
    default:
      break;
  }

  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

void RemovableDeviceNotificationsWindowWin::DoInit(
    GetAttachedDevicesFunc get_attached_devices_func) {
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      kWindowClassName,
      &WrappedWindowProc<RemovableDeviceNotificationsWindowWin::WndProcThunk>,
      0, 0, 0, NULL, NULL, NULL, NULL, NULL,
      &window_class);
  instance_ = window_class.hInstance;
  window_class_ = RegisterClassEx(&window_class);
  DCHECK(window_class_);

  window_ = CreateWindow(MAKEINTATOM(window_class_), 0, 0, 0, 0, 0, 0, 0, 0,
                         instance_, 0);
  SetWindowLongPtr(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

  std::vector<FilePath> removable_devices = get_attached_devices_func();
  for (size_t i = 0; i < removable_devices.size(); i++)
    AddNewDevice(removable_devices[i]);
}

void RemovableDeviceNotificationsWindowWin::AddNewDevice(
    const FilePath& device_path) {
  std::string unique_id;
  string16 device_name;
  bool removable;
  if (!get_device_info_func_(device_path, NULL, &unique_id, &device_name,
                             &removable)) {
    return;
  }

  if (!removable)
    return;

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &RemovableDeviceNotificationsWindowWin::CheckDeviceTypeOnFileThread,
          this, unique_id, device_name, device_path));
}

void RemovableDeviceNotificationsWindowWin::CheckDeviceTypeOnFileThread(
    const std::string& unique_id,
    const FilePath::StringType& device_name,
    const FilePath& device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  MediaStorageUtil::Type type =
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
  if (IsMediaDevice(device.value()))
    type = MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM;
  std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &RemovableDeviceNotificationsWindowWin::ProcessDeviceAttachedOnUIThread,
      this, device_id, device_name, device));
}

void RemovableDeviceNotificationsWindowWin::ProcessDeviceAttachedOnUIThread(
    const std::string& device_id,
    const FilePath::StringType& device_name,
    const FilePath& device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  device_ids_[device] = device_id;
  SystemMonitor::Get()->ProcessRemovableStorageAttached(device_id,
                                                        device_name,
                                                        device.value());
}

}  // namespace chrome
