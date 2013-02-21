// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestRemovableDeviceNotificationsWindowWin implementation.

#include "chrome/browser/storage_monitor/test_removable_device_notifications_window_win.h"

#include "chrome/browser/storage_monitor/test_portable_device_watcher_win.h"
#include "chrome/browser/storage_monitor/test_volume_mount_watcher_win.h"

namespace chrome {
namespace test {

TestRemovableDeviceNotificationsWindowWin::
    TestRemovableDeviceNotificationsWindowWin(
    TestVolumeMountWatcherWin* volume_mount_watcher,
    TestPortableDeviceWatcherWin* portable_device_watcher)
    : RemovableDeviceNotificationsWindowWin(volume_mount_watcher,
                                            portable_device_watcher) {
  DCHECK(volume_mount_watcher_);
  DCHECK(portable_device_watcher);
}

TestRemovableDeviceNotificationsWindowWin::
    ~TestRemovableDeviceNotificationsWindowWin() {
}

void TestRemovableDeviceNotificationsWindowWin::InjectDeviceChange(
    UINT event_type,
    DWORD data) {
  OnDeviceChange(event_type, data);
}

VolumeMountWatcherWin*
TestRemovableDeviceNotificationsWindowWin::volume_mount_watcher() {
  return volume_mount_watcher_.get();
}

}  // namespace test
}  // namespace chrome
