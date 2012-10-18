// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/system_monitor/system_monitor.h"
#include "chrome/browser/system_monitor/volume_mount_watcher_win.h"

class FilePath;

namespace chrome {

class RemovableDeviceNotificationsWindowWin;
typedef RemovableDeviceNotificationsWindowWin RemovableDeviceNotifications;

class RemovableDeviceNotificationsWindowWin {
 public:
  // Creates an instance of RemovableDeviceNotificationsWindowWin. Should only
  // be called by browser start up code. Use GetInstance() instead.
  static RemovableDeviceNotificationsWindowWin* Create();

  virtual ~RemovableDeviceNotificationsWindowWin();

  // base::SystemMonitor has a lifetime somewhat shorter than a Singleton and
  // |this| is constructed/destroyed just after/before SystemMonitor.
  static RemovableDeviceNotificationsWindowWin* GetInstance();

  // Must be called after the file thread is created.
  void Init();

  // Finds the device that contains |path| and populates |device_info|.
  // Returns false if unable to find the device.
  bool GetDeviceInfoForPath(
      const FilePath& path,
      base::SystemMonitor::RemovableStorageInfo* device_info);

 private:
  friend class TestRemovableDeviceNotificationsWindowWin;

  explicit RemovableDeviceNotificationsWindowWin(
      VolumeMountWatcherWin* volume_mount_watcher);

  // Gets the removable storage information given a |device_path|. On success,
  // returns true and fills in |device_location|, |unique_id|, |name| and
  // |removable|.
  bool GetDeviceInfo(const FilePath& device_path,
                     string16* device_location,
                     std::string* unique_id,
                     string16* name,
                     bool* removable);

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

  // Store the volume mount point watcher to manage the mounted devices.
  scoped_refptr<VolumeMountWatcherWin> volume_mount_watcher_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationsWindowWin);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
