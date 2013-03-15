// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestVolumeMountWatcherWin implementation.

#include "chrome/browser/storage_monitor/test_volume_mount_watcher_win.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/utf_string_conversions.h"

namespace chrome {
namespace test {

namespace {

std::vector<base::FilePath> NoAttachedDevices() {
  std::vector<base::FilePath> result;
  return result;
}

std::vector<base::FilePath> FakeGetAttachedDevices() {
  std::vector<base::FilePath> result;
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(0));  // A
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(1));  // B
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(2));  // C
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(3));  // D
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(5));  // F
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(7));  // H
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(13));  // N
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(25));  // Z
  return result;
}

// Gets the details of the mass storage device specified by the |device_path|.
// |device_path| inputs of 'A:\' - 'Z:\' are valid. 'N:\' is not removable.
// 'C:\' is not removable (so that auto-added paths are correctly handled).
bool GetMassStorageDeviceDetails(const base::FilePath& device_path,
                                 string16* device_location,
                                 std::string* unique_id,
                                 string16* name,
                                 bool* removable,
                                 uint64* total_size_in_bytes) {
  // Truncate to root path.
  base::FilePath path(device_path);
  if (device_path.value().length() > 3) {
    path = base::FilePath(device_path.value().substr(0, 3));
  }
  if (path.value()[0] < L'A' || path.value()[0] > L'Z') {
    return false;
  }

  if (device_location)
    *device_location = path.value();
  if (total_size_in_bytes)
    *total_size_in_bytes = 1000000;
  if (unique_id) {
    *unique_id = "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\";
    (*unique_id)[11] = device_path.value()[0];
  }
  if (name)
    *name = path.Append(L" Drive").LossyDisplayName();
  if (removable) {
    *removable = (path.value() != ASCIIToUTF16("N:\\") &&
                  path.value() != ASCIIToUTF16("C:\\"));
  }
  return true;
}

} // namespace

// TestVolumeMountWatcherWin ---------------------------------------------------

TestVolumeMountWatcherWin::TestVolumeMountWatcherWin()
    : attached_devices_fake_(false) {}

TestVolumeMountWatcherWin::~TestVolumeMountWatcherWin() {
}

void TestVolumeMountWatcherWin::AddDeviceForTesting(
    const base::FilePath& device_path,
    const std::string& device_id,
    const std::string& unique_id,
    const string16& device_name,
    bool removable,
    uint64 total_size_in_bytes) {
  VolumeMountWatcherWin::MountPointInfo info;
  info.device_id = device_id;
  info.location = device_path.value();
  info.unique_id = unique_id;
  info.name = device_name;
  info.removable = removable;
  info.total_size_in_bytes = total_size_in_bytes;
  HandleDeviceAttachEventOnUIThread(device_path, info);
}

void TestVolumeMountWatcherWin::SetAttachedDevicesFake() {
  attached_devices_fake_ = true;
}

void TestVolumeMountWatcherWin::FlushWorkerPoolForTesting() {
  device_info_worker_pool_->FlushForTesting();
}

void TestVolumeMountWatcherWin::DeviceCheckComplete(
    const base::FilePath& device_path) {
  devices_checked_.push_back(device_path);
  if (device_check_complete_event_.get())
    device_check_complete_event_->Wait();
  VolumeMountWatcherWin::DeviceCheckComplete(device_path);
}

void TestVolumeMountWatcherWin::BlockDeviceCheckForTesting() {
  device_check_complete_event_.reset(new base::WaitableEvent(false, false));
}

void TestVolumeMountWatcherWin::ReleaseDeviceCheck() {
  device_check_complete_event_->Signal();
}

VolumeMountWatcherWin::GetDeviceDetailsCallbackType
  TestVolumeMountWatcherWin::GetDeviceDetailsCallback() const {
  return base::Bind(&GetMassStorageDeviceDetails);
}

VolumeMountWatcherWin::GetAttachedDevicesCallbackType
  TestVolumeMountWatcherWin::GetAttachedDevicesCallback() const {
  return attached_devices_fake_ ?
      base::Bind(&FakeGetAttachedDevices) : base::Bind(&NoAttachedDevices);
}

void TestVolumeMountWatcherWin::ShutdownWorkerPool() {
  device_info_worker_pool_->Shutdown();
}

}  // namespace test
}  // namespace chrome
