// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestStorageMonitorWin implementation.

#include "chrome/browser/storage_monitor/test_storage_monitor_win.h"

#include "chrome/browser/storage_monitor/test_portable_device_watcher_win.h"
#include "chrome/browser/storage_monitor/test_volume_mount_watcher_win.h"

namespace chrome {
namespace test {

TestStorageMonitorWin::TestStorageMonitorWin(
    TestVolumeMountWatcherWin* volume_mount_watcher,
    TestPortableDeviceWatcherWin* portable_device_watcher)
    : StorageMonitorWin(volume_mount_watcher, portable_device_watcher) {
  DCHECK(volume_mount_watcher_);
  DCHECK(portable_device_watcher);
}

TestStorageMonitorWin::~TestStorageMonitorWin() {
}

void TestStorageMonitorWin::InjectDeviceChange(UINT event_type, DWORD data) {
  OnDeviceChange(event_type, data);
}

VolumeMountWatcherWin*
TestStorageMonitorWin::volume_mount_watcher() {
  return volume_mount_watcher_.get();
}

}  // namespace test
}  // namespace chrome
