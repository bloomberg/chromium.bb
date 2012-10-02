// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_

#include <windows.h>

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/system_monitor/system_monitor.h"

namespace chrome {

// Gets device information given a |device_path|. On success, returns true and
// fills in |location|, |unique_id| and |removable|.
typedef bool (*GetDeviceInfoFunc)(const FilePath& device_path,
                                  string16* location,
                                  std::string* unique_id,
                                  bool* removable);

// Gets the friendly name of the device given a |device_location|.
// |device_location| is the device mount point. E.g.: "G:\" or "C:\foo".
typedef string16 (*GetDeviceNameFunc)(const FilePath& device_location);

// Returns a vector of all the removable devices that are connected.
typedef std::vector<FilePath> (*GetAttachedDevicesFunc)();

class RemovableDeviceNotificationsWindowWin;
typedef RemovableDeviceNotificationsWindowWin RemovableDeviceNotifications;

class RemovableDeviceNotificationsWindowWin
    : public base::RefCountedThreadSafe<RemovableDeviceNotificationsWindowWin> {
 public:
  // Should only be called by browser start up code.  Use GetInstance() instead.
  RemovableDeviceNotificationsWindowWin();

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

 protected:
  // Only for use in unit tests.
  void InitForTest(GetDeviceInfoFunc getDeviceInfo,
                   GetDeviceNameFunc getDeviceName,
                   GetAttachedDevicesFunc getAttachedDevices);

  void OnDeviceChange(UINT event_type, LPARAM data);

 private:
  friend class
      base::RefCountedThreadSafe<RemovableDeviceNotificationsWindowWin>;
  friend class TestRemovableDeviceNotificationsWindowWin;

  typedef std::map<FilePath, std::string> MountPointDeviceIdMap;

  virtual ~RemovableDeviceNotificationsWindowWin();

  static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT message, WPARAM wparam,
                                       LPARAM lparam);

  LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                           LPARAM lparam);

  void DoInit(GetAttachedDevicesFunc get_attached_devices_func);

  void AddNewDevice(const FilePath& device_path);

  void AddExistingDevicesOnFileThread(
      GetAttachedDevicesFunc get_attached_devices_func);

  void CheckDeviceTypeOnFileThread(const std::string& unique_id,
                                   const FilePath& device);

  void ProcessDeviceAttachedOnUIThread(
      const std::string& device_id,
      const string16& device_name,
      const FilePath& device);

  // The window class of |window_|.
  ATOM window_class_;
  // The handle of the module that contains the window procedure of |window_|.
  HMODULE instance_;
  HWND window_;

  GetDeviceInfoFunc get_device_info_func_;
  GetDeviceNameFunc get_device_name_func_;

  // A map from device mount point to device id. Only accessed on the UI Thread.
  MountPointDeviceIdMap device_ids_;

  DISALLOW_COPY_AND_ASSIGN(RemovableDeviceNotificationsWindowWin);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
