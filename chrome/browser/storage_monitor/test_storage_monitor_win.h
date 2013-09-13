// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains a subclass of StorageMonitorWin to simulate device
// changed events for testing.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_TEST_STORAGE_MONITOR_WIN_H_
#define CHROME_BROWSER_STORAGE_MONITOR_TEST_STORAGE_MONITOR_WIN_H_

#include <windows.h>

#include "base/memory/ref_counted.h"
#include "chrome/browser/storage_monitor/storage_monitor_win.h"

class TestPortableDeviceWatcherWin;
class TestVolumeMountWatcherWin;

class TestStorageMonitorWin: public StorageMonitorWin {
 public:
  TestStorageMonitorWin(
      TestVolumeMountWatcherWin* volume_mount_watcher,
      TestPortableDeviceWatcherWin* portable_device_watcher);

  virtual ~TestStorageMonitorWin();

  void InjectDeviceChange(UINT event_type, DWORD data);

  VolumeMountWatcherWin* volume_mount_watcher();

  virtual Receiver* receiver() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestStorageMonitorWin);
};

#endif  // CHROME_BROWSER_STORAGE_MONITOR_TEST_STORAGE_MONITOR_WIN_H_
