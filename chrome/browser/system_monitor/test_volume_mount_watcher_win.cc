// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestVolumeMountWatcherWin implementation.

#include "chrome/browser/system_monitor/test_volume_mount_watcher_win.h"

#include "base/file_path.h"

namespace chrome {
namespace test {

namespace {

// Gets the details of the mass storage device specified by the |device_path|.
// |device_path| inputs of 'A:\' - 'Z:\' are valid. 'N:\' is not removable.
bool GetMassStorageDeviceDetails(const FilePath& device_path,
                                 string16* device_location,
                                 std::string* unique_id,
                                 string16* name,
                                 bool* removable) {
  if (device_path.value().length() != 3 || device_path.value()[0] < L'A' ||
      device_path.value()[0] > L'Z') {
    return false;
  }

  if (device_location)
    *device_location = device_path.value();
  if (unique_id) {
    *unique_id = "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\";
    (*unique_id)[11] = device_path.value()[0];
  }
  if (name)
    *name = device_path.Append(L" Drive").LossyDisplayName();
  if (removable)
    *removable = device_path.value()[0] != L'N';
  return true;
}

// Returns a list of attached device locations.
std::vector<FilePath> GetTestAttachedDevices() {
  std::vector<FilePath> result;
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(0));
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(1));
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(2));
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(3));
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(5));
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(7));
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(25));
  return result;
}

}  // namespace

// TestVolumeMountWatcherWin ---------------------------------------------------

TestVolumeMountWatcherWin::TestVolumeMountWatcherWin()
    : pre_attach_devices_(false) {
}

bool TestVolumeMountWatcherWin::GetDeviceInfo(const FilePath& device_path,
                                              string16* device_location,
                                              std::string* unique_id,
                                              string16* name,
                                              bool* removable) {
  return GetMassStorageDeviceDetails(device_path, device_location, unique_id,
                                     name, removable);
}

std::vector<FilePath> TestVolumeMountWatcherWin::GetAttachedDevices() {
  return pre_attach_devices_ ?
      GetTestAttachedDevices() : std::vector<FilePath>();
}

TestVolumeMountWatcherWin::~TestVolumeMountWatcherWin() {
}

}  // namespace test
}  // namespace chrome