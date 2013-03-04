// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_WIN_H_
#define CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_WIN_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"

namespace base {
class FilePath;
}

namespace chrome {

namespace test {
class TestStorageMonitorWin;
}

class PortableDeviceWatcherWin;
class VolumeMountWatcherWin;

class StorageMonitorWin : public StorageMonitor {
 public:
  // Creates an instance of StorageMonitorWin. Should only be called by browser
  // start up code. Use GetInstance() instead.
  static StorageMonitorWin* Create();

  virtual ~StorageMonitorWin();

  // Must be called after the file thread is created.
  void Init();

  // StorageMonitor:
  virtual bool GetStorageInfoForPath(const base::FilePath& path,
                                     StorageInfo* device_info) const OVERRIDE;
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const OVERRIDE;

  virtual uint64 GetStorageSize(
      const base::FilePath::StringType& location) const OVERRIDE;

 private:
  class PortableDeviceNotifications;
  friend class test::TestStorageMonitorWin;

  // To support unit tests, this constructor takes |volume_mount_watcher| and
  // |portable_device_watcher| objects. These params are either constructed in
  // unit tests or in StorageMonitorWin::Create() function.
  StorageMonitorWin(VolumeMountWatcherWin* volume_mount_watcher,
                    PortableDeviceWatcherWin* portable_device_watcher);

  // Gets the removable storage information given a |device_path|. On success,
  // returns true and fills in |device_location|, |unique_id|, |name| and
  // |removable|, and |total_size_in_bytes|.
  bool GetDeviceInfo(const base::FilePath& device_path,
                     string16* device_location,
                     std::string* unique_id,
                     string16* name,
                     bool* removable,
                     uint64* total_size_in_bytes) const;

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
  scoped_ptr<VolumeMountWatcherWin> volume_mount_watcher_;

  // The portable device watcher, used to manage media transfer protocol
  // devices.
  scoped_ptr<PortableDeviceWatcherWin> portable_device_watcher_;

  DISALLOW_COPY_AND_ASSIGN(StorageMonitorWin);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_WIN_H_
