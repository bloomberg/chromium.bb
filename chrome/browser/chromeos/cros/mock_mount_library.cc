// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/mock_mount_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

using testing::_;
using testing::Invoke;

const char* kTestSystemPath = "/this/system/path";
const char* kTestDevicePath = "/this/device/path";
const char* kTestMountPath = "/media/foofoo";
const char* kTestFilePath = "/this/file/path";
const char* kTestDeviceLabel = "A label";
const char* kTestDriveLabel = "Another label";
const char* kTestParentPath = "/this/is/my/parent";

void MockMountLibrary::AddObserverInternal(MountLibrary::Observer* observer) {
  observers_.AddObserver(observer);
}

void MockMountLibrary::RemoveObserverInternal(
    MountLibrary::Observer* observer) {
  observers_.RemoveObserver(observer);
}

MockMountLibrary::MockMountLibrary() {
  ON_CALL(*this, AddObserver(_))
      .WillByDefault(Invoke(this, &MockMountLibrary::AddObserverInternal));
  ON_CALL(*this, RemoveObserver(_))
      .WillByDefault(Invoke(this, &MockMountLibrary::RemoveObserverInternal));
  ON_CALL(*this, disks())
      .WillByDefault(Invoke(this, &MockMountLibrary::disksInternal));
}

MockMountLibrary::~MockMountLibrary() {

}

void MockMountLibrary::FireDeviceInsertEvents() {

  scoped_ptr<MountLibrary::Disk> disk1(new MountLibrary::Disk(
      std::string(kTestDevicePath),
      std::string(),
      std::string(kTestSystemPath),
      std::string(kTestFilePath),
      std::string(),
      std::string(kTestDriveLabel),
      std::string(kTestParentPath),
      FLASH,
      4294967295U,
      false,
      false,
      true,
      false));

  disks_.clear();
  disks_.insert(std::pair<std::string, MountLibrary::Disk*>(
      std::string(kTestDevicePath), disk1.get()));

  // Device Added
  chromeos::MountLibraryEventType evt;
  evt = chromeos::MOUNT_DEVICE_ADDED;
  UpdateDeviceChanged(evt, kTestSystemPath);

  // Disk Added
  evt = chromeos::MOUNT_DISK_ADDED;
  UpdateDiskChanged(evt, disk1.get());

  // Disk Changed
  scoped_ptr<MountLibrary::Disk> disk2(new MountLibrary::Disk(
      std::string(kTestDevicePath),
      std::string(kTestMountPath),
      std::string(kTestSystemPath),
      std::string(kTestFilePath),
      std::string(kTestDeviceLabel),
      std::string(kTestDriveLabel),
      std::string(kTestParentPath),
      FLASH,
      1073741824,
      false,
      false,
      true,
      false));
  disks_.clear();
  disks_.insert(std::pair<std::string, MountLibrary::Disk*>(
      std::string(kTestDevicePath), disk2.get()));
  evt = chromeos::MOUNT_DISK_CHANGED;
  UpdateDiskChanged(evt, disk2.get());
}

void MockMountLibrary::FireDeviceRemoveEvents() {
  scoped_ptr<MountLibrary::Disk> disk(new MountLibrary::Disk(
      std::string(kTestDevicePath),
      std::string(kTestMountPath),
      std::string(kTestSystemPath),
      std::string(kTestFilePath),
      std::string(kTestDeviceLabel),
      std::string(kTestDriveLabel),
      std::string(kTestParentPath),
      FLASH,
      1073741824,
      false,
      false,
      true,
      false));
  disks_.clear();
  disks_.insert(std::pair<std::string, MountLibrary::Disk*>(
      std::string(kTestDevicePath), disk.get()));
  UpdateDiskChanged(chromeos::MOUNT_DISK_REMOVED, disk.get());
}

void MockMountLibrary::UpdateDiskChanged(MountLibraryEventType evt,
                                         const MountLibrary::Disk* disk) {
  // Make sure we run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(Observer, observers_, DiskChanged(evt, disk));
}


void MockMountLibrary::UpdateDeviceChanged(MountLibraryEventType evt,
                                           const std::string& path) {
  // Make sure we run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(Observer, observers_, DeviceChanged(evt, path));
}

}  // namespace chromeos
