// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/removable_device_notifications_mac.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/mock_removable_storage_observer.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

uint64 kTestSize = 1000000ULL;

namespace {

DiskInfoMac CreateDiskInfoMac(const std::string& unique_id,
                              const std::string& model_name,
                              const string16& display_name,
                              const base::FilePath& mount_point,
                              uint64 size_bytes) {
  NSMutableDictionary *dict = [NSMutableDictionary dictionary];
  [dict setObject:@"dummy_bsd_name"
           forKey:base::mac::CFToNSCast(kDADiskDescriptionMediaBSDNameKey)];
  [dict setObject:base::SysUTF8ToNSString(unique_id)
           forKey:base::mac::CFToNSCast(kDADiskDescriptionDeviceRevisionKey)];
  if (!model_name.empty()) {
    [dict setObject:base::SysUTF8ToNSString(model_name)
             forKey:base::mac::CFToNSCast(kDADiskDescriptionDeviceModelKey)];
  }
  NSString* path = base::mac::FilePathToNSString(mount_point);
  [dict setObject:[NSURL fileURLWithPath:path]
           forKey:base::mac::CFToNSCast(kDADiskDescriptionVolumePathKey)];
  [dict setObject:base::SysUTF16ToNSString(display_name)
           forKey:base::mac::CFToNSCast(kDADiskDescriptionVolumeNameKey)];
  [dict setObject:[NSNumber numberWithBool:YES]
           forKey:base::mac::CFToNSCast(kDADiskDescriptionMediaRemovableKey)];
  [dict setObject:[NSNumber numberWithInt:size_bytes]
           forKey:base::mac::CFToNSCast(kDADiskDescriptionMediaSizeKey)];
  return DiskInfoMac::BuildDiskInfoOnFileThread(base::mac::NSToCFCast(dict));
}

}  // namespace

class RemovableDeviceNotificationsMacTest : public testing::Test {
 public:
  RemovableDeviceNotificationsMacTest()
      : message_loop_(MessageLoop::TYPE_IO),
        file_thread_(content::BrowserThread::FILE, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    notifications_ = new RemovableDeviceNotificationsMac;

    mock_storage_observer_.reset(new MockRemovableStorageObserver);
    notifications_->AddObserver(mock_storage_observer_.get());


    unique_id_ = "test_id";
    display_name_ = ASCIIToUTF16("977 KB Test Display Name");
    mount_point_ = base::FilePath("/unused_test_directory");
    device_id_ = MediaStorageUtil::MakeDeviceId(
        MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM, unique_id_);
    disk_info_ = CreateDiskInfoMac(unique_id_, "",
                                   ASCIIToUTF16("Test Display Name"),
                                   mount_point_, kTestSize);
  }

 protected:
  // The message loop and file thread to run tests on.
  MessageLoop message_loop_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<MockRemovableStorageObserver> mock_storage_observer_;

  // Information about the disk.
  std::string unique_id_;
  string16 display_name_;
  base::FilePath mount_point_;
  std::string device_id_;
  DiskInfoMac disk_info_;

  scoped_refptr<RemovableDeviceNotificationsMac> notifications_;
};

TEST_F(RemovableDeviceNotificationsMacTest, AddRemove) {
  notifications_->UpdateDisk(
      disk_info_, RemovableDeviceNotificationsMac::UPDATE_DEVICE_ADDED);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, mock_storage_observer_->attach_calls());
  EXPECT_EQ(0, mock_storage_observer_->detach_calls());
  EXPECT_EQ(device_id_, mock_storage_observer_->last_attached().device_id);
  EXPECT_EQ(display_name_, mock_storage_observer_->last_attached().name);
  EXPECT_EQ(mount_point_.value(),
            mock_storage_observer_->last_attached().location);

  notifications_->UpdateDisk(
      disk_info_, RemovableDeviceNotificationsMac::UPDATE_DEVICE_REMOVED);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, mock_storage_observer_->attach_calls());
  EXPECT_EQ(1, mock_storage_observer_->detach_calls());
  EXPECT_EQ(device_id_, mock_storage_observer_->last_detached().device_id);
}

TEST_F(RemovableDeviceNotificationsMacTest, UpdateVolumeName) {
  notifications_->UpdateDisk(
      disk_info_, RemovableDeviceNotificationsMac::UPDATE_DEVICE_ADDED);
  message_loop_.RunUntilIdle();

  EXPECT_EQ(1, mock_storage_observer_->attach_calls());
  EXPECT_EQ(0, mock_storage_observer_->detach_calls());
  EXPECT_EQ(device_id_, mock_storage_observer_->last_attached().device_id);
  EXPECT_EQ(display_name_, mock_storage_observer_->last_attached().name);
  EXPECT_EQ(mount_point_.value(),
            mock_storage_observer_->last_attached().location);

  string16 new_display_name(ASCIIToUTF16("977 KB Test Display Name"));
  DiskInfoMac info2 = CreateDiskInfoMac(
      unique_id_, "", ASCIIToUTF16("Test Display Name"), mount_point_,
      kTestSize);
  notifications_->UpdateDisk(
      info2, RemovableDeviceNotificationsMac::UPDATE_DEVICE_CHANGED);
  message_loop_.RunUntilIdle();

  EXPECT_EQ(1, mock_storage_observer_->detach_calls());
  EXPECT_EQ(device_id_, mock_storage_observer_->last_detached().device_id);
  EXPECT_EQ(2, mock_storage_observer_->attach_calls());
  EXPECT_EQ(device_id_, mock_storage_observer_->last_attached().device_id);
  EXPECT_EQ(new_display_name, mock_storage_observer_->last_attached().name);
  EXPECT_EQ(mount_point_.value(),
            mock_storage_observer_->last_attached().location);
}

TEST_F(RemovableDeviceNotificationsMacTest, DCIM) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(file_util::CreateDirectory(
      temp_dir.path().Append(kDCIMDirectoryName)));

  base::FilePath mount_point = temp_dir.path();
  DiskInfoMac info = CreateDiskInfoMac(
      unique_id_, "", ASCIIToUTF16("Test Display Name"), mount_point,
      kTestSize);
  std::string device_id = MediaStorageUtil::MakeDeviceId(
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM, unique_id_);

  notifications_->UpdateDisk(
      info, RemovableDeviceNotificationsMac::UPDATE_DEVICE_ADDED);
  message_loop_.RunUntilIdle();

  EXPECT_EQ(1, mock_storage_observer_->attach_calls());
  EXPECT_EQ(0, mock_storage_observer_->detach_calls());
  EXPECT_EQ(device_id, mock_storage_observer_->last_attached().device_id);
  EXPECT_EQ(display_name_, mock_storage_observer_->last_attached().name);
  EXPECT_EQ(mount_point.value(),
            mock_storage_observer_->last_attached().location);
}

TEST_F(RemovableDeviceNotificationsMacTest, GetStorageInfo) {
  notifications_->UpdateDisk(
      disk_info_, RemovableDeviceNotificationsMac::UPDATE_DEVICE_ADDED);
  message_loop_.RunUntilIdle();

  EXPECT_EQ(1, mock_storage_observer_->attach_calls());
  EXPECT_EQ(0, mock_storage_observer_->detach_calls());
  EXPECT_EQ(device_id_, mock_storage_observer_->last_attached().device_id);
  EXPECT_EQ(display_name_, mock_storage_observer_->last_attached().name);
  EXPECT_EQ(mount_point_.value(),
            mock_storage_observer_->last_attached().location);

  StorageMonitor::StorageInfo info;
  EXPECT_TRUE(notifications_->GetStorageInfoForPath(
      mount_point_.AppendASCII("foo"), &info));
  EXPECT_EQ(info.device_id, device_id_);
  EXPECT_EQ(info.name, ASCIIToUTF16("Test Display Name"));
  EXPECT_EQ(info.location, mount_point_.value());

  EXPECT_FALSE(notifications_->GetStorageInfoForPath(
      base::FilePath("/non/matching/path"), &info));
}

TEST_F(RemovableDeviceNotificationsMacTest, GetStorageSize) {
  notifications_->UpdateDisk(
      disk_info_, RemovableDeviceNotificationsMac::UPDATE_DEVICE_ADDED);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(1, mock_storage_observer_->attach_calls());

  EXPECT_EQ(kTestSize,
            notifications_->GetStorageSize("/unused_test_directory"));
}

// Test that mounting a DMG doesn't send a notification.
TEST_F(RemovableDeviceNotificationsMacTest, DMG) {
  DiskInfoMac info = CreateDiskInfoMac(
      unique_id_, "Disk Image", display_name_, mount_point_, kTestSize);
  notifications_->UpdateDisk(
      info, RemovableDeviceNotificationsMac::UPDATE_DEVICE_ADDED);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(0, mock_storage_observer_->attach_calls());
}

}  // namespace chrome
