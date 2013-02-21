// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains a subclass of RemovableDeviceNotificationsWindowWin to
// simulate device changed events for testing.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_TEST_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
#define CHROME_BROWSER_STORAGE_MONITOR_TEST_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_

#include <windows.h>

#include "base/memory/ref_counted.h"
#include "chrome/browser/storage_monitor/removable_device_notifications_window_win.h"

namespace chrome {
namespace test {

class TestPortableDeviceWatcherWin;
class TestVolumeMountWatcherWin;

class TestRemovableDeviceNotificationsWindowWin
    : public RemovableDeviceNotificationsWindowWin {
 public:
  TestRemovableDeviceNotificationsWindowWin(
      TestVolumeMountWatcherWin* volume_mount_watcher,
      TestPortableDeviceWatcherWin* portable_device_watcher);

  virtual ~TestRemovableDeviceNotificationsWindowWin();

  void InjectDeviceChange(UINT event_type, DWORD data);

  VolumeMountWatcherWin* volume_mount_watcher();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRemovableDeviceNotificationsWindowWin);
};

}  // namespace test
}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_TEST_REMOVABLE_DEVICE_NOTIFICATIONS_WINDOW_WIN_H_
