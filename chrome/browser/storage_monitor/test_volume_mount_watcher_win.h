// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains a subclass of VolumeMountWatcherWin to expose some
// functionality for testing.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_
#define CHROME_BROWSER_STORAGE_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_

#include <string>
#include <vector>

#include "base/string16.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/storage_monitor/volume_mount_watcher_win.h"

namespace base {
class FilePath;
}

namespace chrome {
namespace test {

class TestVolumeMountWatcherWin : public VolumeMountWatcherWin {
 public:
  TestVolumeMountWatcherWin();
  virtual ~TestVolumeMountWatcherWin();

  void AddDeviceForTesting(const base::FilePath& device_path,
                           const std::string& device_id,
                           const std::string& unique_id,
                           const string16& device_name,
                           bool removable,
                           uint64 total_size_in_bytes);

  void SetAttachedDevicesFake();

  void FlushWorkerPoolForTesting();

  virtual void DeviceCheckComplete(const base::FilePath& device_path);

  std::vector<base::FilePath> devices_checked() const {
      return devices_checked_;
  }

  void BlockDeviceCheckForTesting();

  void ReleaseDeviceCheck();

  // VolumeMountWatcherWin:
  virtual bool GetDeviceInfo(const base::FilePath& device_path,
                             string16* device_location,
                             std::string* unique_id,
                             string16* name,
                             bool* removable,
                             uint64* total_size_in_bytes) const OVERRIDE;
  virtual std::vector<base::FilePath> GetAttachedDevices();

  bool GetRawDeviceInfo(const base::FilePath& device_path,
                        string16* device_location,
                        std::string* unique_id,
                        string16* name,
                        bool* removable,
                        uint64* total_size_in_bytes);

 private:
  std::vector<base::FilePath> devices_checked_;
  scoped_ptr<base::WaitableEvent> device_check_complete_event_;

  DISALLOW_COPY_AND_ASSIGN(TestVolumeMountWatcherWin);
};

}  // namespace test
}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_
