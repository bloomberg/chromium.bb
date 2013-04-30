// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/test_storage_monitor.h"

#include "chrome/browser/storage_monitor/media_storage_util.h"

#if defined(OS_LINUX)
#include "chrome/browser/storage_monitor/test_media_transfer_protocol_manager_linux.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"
#endif

namespace chrome {
namespace test {

TestStorageMonitor::TestStorageMonitor()
    : StorageMonitor(),
      init_called_(false) {
#if defined(OS_LINUX)
  media_transfer_protocol_manager_.reset(
      new TestMediaTransferProtocolManagerLinux());
#endif
}

TestStorageMonitor::~TestStorageMonitor() {}

TestStorageMonitor*
TestStorageMonitor::CreateForBrowserTests() {
  StorageMonitor::RemoveSingletonForTesting();
  return new TestStorageMonitor();
}

void TestStorageMonitor::Init() {
  init_called_ = true;
}

void TestStorageMonitor::MarkInitialized() {
  StorageMonitor::MarkInitialized();
}

bool TestStorageMonitor::GetStorageInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  if (!path.IsAbsolute())
    return false;

  if (device_info) {
    device_info->device_id = MediaStorageUtil::MakeDeviceId(
        MediaStorageUtil::FIXED_MASS_STORAGE, path.AsUTF8Unsafe());
    device_info->name = path.BaseName().LossyDisplayName();
    device_info->location = path.value();
    device_info->total_size_in_bytes = 0;
  }
  return true;
}

#if defined(OS_WIN)
bool TestStorageMonitor::GetMTPStorageInfoFromDeviceId(
    const std::string& storage_device_id,
    string16* device_location,
    string16* storage_object_id) const {
  return false;
}
#endif

#if defined(OS_LINUX)
device::MediaTransferProtocolManager*
TestStorageMonitor::media_transfer_protocol_manager() {
  return media_transfer_protocol_manager_.get();
}
#endif

StorageMonitor::Receiver* TestStorageMonitor::receiver() const {
  return StorageMonitor::receiver();
}

void TestStorageMonitor::EjectDevice(
    const std::string& device_id,
    base::Callback<void(EjectStatus)> callback) {
  ejected_device_ = device_id;
  callback.Run(EJECT_OK);
}

}  // namespace test
}  // namespace chrome
