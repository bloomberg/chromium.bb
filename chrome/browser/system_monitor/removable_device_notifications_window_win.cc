// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/removable_device_notifications_window_win.h"

#include <windows.h>
#include <dbt.h>
#include <fileapi.h>
#include <setupapi.h>
#include <Winioctl.h>

#include "base/file_path.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
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

// On success, returns true and fills in |device_number| with the storage number
// of the device present at |path|.
// |path| can be a volume name path(E.g.: \\?\Volume{GUID})  or a
// device path(E.g.: \\?\usb#vid_FF&pid_000F#32&2&1#{abcd-1234-ffde-1112-9172})
bool GetDeviceNumberForDevice(const string16& path, int* device_number) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (path.empty())
    return false;

  base::win::ScopedHandle device_handle(
      CreateFile(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                 NULL, OPEN_EXISTING, 0, NULL));
  if (!device_handle.IsValid())
    return false;

  STORAGE_DEVICE_NUMBER storage_device_num;
  DWORD bytes_returned = 0;
  if (DeviceIoControl(device_handle, IOCTL_STORAGE_GET_DEVICE_NUMBER,
                      NULL, 0, &storage_device_num, sizeof(storage_device_num),
                      &bytes_returned, NULL)) {
    *device_number = static_cast<int>(storage_device_num.DeviceNumber);
    return true;
  }
  return false;
}

// Extracts volume guid path details for the device present at |mount_point|.
// On success, returns true and fills in |volume_guid_path|.
// E.g: If the |mount_point| is "C:\mount_point" then volume guid path is
// something similar to "\\?\Volume{GUID}\".
bool GetVolumeGUIDPathFromMountPoint(const string16& mount_point,
                                     string16* volume_guid_path) {
  char16 guid[kMaxPathBufLen];
  if (!GetVolumeNameForVolumeMountPoint(mount_point.c_str(), guid,
                                        kMaxPathBufLen)) {
    return false;
  }
  // In case it has two GUID's (see the blog
  // http://blogs.msdn.com/b/adioltean/archive/2005/04/16/408947.aspx), do it
  // again.
  if (!GetVolumeNameForVolumeMountPoint(guid, guid, kMaxPathBufLen))
    return false;

  *volume_guid_path = string16(guid);
  return true;
}

// On success, returns true and fills in |device_number| with the storage number
// of the device present at |mount_point|.
bool GetDeviceNumberFromMountPoint(const string16& mount_point,
                                   int* device_number) {
  if (mount_point.empty())
    return false;

  string16 volume_guid_path;
  if (!GetVolumeGUIDPathFromMountPoint(mount_point, &volume_guid_path))
    return false;

  size_t guid_path_len = volume_guid_path.length();

  if (!EndsWith(volume_guid_path, L"\\", false))
    return false;

  // Remove the trailing backslash from volume guid path.
  string16 volume_name_path = volume_guid_path.substr(0, guid_path_len - 1);
  return GetDeviceNumberForDevice(volume_name_path.c_str(), device_number);
}

// This is a thin wrapper around SetupDiGetDeviceRegistryProperty function.
// Returns false if unable to get |property_key| value.
bool GetDeviceRegistryPropertyValue(HDEVINFO device_info_set,
                                    SP_DEVINFO_DATA* device_info_data,
                                    DWORD property_key,
                                    string16* value) {
  // Get the size of the buffer.
  DWORD property_buffer_size = 0;
  if (!SetupDiGetDeviceRegistryProperty(device_info_set, device_info_data,
                                        property_key, NULL, NULL, NULL,
                                        &property_buffer_size)) {
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
      return false;
  }
  scoped_array<char16> property_buffer(new char16[property_buffer_size]);
  ZeroMemory(property_buffer.get(), property_buffer_size);
  if (!SetupDiGetDeviceRegistryProperty(
          device_info_set, device_info_data, property_key, NULL,
          reinterpret_cast<BYTE*>(property_buffer.get()),
          property_buffer_size,
          &property_buffer_size)) {
    return false;
  }

  *value = string16(property_buffer.get());
  return !value->empty();
}

// On success, returns the volume name of the device present at |mount_point|.
// or an empty string otherwise.
string16 GetVolumeNameFromMountPoint(const FilePath& mount_point) {
  string16 device_name;
  char16 volume_name[kMaxPathBufLen];
  if (GetVolumeInformation(mount_point.value().c_str(), volume_name,
                           kMaxPathBufLen, NULL, NULL, NULL, NULL, 0)) {
    device_name = string16(volume_name);
  }
  return device_name;
}

// On success, returns the friendly name of the device present at
// |mount_point| or an empty string otherwise.
string16 GetDeviceFriendlyNameFromMountPoint(const FilePath& mount_point) {
  string16 device_name;
  int device_number = 0;
  if (!GetDeviceNumberFromMountPoint(mount_point.value(), &device_number))
    return device_name;  // Unable to get the device number.

  // Query only about disk drives (CDROM's and Floppy's are not included).
  const GUID* guid = reinterpret_cast<const GUID *>(&GUID_DEVINTERFACE_DISK);

  // Get a "device information set" handle for all devices attached to the
  // system.
  HDEVINFO device_info_set = SetupDiGetClassDevs(
      guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

  if (device_info_set == INVALID_HANDLE_VALUE)
    return device_name;

  SP_DEVICE_INTERFACE_DATA interface_data;
  interface_data.cbSize = sizeof(interface_data);
  for (DWORD index = 0;
       SetupDiEnumDeviceInterfaces(device_info_set, NULL, guid, index,
                                   &interface_data); ++index) {
    // Get the interface detail size.
    DWORD interface_detail_size = 0;
    if (!SetupDiGetDeviceInterfaceDetail(device_info_set, &interface_data,
                                         NULL, 0, &interface_detail_size,
                                         NULL)) {
      if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        continue;
    }

    scoped_array<char> interface_detail_buffer(new char[interface_detail_size]);
    ZeroMemory(interface_detail_buffer.get(), interface_detail_size);
    SP_DEVICE_INTERFACE_DETAIL_DATA* interface_detail =
        reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(
            interface_detail_buffer.get());
    interface_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
    SP_DEVINFO_DATA device_info_data;
    device_info_data.cbSize = sizeof(device_info_data);

    // Retrieve a context structure for a device interface of a device
    // information set.
    if (!SetupDiGetDeviceInterfaceDetail(device_info_set, &interface_data,
                                         interface_detail,
                                         interface_detail_size,
                                         0,
                                         &device_info_data)) {
      continue;
    }

    int storage_device_number = 0;
    if (!GetDeviceNumberForDevice(interface_detail->DevicePath,
                                  &storage_device_number)) {
      continue;
    }

    if (device_number != storage_device_number) {
      // |device_info_data| does not correspond to the device present at
      // |device_location|.
      continue;
    }

    // Matching device info found. Get the friendly name of the device.
    bool success = GetDeviceRegistryPropertyValue(device_info_set,
                                                  &device_info_data,
                                                  SPDRP_FRIENDLYNAME,
                                                  &device_name);
    if (!success) {
      success = GetDeviceRegistryPropertyValue(device_info_set,
                                               &device_info_data,
                                               SPDRP_DEVICEDESC, &device_name);
    }
    break;
  }
  SetupDiDestroyDeviceInfoList(device_info_set);
  return device_name;
}

// Returns the display name of the device present at |mount_point|.
string16 GetDeviceDisplayName(const FilePath& mount_point) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  string16 device_name = GetVolumeNameFromMountPoint(mount_point);
  if (device_name.empty())
    device_name = GetDeviceFriendlyNameFromMountPoint(mount_point);

  if (device_name.empty()) {
    device_name = mount_point.LossyDisplayName();
  } else {
    // Append the mount point(E.g.: "H:\") to the friendly name
    // or volume name (E.g.: "USB Disk Device") to construct
    // "USB Disk Device (H:\)".
    device_name = base::StringPrintf(L"%ls (%ls)", device_name.c_str(),
                                     mount_point.LossyDisplayName().c_str());
  }
  return device_name;
}

// The following msdn blog entry is helpful for understanding disk volumes
// and how they are treated in Windows:
// http://blogs.msdn.com/b/adioltean/archive/2005/04/16/408947.aspx
bool GetDeviceInfo(const FilePath& device_path, string16* device_location,
                   std::string* unique_id, bool* removable) {
  char16 mount_point[kMaxPathBufLen];
  if (!GetVolumePathName(device_path.value().c_str(), mount_point,
                         kMaxPathBufLen)) {
    return false;
  }

  char16 fs_name[kMaxPathBufLen];
  if (!GetVolumeInformation(mount_point, NULL, NULL, NULL, NULL, NULL, fs_name,
                            kMaxPathBufLen)) {
      // Unknown file system.
      // E.g: When the user attaches a USB 4-in-1 memory card reader, Windows
      // displays the card reader as 4 removable devices. All the empty slots
      // volumes are shown as unknown file systems.
      return false;
  }

  if (device_location)
    *device_location = string16(mount_point);

  if (unique_id) {
    string16 guid;
    if (!GetVolumeGUIDPathFromMountPoint(mount_point, &guid))
      return false;
    *unique_id = UTF16ToUTF8(guid);
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
      get_device_info_func_(&GetDeviceInfo),
      get_device_name_func_(&GetDeviceDisplayName) {
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
  bool removable;
  if (!get_device_info_func_(path, &location, &unique_id, &removable))
    return false;

  // To compute the device id, the device type is needed.  For removable
  // devices, that requires knowing if there's a DCIM directory, which would
  // require bouncing over to the file thread.  Instead, just iterate the
  // devices in SystemMonitor.
  std::string device_id;
  string16 name;
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
        name = attached_devices[i].name;
        break;
      }
    }
    if (!found)
      return false;
  } else {
    device_id = MediaStorageUtil::MakeDeviceId(
        MediaStorageUtil::FIXED_MASS_STORAGE, unique_id);
    name = path.LossyDisplayName();
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
    GetDeviceNameFunc get_device_name_func,
    GetAttachedDevicesFunc get_attached_devices_func) {
  get_device_info_func_ = get_device_info_func;
  get_device_name_func_ = get_device_name_func;
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

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
      &RemovableDeviceNotificationsWindowWin::AddExistingDevicesOnFileThread,
      this, get_attached_devices_func));
}

void RemovableDeviceNotificationsWindowWin::AddNewDevice(
    const FilePath& device_path) {
  std::string unique_id;
  bool removable;
  if (!get_device_info_func_(device_path, NULL, &unique_id, &removable))
    return;

  if (!removable)
    return;

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &RemovableDeviceNotificationsWindowWin::CheckDeviceTypeOnFileThread,
          this, unique_id, device_path));
}

void RemovableDeviceNotificationsWindowWin::AddExistingDevicesOnFileThread(
    GetAttachedDevicesFunc get_attached_devices_func) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<FilePath> removable_devices = get_attached_devices_func();
  for (size_t i = 0; i < removable_devices.size(); i++)
    AddNewDevice(removable_devices[i]);
}

void RemovableDeviceNotificationsWindowWin::CheckDeviceTypeOnFileThread(
    const std::string& unique_id,
    const FilePath& device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  MediaStorageUtil::Type type =
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
  if (IsMediaDevice(device.value()))
    type = MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM;

  DCHECK(!unique_id.empty());
  std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);

  string16 device_name = get_device_name_func_(device);
  DCHECK(!device_name.empty());

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &RemovableDeviceNotificationsWindowWin::ProcessDeviceAttachedOnUIThread,
      this, device_id, device_name, device));
}

void RemovableDeviceNotificationsWindowWin::ProcessDeviceAttachedOnUIThread(
    const std::string& device_id,
    const string16& device_name,
    const FilePath& device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  device_ids_[device] = device_id;
  SystemMonitor::Get()->ProcessRemovableStorageAttached(device_id,
                                                        device_name,
                                                        device.value());
}

}  // namespace chrome
