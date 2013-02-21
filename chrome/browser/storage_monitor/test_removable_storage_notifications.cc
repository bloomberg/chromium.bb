// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/test_removable_storage_notifications.h"

namespace chrome {
namespace test {

TestRemovableStorageNotifications::TestRemovableStorageNotifications()
    : RemovableStorageNotifications() {}

TestRemovableStorageNotifications::~TestRemovableStorageNotifications() {}

TestRemovableStorageNotifications*
TestRemovableStorageNotifications::CreateForBrowserTests() {
  RemovableStorageNotifications::RemoveSingletonForTesting();
  return new TestRemovableStorageNotifications();
}

bool TestRemovableStorageNotifications::GetDeviceInfoForPath(
    const base::FilePath& path,
    StorageInfo* device_info) const {
  return false;
}

uint64 TestRemovableStorageNotifications::GetStorageSize(
    const base::FilePath::StringType& location) const {
  return 0;
}

#if defined(OS_WIN)
bool TestRemovableStorageNotifications::GetMTPStorageInfoFromDeviceId(
    const std::string& storage_device_id,
    string16* device_location,
    string16* storage_object_id) const {
  return false;
}
#endif

void TestRemovableStorageNotifications::ProcessAttach(
    const std::string& id,
    const string16& name,
    const base::FilePath::StringType& location) {
  receiver()->ProcessAttach(id, name, location);
}

void TestRemovableStorageNotifications::ProcessDetach(const std::string& id) {
  receiver()->ProcessDetach(id);
}

RemovableStorageNotifications::Receiver*
TestRemovableStorageNotifications::receiver() const {
  return RemovableStorageNotifications::receiver();
}

void TestRemovableStorageNotifications::EjectDevice(
    const std::string& device_id,
    base::Callback<void(EjectStatus)> callback) {
  ejected_device_ = device_id;
  callback.Run(EJECT_OK);
}

}  // namespace test
}  // namespace chrome
