// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/removable_device_notifications_window_win.h"

#include <windows.h>
#include <dbt.h>
#include <fileapi.h>

#include "base/win/wrapped_window_proc.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/portable_device_watcher_win.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "chrome/browser/storage_monitor/volume_mount_watcher_win.h"

namespace chrome {

namespace {

const char16 kWindowClassName[] = L"Chrome_RemovableDeviceNotificationWindow";

}  // namespace


// RemovableDeviceNotificationsWindowWin --------------------------------------

// static
RemovableDeviceNotificationsWindowWin*
    RemovableDeviceNotificationsWindowWin::Create() {
  return new RemovableDeviceNotificationsWindowWin(
      new VolumeMountWatcherWin(), new PortableDeviceWatcherWin());
}

RemovableDeviceNotificationsWindowWin::
    RemovableDeviceNotificationsWindowWin(
    VolumeMountWatcherWin* volume_mount_watcher,
    PortableDeviceWatcherWin* portable_device_watcher)
    : window_class_(0),
      instance_(NULL),
      window_(NULL),
      volume_mount_watcher_(volume_mount_watcher),
      portable_device_watcher_(portable_device_watcher) {
  DCHECK(volume_mount_watcher_);
  DCHECK(portable_device_watcher_);
  volume_mount_watcher_->SetNotifications(receiver());
  portable_device_watcher_->SetNotifications(receiver());
}

RemovableDeviceNotificationsWindowWin::
    ~RemovableDeviceNotificationsWindowWin() {
  volume_mount_watcher_->SetNotifications(NULL);
  portable_device_watcher_->SetNotifications(NULL);

  if (window_)
    DestroyWindow(window_);

  if (window_class_)
    UnregisterClass(MAKEINTATOM(window_class_), instance_);
}

void RemovableDeviceNotificationsWindowWin::Init() {
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      kWindowClassName,
      &base::win::WrappedWindowProc<
          RemovableDeviceNotificationsWindowWin::WndProcThunk>,
      0, 0, 0, NULL, NULL, NULL, NULL, NULL,
      &window_class);
  instance_ = window_class.hInstance;
  window_class_ = RegisterClassEx(&window_class);
  DCHECK(window_class_);

  window_ = CreateWindow(MAKEINTATOM(window_class_), 0, 0, 0, 0, 0, 0, 0, 0,
                         instance_, 0);
  SetWindowLongPtr(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  volume_mount_watcher_->Init();
  portable_device_watcher_->Init(window_);
}

bool RemovableDeviceNotificationsWindowWin::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  string16 location;
  std::string unique_id;
  string16 name;
  bool removable;
  uint64 total_size_in_bytes;
  if (!GetDeviceInfo(path, &location, &unique_id, &name, &removable,
                     &total_size_in_bytes)) {
    return false;
  }

  // To compute the device id, the device type is needed.  For removable
  // devices, that requires knowing if there's a DCIM directory, which would
  // require bouncing over to the file thread.  Instead, just iterate the
  // devices.
  std::string device_id;
  if (removable) {
    std::vector<StorageInfo> attached_devices = GetAttachedStorage();
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

uint64 RemovableDeviceNotificationsWindowWin::GetStorageSize(
    const base::FilePath::StringType& location) const {
  return volume_mount_watcher_->GetStorageSize(location);
}

bool RemovableDeviceNotificationsWindowWin::GetMTPStorageInfoFromDeviceId(
    const std::string& storage_device_id,
    string16* device_location,
    string16* storage_object_id) const {
  MediaStorageUtil::Type type;
  MediaStorageUtil::CrackDeviceId(storage_device_id, &type, NULL);
  return ((type == MediaStorageUtil::MTP_OR_PTP) &&
      portable_device_watcher_->GetMTPStorageInfoFromDeviceId(
          storage_device_id, device_location, storage_object_id));
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

bool RemovableDeviceNotificationsWindowWin::GetDeviceInfo(
    const base::FilePath& device_path, string16* device_location,
    std::string* unique_id, string16* name, bool* removable,
    uint64* total_size_in_bytes) const {
  // TODO(kmadhusu) Implement PortableDeviceWatcherWin::GetDeviceInfo()
  // function when we have the functionality to add a sub directory of
  // portable device as a media gallery.
  return volume_mount_watcher_->GetDeviceInfo(device_path, device_location,
                                              unique_id, name, removable,
                                              total_size_in_bytes);
}

void RemovableDeviceNotificationsWindowWin::OnDeviceChange(UINT event_type,
                                                           LPARAM data) {
  volume_mount_watcher_->OnWindowMessage(event_type, data);
  portable_device_watcher_->OnWindowMessage(event_type, data);
}

}  // namespace chrome
