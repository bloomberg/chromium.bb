// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/disks/mock_disk_mount_manager.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::ReturnRef;

namespace chromeos {
namespace disks {

namespace {

const char* kTestSystemPath = "/this/system/path";
const char* kTestSystemPathPrefix = "/this/system";
const char* kTestDevicePath = "/this/device/path";
const char* kTestMountPath = "/media/foofoo";
const char* kTestFilePath = "/this/file/path";
const char* kTestDeviceLabel = "A label";
const char* kTestDriveLabel = "Another label";

}  // namespace

void MockDiskMountManager::AddObserverInternal(
    DiskMountManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void MockDiskMountManager::RemoveObserverInternal(
    DiskMountManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

MockDiskMountManager::MockDiskMountManager() {
  ON_CALL(*this, AddObserver(_))
      .WillByDefault(Invoke(this, &MockDiskMountManager::AddObserverInternal));
  ON_CALL(*this, RemoveObserver(_))
      .WillByDefault(Invoke(this,
                            &MockDiskMountManager::RemoveObserverInternal));
  ON_CALL(*this, disks())
      .WillByDefault(Invoke(this, &MockDiskMountManager::disksInternal));
}

MockDiskMountManager::~MockDiskMountManager() {
}

void MockDiskMountManager::NotifyDeviceInsertEvents() {
  scoped_ptr<DiskMountManager::Disk> disk1(new DiskMountManager::Disk(
      std::string(kTestDevicePath),
      std::string(),
      std::string(kTestSystemPath),
      std::string(kTestFilePath),
      std::string(),
      std::string(kTestDriveLabel),
      std::string(kTestSystemPathPrefix),
      DEVICE_TYPE_USB,
      4294967295U,
      false,  // is_parent
      false,  // is_read_only
      true,  // has_media
      false,  // on_boot_device
      false));  // is_hidden

  disks_.clear();
  disks_.insert(std::pair<std::string, DiskMountManager::Disk*>(
      std::string(kTestDevicePath), disk1.get()));

  // Device Added
  DiskMountManagerEventType event;
  event = MOUNT_DEVICE_ADDED;
  NotifyDeviceChanged(event, kTestSystemPath);

  // Disk Added
  event = MOUNT_DISK_ADDED;
  NotifyDiskChanged(event, disk1.get());

  // Disk Changed
  scoped_ptr<DiskMountManager::Disk> disk2(new DiskMountManager::Disk(
      std::string(kTestDevicePath),
      std::string(kTestMountPath),
      std::string(kTestSystemPath),
      std::string(kTestFilePath),
      std::string(kTestDeviceLabel),
      std::string(kTestDriveLabel),
      std::string(kTestSystemPathPrefix),
      DEVICE_TYPE_MOBILE,
      1073741824,
      false,  // is_parent
      false,  // is_read_only
      true,  // has_media
      false,  // on_boot_device
      false));  // is_hidden
  disks_.clear();
  disks_.insert(std::pair<std::string, DiskMountManager::Disk*>(
      std::string(kTestDevicePath), disk2.get()));
  event = MOUNT_DISK_CHANGED;
  NotifyDiskChanged(event, disk2.get());
}

void MockDiskMountManager::NotifyDeviceRemoveEvents() {
  scoped_ptr<DiskMountManager::Disk> disk(new DiskMountManager::Disk(
      std::string(kTestDevicePath),
      std::string(kTestMountPath),
      std::string(kTestSystemPath),
      std::string(kTestFilePath),
      std::string(kTestDeviceLabel),
      std::string(kTestDriveLabel),
      std::string(kTestSystemPathPrefix),
      DEVICE_TYPE_SD,
      1073741824,
      false,  // is_parent
      false,  // is_read_only
      true,  // has_media
      false,  // on_boot_device
      false));  // is_hidden
  disks_.clear();
  disks_.insert(std::pair<std::string, DiskMountManager::Disk*>(
      std::string(kTestDevicePath), disk.get()));
  NotifyDiskChanged(MOUNT_DISK_REMOVED, disk.get());
}

void MockDiskMountManager::SetupDefaultReplies() {
  EXPECT_CALL(*this, AddObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*this, RemoveObserver(_))
      .Times(AnyNumber());
  EXPECT_CALL(*this, disks())
      .WillRepeatedly(ReturnRef(disks_));
  EXPECT_CALL(*this, RequestMountInfoRefresh())
      .Times(AnyNumber());
  EXPECT_CALL(*this, MountPath(_, _, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*this, UnmountPath(_))
      .Times(AnyNumber());
  EXPECT_CALL(*this, FormatUnmountedDevice(_))
      .Times(AnyNumber());
  EXPECT_CALL(*this, FormatMountedDevice(_))
      .Times(AnyNumber());
  EXPECT_CALL(*this, UnmountDeviceRecursive(_, _, _))
      .Times(AnyNumber());
}

void MockDiskMountManager::NotifyDiskChanged(DiskMountManagerEventType event,
                                             const DiskMountManager::Disk* disk)
{
  // Make sure we run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(Observer, observers_, DiskChanged(event, disk));
}

void MockDiskMountManager::NotifyDeviceChanged(DiskMountManagerEventType event,
                                               const std::string& path) {
  // Make sure we run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(Observer, observers_, DeviceChanged(event, path));
}

}  // namespace disks
}  // namespace chromeos
