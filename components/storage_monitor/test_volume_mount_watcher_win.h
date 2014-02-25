// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains a subclass of VolumeMountWatcherWin to expose some
// functionality for testing.

#ifndef COMPONENTS_STORAGE_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_
#define COMPONENTS_STORAGE_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "components/storage_monitor/volume_mount_watcher_win.h"

namespace base {
class FilePath;
}

namespace storage_monitor {

class TestVolumeMountWatcherWin : public VolumeMountWatcherWin {
 public:
  TestVolumeMountWatcherWin();
  virtual ~TestVolumeMountWatcherWin();

  static bool GetDeviceRemovable(const base::FilePath& device_path,
                                 bool* removable);

  void AddDeviceForTesting(const base::FilePath& device_path,
                           const std::string& device_id,
                           const base::string16& device_name,
                           uint64 total_size_in_bytes);

  void SetAttachedDevicesFake();

  void FlushWorkerPoolForTesting();

  virtual void DeviceCheckComplete(const base::FilePath& device_path);

  const std::vector<base::FilePath>& devices_checked() const {
      return devices_checked_;
  }

  void BlockDeviceCheckForTesting();

  void ReleaseDeviceCheck();

  // VolumeMountWatcherWin:
  virtual GetAttachedDevicesCallbackType
      GetAttachedDevicesCallback() const OVERRIDE;
  virtual GetDeviceDetailsCallbackType
      GetDeviceDetailsCallback() const OVERRIDE;

  // Should be used by unit tests to make sure the worker pool doesn't survive
  // into other test runs.
  void ShutdownWorkerPool();

 private:
  std::vector<base::FilePath> devices_checked_;
  scoped_ptr<base::WaitableEvent> device_check_complete_event_;
  bool attached_devices_fake_;

  DISALLOW_COPY_AND_ASSIGN(TestVolumeMountWatcherWin);
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_
