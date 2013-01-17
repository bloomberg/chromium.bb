// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/system_monitor/removable_storage_notifications.h"

class FilePath;

namespace chrome {

class PortableDeviceWatcherWin;
class VolumeMountWatcherWin;

class RemovableDeviceNotificationsWindowWin
    : public RemovableStorageNotifications {
 public:
  // Creates an instance of RemovableDeviceNotificationsWindowWin. Should only
  // be called by browser start up code. Use GetInstance() instead.
  static RemovableDeviceNotificationsWindowWin* Create();

  virtual ~RemovableDeviceNotificationsWindowWin();

  // Must be called after the file thread is created.
  void Init();

  // RemovableStorageNotifications:
  virtual bool GetDeviceInfoForPath(
      const FilePath& path,
      base::SystemMonitor::RemovableStorageInfo* device_info) const OVERRIDE;
  virtual uint64 GetStorageSize(const std::string& location) const OVERRIDE;
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const OVERRIDE;

 private:
  class PortableDeviceNotifications;
  friend class TestRemovableDeviceNotificationsWindowWin;

  // To support unit tests, this constructor takes |volume_mount_watcher| and
  // |portable_device_watcher| objects. These params are either constructed in
  // unit tests or in RemovableDeviceNotificationsWindowWin::Create() function.
  RemovableDeviceNotificationsWindowWin(
      VolumeMountWatcherWin* volume_mount_watcher,
      PortableDeviceWatcherWin* portable_device_watcher);

  // Gets the removable storage information given a |device_path|. On success,
  // returns true and fills in |device_location|, |unique_id|, |name| and
  // |removable|.
  bool GetDeviceInfo(const FilePath& device_path,
                     string16* device_location,
                     std::string* unique_id,
                     string16* name,
                     bool* removable) const;

  static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT message, WPARAM wparam,
                                       LPARAM lparam);

  LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                           LPARAM lparam);

  void OnDeviceChange(UINT event_type, LPARAM data);

  // The window class of |window_|.
  ATOM window_class_;

  // The handle of the module that contains the window procedure of |window_|.
  HMODULE instance_;
  HWND window_;

  // The volume mount point watcher, used to manage the mounted devices.
  scoped_refptr<VolumeMountWatcherWin> volume_mount_watcher_;

  // The portable device watcher, used to manage media transfer protocol
  // devices.
  scoped_ptr<PortableDeviceWatcherWin> portable_device_watcher_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationsWindowWin);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
