// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestVolumeMountWatcherWin implementation.

#include "chrome/browser/storage_monitor/test_volume_mount_watcher_win.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"

namespace chrome {
namespace test {

namespace {

std::vector<base::FilePath> FakeGetSingleAttachedDevice() {
  std::vector<base::FilePath> result;
  result.push_back(VolumeMountWatcherWin::DriveNumberToFilePath(2));  // C
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
                                 StorageInfo* info) {
  // Truncate to root path.
  base::FilePath path(device_path);
  if (device_path.value().length() > 3) {
    path = base::FilePath(device_path.value().substr(0, 3));
  }
  if (path.value()[0] < L'A' || path.value()[0] > L'Z') {
    return false;
  }

  if (info) {
    info->location = path.value();
    info->total_size_in_bytes = 1000000;

    std::string unique_id =
        "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\";
    unique_id[11] = device_path.value()[0];
    chrome::MediaStorageUtil::Type type =
        chrome::MediaStorageUtil::FIXED_MASS_STORAGE;
    if (path.value() != ASCIIToUTF16("N:\\") &&
        path.value() != ASCIIToUTF16("C:\\")) {
      type = chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM;
    }
    info->device_id = chrome::MediaStorageUtil::MakeDeviceId(type, unique_id);
    info->name = path.Append(L" Drive").LossyDisplayName();
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
    const string16& device_name,
    uint64 total_size_in_bytes) {
  StorageInfo info;
  info.device_id = device_id;
  info.location = device_path.value();
  info.name = device_name;
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
  devices_checked_.clear();
}

void TestVolumeMountWatcherWin::ReleaseDeviceCheck() {
  device_check_complete_event_->Signal();
}

bool TestVolumeMountWatcherWin::GetDeviceRemovable(
    const base::FilePath& device_path,
    bool* removable) const {
  StorageInfo info;
  bool success = GetMassStorageDeviceDetails(device_path, &info);
  *removable = MediaStorageUtil::IsRemovableDevice(info.device_id);
  return success;
}

VolumeMountWatcherWin::GetDeviceDetailsCallbackType
TestVolumeMountWatcherWin::GetDeviceDetailsCallback() const {
  return base::Bind(&GetMassStorageDeviceDetails);
}

VolumeMountWatcherWin::GetAttachedDevicesCallbackType
  TestVolumeMountWatcherWin::GetAttachedDevicesCallback() const {
  if (attached_devices_fake_)
    return base::Bind(&FakeGetAttachedDevices);

  return base::Bind(&FakeGetSingleAttachedDevice);
}

void TestVolumeMountWatcherWin::ShutdownWorkerPool() {
  device_info_worker_pool_->Shutdown();
}

}  // namespace test
}  // namespace chrome
