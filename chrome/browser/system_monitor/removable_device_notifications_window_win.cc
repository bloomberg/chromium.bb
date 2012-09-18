// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/removable_device_notifications_window_win.h"

#include <windows.h>
#include <dbt.h>

#include <string>

#include "base/file_path.h"
#include "base/string_number_conversions.h"
#include "base/system_monitor/system_monitor.h"
#include "base/win/wrapped_window_proc.h"
#include "chrome/browser/system_monitor/media_device_notifications_utils.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "content/public/browser/browser_thread.h"

using base::SystemMonitor;
using content::BrowserThread;

namespace {

const wchar_t WindowClassName[] = L"Chrome_RemovableDeviceNotificationWindow";

LRESULT GetVolumeName(LPCWSTR drive,
                      LPWSTR volume_name,
                      unsigned int volume_name_len) {
  return GetVolumeInformation(drive, volume_name, volume_name_len, NULL, NULL,
      NULL, NULL, 0);
}

// Returns 0 if the devicetype is not volume.
DWORD GetVolumeBitMaskFromBroadcastHeader(DWORD data) {
  PDEV_BROADCAST_HDR dev_broadcast_hdr =
      reinterpret_cast<PDEV_BROADCAST_HDR>(data);
  if (dev_broadcast_hdr->dbch_devicetype == DBT_DEVTYP_VOLUME) {
    PDEV_BROADCAST_VOLUME dev_broadcast_volume =
        reinterpret_cast<PDEV_BROADCAST_VOLUME>(dev_broadcast_hdr);
    return dev_broadcast_volume->dbcv_unitmask;
  }
  return 0;
}

}  // namespace

namespace chrome {

RemovableDeviceNotificationsWindowWin::RemovableDeviceNotificationsWindowWin()
    : atom_(0),
      instance_(NULL),
      window_(NULL),
      volume_name_func_(&GetVolumeName) {
  Init();
}

RemovableDeviceNotificationsWindowWin::RemovableDeviceNotificationsWindowWin(
    VolumeNameFunc volume_name_func)
    : atom_(0),
      instance_(NULL),
      window_(NULL),
      volume_name_func_(volume_name_func) {
  Init();
}

void RemovableDeviceNotificationsWindowWin::Init() {
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      WindowClassName,
      &base::win::WrappedWindowProc<
          RemovableDeviceNotificationsWindowWin::WndProcThunk>,
      0, 0, 0, NULL, NULL, NULL, NULL, NULL,
      &window_class);
  instance_ = window_class.hInstance;
  atom_ = RegisterClassEx(&window_class);
  DCHECK(atom_);

  window_ = CreateWindow(MAKEINTATOM(atom_), 0, 0, 0, 0, 0, 0, 0, 0, instance_,
                         0);
  SetWindowLongPtr(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
}

RemovableDeviceNotificationsWindowWin::~RemovableDeviceNotificationsWindowWin(
    ) {
  if (window_)
    DestroyWindow(window_);

  if (atom_)
    UnregisterClass(MAKEINTATOM(atom_), instance_);
}

LRESULT RemovableDeviceNotificationsWindowWin::OnDeviceChange(UINT event_type,
                                                              DWORD data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (event_type) {
    case DBT_DEVICEARRIVAL: {
      DWORD unitmask = GetVolumeBitMaskFromBroadcastHeader(data);
      for (int i = 0; unitmask; ++i, unitmask >>= 1) {
        if (unitmask & 0x01) {
          FilePath::StringType drive(L"_:\\");
          drive[0] = L'A' + i;
          WCHAR volume_name[MAX_PATH + 1];
          if ((*volume_name_func_)(drive.c_str(), volume_name, MAX_PATH + 1)) {
            // TODO(kmadhusu) We need to look up a real device id as well as
            // having a fall back for volume name.
            std::string device_id = MediaStorageUtil::MakeDeviceId(
                MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM,
                base::IntToString(i));
            BrowserThread::PostTask(
                BrowserThread::FILE, FROM_HERE,
                base::Bind(&RemovableDeviceNotificationsWindowWin::
                    CheckDeviceTypeOnFileThread, this, device_id,
                    FilePath::StringType(volume_name), FilePath(drive)));
          }
        }
      }
      break;
    }
    case DBT_DEVICEREMOVECOMPLETE: {
      DWORD unitmask = GetVolumeBitMaskFromBroadcastHeader(data);
      for (int i = 0; unitmask; ++i, unitmask >>= 1) {
        if (unitmask & 0x01) {
          std::string device_id = MediaStorageUtil::MakeDeviceId(
              MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM,
              base::IntToString(i));
          SystemMonitor::Get()->ProcessRemovableStorageDetached(device_id);
        }
      }
      break;
    }
  }
  return TRUE;
}

void RemovableDeviceNotificationsWindowWin::CheckDeviceTypeOnFileThread(
    const std::string& id,
    const FilePath::StringType& device_name,
    const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!IsMediaDevice(path.value()))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &RemovableDeviceNotificationsWindowWin::
              ProcessRemovableDeviceAttachedOnUIThread,
          this, id, device_name, path));
}

void
RemovableDeviceNotificationsWindowWin::ProcessRemovableDeviceAttachedOnUIThread(
    const std::string& id,
    const FilePath::StringType& device_name,
    const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SystemMonitor::Get()->ProcessRemovableStorageAttached(id,
                                                        device_name,
                                                        path.value());
}

LRESULT CALLBACK RemovableDeviceNotificationsWindowWin::WndProc(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_DEVICECHANGE:
      return OnDeviceChange(static_cast<UINT>(wparam),
                            static_cast<DWORD>(lparam));
    default:
      break;
  }

  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

// static
LRESULT CALLBACK RemovableDeviceNotificationsWindowWin::WndProcThunk(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
  RemovableDeviceNotificationsWindowWin* msg_wnd =
      reinterpret_cast<RemovableDeviceNotificationsWindowWin*>(
          GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (msg_wnd)
    return msg_wnd->WndProc(hwnd, message, wparam, lparam);
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

}  // namespace chrome
