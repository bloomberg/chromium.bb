// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains a subclass of VolumeMountWatcherWin to expose some
// functionality for testing.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_

#include <string>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/system_monitor/volume_mount_watcher_win.h"

class FilePath;

namespace chrome {
namespace test {

class TestVolumeMountWatcherWin : public VolumeMountWatcherWin {
 public:
  TestVolumeMountWatcherWin();

  void set_pre_attach_devices(bool pre_attach_devices) {
    pre_attach_devices_ = pre_attach_devices;
  }

  // VolumeMountWatcherWin:
  virtual bool GetDeviceInfo(const FilePath& device_path,
                             string16* device_location,
                             std::string* unique_id,
                             string16* name,
                             bool* removable) OVERRIDE;
  virtual std::vector<FilePath> GetAttachedDevices() OVERRIDE;

 private:
  // Private, this class is ref-counted.
  virtual ~TestVolumeMountWatcherWin();

  // Set to true to pre-attach test devices.
  bool pre_attach_devices_;

  DISALLOW_COPY_AND_ASSIGN(TestVolumeMountWatcherWin);
};

}  // namespace test
}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_TEST_VOLUME_MOUNT_WATCHER_WIN_H_