// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::StorageMonitorCros unit tests.

#include "chrome/browser/storage_monitor/storage_monitor_chromeos.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/mock_removable_storage_observer.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using content::BrowserThread;
using disks::DiskMountManager;
using testing::_;

const char kDeviceNameWithManufacturerDetails[] = "110 KB (CompanyA, Z101)";
const char kDevice1[] = "/dev/d1";
const char kDevice1Name[] = "d1";
const char kDevice1NameWithSizeInfo[] = "110 KB d1";
const char kDevice2[] = "/dev/disk/d2";
const char kDevice2Name[] = "d2";
const char kDevice2NameWithSizeInfo[] = "207 KB d2";
const char kEmptyDeviceLabel[] = "";
const char kMountPointA[] = "mnt_a";
const char kMountPointB[] = "mnt_b";
const char kSDCardDeviceName1[] = "8.6 MB Amy_SD";
const char kSDCardDeviceName2[] = "8.6 MB SD Card";
const char kSDCardMountPoint1[] = "media/removable/Amy_SD";
const char kSDCardMountPoint2[] = "media/removable/SD Card";
const char kProductName[] = "Z101";
const char kUniqueId1[] = "FFFF-FFFF";
const char kUniqueId2[] = "FFFF-FF0F";
const char kVendorName[] = "CompanyA";

uint64 kDevice1SizeInBytes = 113048;
uint64 kDevice2SizeInBytes = 212312;
uint64 kSDCardSizeInBytes = 9000000;

std::string GetDCIMDeviceId(const std::string& unique_id) {
  return chrome::MediaStorageUtil::MakeDeviceId(
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM,
      chrome::kFSUniqueIdPrefix + unique_id);
}

// Wrapper class to test StorageMonitorCros.
class StorageMonitorCrosTest : public testing::Test {
 public:
  StorageMonitorCrosTest();
  virtual ~StorageMonitorCrosTest();

 protected:
  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void MountDevice(MountError error_code,
                   const DiskMountManager::MountPointInfo& mount_info,
                   const std::string& unique_id,
                   const std::string& device_label,
                   const std::string& vendor_name,
                   const std::string& product_name,
                   DeviceType device_type,
                   uint64 device_size_in_bytes);

  void UnmountDevice(MountError error_code,
                     const DiskMountManager::MountPointInfo& mount_info);

  uint64 GetDeviceStorageSize(const std::string& device_location);

  // Create a directory named |dir| relative to the test directory.
  // Set |with_dcim_dir| to true if the created directory will have a "DCIM"
  // subdirectory.
  // Returns the full path to the created directory on success, or an empty
  // path on failure.
  base::FilePath CreateMountPoint(const std::string& dir, bool with_dcim_dir);

  static void PostQuitToUIThread();
  static void WaitForFileThread();

  chrome::MockRemovableStorageObserver& observer() {
    return *mock_storage_observer_;
  }

 private:
  // The message loops and threads to run tests on.
  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  // Temporary directory for created test data.
  base::ScopedTempDir scoped_temp_dir_;

  // Objects that talks with StorageMonitorCros.
  scoped_ptr<chrome::MockRemovableStorageObserver> mock_storage_observer_;
  // Owned by DiskMountManager.
  disks::MockDiskMountManager* disk_mount_manager_mock_;

  scoped_refptr<StorageMonitorCros> monitor_;

  DISALLOW_COPY_AND_ASSIGN(StorageMonitorCrosTest);
};

StorageMonitorCrosTest::StorageMonitorCrosTest()
    : ui_thread_(BrowserThread::UI, &ui_loop_),
      file_thread_(BrowserThread::FILE) {
}

StorageMonitorCrosTest::~StorageMonitorCrosTest() {
}

void StorageMonitorCrosTest::SetUp() {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  file_thread_.Start();
  disk_mount_manager_mock_ = new disks::MockDiskMountManager();
  DiskMountManager::InitializeForTesting(disk_mount_manager_mock_);
  disk_mount_manager_mock_->SetupDefaultReplies();

  mock_storage_observer_.reset(new chrome::MockRemovableStorageObserver);

  // Initialize the test subject.
  monitor_ = new StorageMonitorCros();
  monitor_->AddObserver(mock_storage_observer_.get());
}

void StorageMonitorCrosTest::TearDown() {
  monitor_->RemoveObserver(mock_storage_observer_.get());
  monitor_ = NULL;

  disk_mount_manager_mock_ = NULL;
  DiskMountManager::Shutdown();
  WaitForFileThread();
}

void StorageMonitorCrosTest::MountDevice(
    MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info,
    const std::string& unique_id,
    const std::string& device_label,
    const std::string& vendor_name,
    const std::string& product_name,
    DeviceType device_type,
    uint64 device_size_in_bytes) {
  if (error_code == MOUNT_ERROR_NONE) {
    disk_mount_manager_mock_->CreateDiskEntryForMountDevice(
        mount_info, unique_id, device_label, vendor_name, product_name,
        device_type, device_size_in_bytes);
  }
  monitor_->OnMountEvent(disks::DiskMountManager::MOUNTING, error_code,
                         mount_info);
  WaitForFileThread();
}

void StorageMonitorCrosTest::UnmountDevice(
    MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  monitor_->OnMountEvent(disks::DiskMountManager::UNMOUNTING, error_code,
                         mount_info);
  if (error_code == MOUNT_ERROR_NONE)
    disk_mount_manager_mock_->RemoveDiskEntryForMountDevice(mount_info);
  WaitForFileThread();
}

uint64 StorageMonitorCrosTest::GetDeviceStorageSize(
    const std::string& device_location) {
  return monitor_->GetStorageSize(device_location);
}

base::FilePath StorageMonitorCrosTest::CreateMountPoint(
    const std::string& dir, bool with_dcim_dir) {
  base::FilePath return_path(scoped_temp_dir_.path());
  return_path = return_path.AppendASCII(dir);
  base::FilePath path(return_path);
  if (with_dcim_dir)
    path = path.Append(chrome::kDCIMDirectoryName);
  if (!file_util::CreateDirectory(path))
    return base::FilePath();
  return return_path;
}

// static
void StorageMonitorCrosTest::PostQuitToUIThread() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          MessageLoop::QuitClosure());
}

// static
void StorageMonitorCrosTest::WaitForFileThread() {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&PostQuitToUIThread));
  MessageLoop::current()->Run();
}

// Simple test case where we attach and detach a media device.
TEST_F(StorageMonitorCrosTest, BasicAttachDetach) {
  base::FilePath mount_path1 = CreateMountPoint(kMountPointA, true);
  ASSERT_FALSE(mount_path1.empty());
  DiskMountManager::MountPointInfo mount_info(kDevice1,
                                              mount_path1.value(),
                                              MOUNT_TYPE_DEVICE,
                                              disks::MOUNT_CONDITION_NONE);
  MountDevice(MOUNT_ERROR_NONE, mount_info, kUniqueId1, kDevice1Name,
              kVendorName, kProductName, DEVICE_TYPE_USB, kDevice1SizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1), observer().last_attached().device_id);
  EXPECT_EQ(ASCIIToUTF16(kDevice1NameWithSizeInfo),
            observer().last_attached().name);
  EXPECT_EQ(mount_path1.value(), observer().last_attached().location);

  UnmountDevice(MOUNT_ERROR_NONE, mount_info);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1), observer().last_detached().device_id);

  base::FilePath mount_path2 = CreateMountPoint(kMountPointB, true);
  ASSERT_FALSE(mount_path2.empty());
  DiskMountManager::MountPointInfo mount_info2(kDevice2,
                                               mount_path2.value(),
                                               MOUNT_TYPE_DEVICE,
                                               disks::MOUNT_CONDITION_NONE);
  MountDevice(MOUNT_ERROR_NONE, mount_info2, kUniqueId2, kDevice2Name,
              kVendorName, kProductName, DEVICE_TYPE_USB, kDevice2SizeInBytes);
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2), observer().last_attached().device_id);
  EXPECT_EQ(ASCIIToUTF16(kDevice2NameWithSizeInfo),
            observer().last_attached().name);
  EXPECT_EQ(mount_path2.value(), observer().last_attached().location);

  UnmountDevice(MOUNT_ERROR_NONE, mount_info2);
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2), observer().last_detached().device_id);
}

// Removable mass storage devices with no dcim folder are also recognized.
TEST_F(StorageMonitorCrosTest, NoDCIM) {
  testing::Sequence mock_sequence;
  base::FilePath mount_path = CreateMountPoint(kMountPointA, false);
  const std::string kUniqueId = "FFFF-FFFF";
  ASSERT_FALSE(mount_path.empty());
  DiskMountManager::MountPointInfo mount_info(kDevice1,
                                              mount_path.value(),
                                              MOUNT_TYPE_DEVICE,
                                              disks::MOUNT_CONDITION_NONE);
  const std::string device_id = chrome::MediaStorageUtil::MakeDeviceId(
      chrome::MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM,
      chrome::kFSUniqueIdPrefix + kUniqueId);
  MountDevice(MOUNT_ERROR_NONE, mount_info, kUniqueId, kDevice1Name,
              kVendorName, kProductName, DEVICE_TYPE_USB, kDevice1SizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(device_id, observer().last_attached().device_id);
  EXPECT_EQ(ASCIIToUTF16(kDevice1NameWithSizeInfo),
            observer().last_attached().name);
  EXPECT_EQ(mount_path.value(), observer().last_attached().location);
}

// Non device mounts and mount errors are ignored.
TEST_F(StorageMonitorCrosTest, Ignore) {
  testing::Sequence mock_sequence;
  base::FilePath mount_path = CreateMountPoint(kMountPointA, true);
  const std::string kUniqueId = "FFFF-FFFF";
  ASSERT_FALSE(mount_path.empty());

  // Mount error.
  DiskMountManager::MountPointInfo mount_info(kDevice1,
                                              mount_path.value(),
                                              MOUNT_TYPE_DEVICE,
                                              disks::MOUNT_CONDITION_NONE);
  MountDevice(MOUNT_ERROR_UNKNOWN, mount_info, kUniqueId, kDevice1Name,
              kVendorName, kProductName, DEVICE_TYPE_USB, kDevice1SizeInBytes);
  EXPECT_EQ(0, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Not a device
  mount_info.mount_type = MOUNT_TYPE_ARCHIVE;
  MountDevice(MOUNT_ERROR_NONE, mount_info, kUniqueId, kDevice1Name,
              kVendorName, kProductName, DEVICE_TYPE_USB, kDevice1SizeInBytes);
  EXPECT_EQ(0, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Unsupported file system.
  mount_info.mount_type = MOUNT_TYPE_DEVICE;
  mount_info.mount_condition = disks::MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM;
  MountDevice(MOUNT_ERROR_NONE, mount_info, kUniqueId, kDevice1Name,
              kVendorName, kProductName, DEVICE_TYPE_USB, kDevice1SizeInBytes);
  EXPECT_EQ(0, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
}

TEST_F(StorageMonitorCrosTest, SDCardAttachDetach) {
  base::FilePath mount_path1 = CreateMountPoint(kSDCardMountPoint1, true);
  ASSERT_FALSE(mount_path1.empty());
  DiskMountManager::MountPointInfo mount_info1(kSDCardDeviceName1,
                                               mount_path1.value(),
                                               MOUNT_TYPE_DEVICE,
                                               disks::MOUNT_CONDITION_NONE);
  MountDevice(MOUNT_ERROR_NONE, mount_info1, kUniqueId2, kSDCardDeviceName1,
              kVendorName, kProductName, DEVICE_TYPE_SD, kSDCardSizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2), observer().last_attached().device_id);
  EXPECT_EQ(ASCIIToUTF16(kSDCardDeviceName1),
            observer().last_attached().name);
  EXPECT_EQ(mount_path1.value(), observer().last_attached().location);

  UnmountDevice(MOUNT_ERROR_NONE, mount_info1);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2), observer().last_detached().device_id);

  base::FilePath mount_path2 = CreateMountPoint(kSDCardMountPoint2, true);
  ASSERT_FALSE(mount_path2.empty());
  DiskMountManager::MountPointInfo mount_info2(kSDCardDeviceName2,
                                               mount_path2.value(),
                                               MOUNT_TYPE_DEVICE,
                                               disks::MOUNT_CONDITION_NONE);
  MountDevice(MOUNT_ERROR_NONE, mount_info2, kUniqueId2, kSDCardDeviceName2,
              kVendorName, kProductName, DEVICE_TYPE_SD, kSDCardSizeInBytes);
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2), observer().last_attached().device_id);
  EXPECT_EQ(ASCIIToUTF16(kSDCardDeviceName2),
            observer().last_attached().name);
  EXPECT_EQ(mount_path2.value(), observer().last_attached().location);

  UnmountDevice(MOUNT_ERROR_NONE, mount_info2);
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2), observer().last_detached().device_id);
}

TEST_F(StorageMonitorCrosTest, AttachDeviceWithEmptyLabel) {
  base::FilePath mount_path1 = CreateMountPoint(kMountPointA, true);
  ASSERT_FALSE(mount_path1.empty());
  DiskMountManager::MountPointInfo mount_info(kEmptyDeviceLabel,
                                              mount_path1.value(),
                                              MOUNT_TYPE_DEVICE,
                                              disks::MOUNT_CONDITION_NONE);
  MountDevice(MOUNT_ERROR_NONE, mount_info, kUniqueId1, kEmptyDeviceLabel,
              kVendorName, kProductName, DEVICE_TYPE_USB, kDevice1SizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1), observer().last_attached().device_id);
  EXPECT_EQ(ASCIIToUTF16(kDeviceNameWithManufacturerDetails),
            observer().last_attached().name);
  EXPECT_EQ(mount_path1.value(), observer().last_attached().location);

  UnmountDevice(MOUNT_ERROR_NONE, mount_info);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1), observer().last_detached().device_id);
}

TEST_F(StorageMonitorCrosTest, GetStorageSize) {
  base::FilePath mount_path1 = CreateMountPoint(kMountPointA, true);
  ASSERT_FALSE(mount_path1.empty());
  DiskMountManager::MountPointInfo mount_info(kEmptyDeviceLabel,
                                              mount_path1.value(),
                                              MOUNT_TYPE_DEVICE,
                                              disks::MOUNT_CONDITION_NONE);
  MountDevice(MOUNT_ERROR_NONE, mount_info, kUniqueId1, kEmptyDeviceLabel,
              kVendorName, kProductName, DEVICE_TYPE_USB, kDevice1SizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1), observer().last_attached().device_id);
  EXPECT_EQ(ASCIIToUTF16(kDeviceNameWithManufacturerDetails),
            observer().last_attached().name);
  EXPECT_EQ(mount_path1.value(), observer().last_attached().location);

  EXPECT_EQ(kDevice1SizeInBytes, GetDeviceStorageSize(mount_path1.value()));
  UnmountDevice(MOUNT_ERROR_NONE, mount_info);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1), observer().last_detached().device_id);
}

}  // namespace

}  // namespace chromeos
