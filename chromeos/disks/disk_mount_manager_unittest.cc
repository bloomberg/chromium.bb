// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/fake_cros_disks_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::disks::DiskMountManager;
using chromeos::CrosDisksClient;
using chromeos::DBusThreadManager;
using chromeos::FakeCrosDisksClient;
using chromeos::FakeDBusThreadManager;
using testing::_;
using testing::Field;
using testing::InSequence;

namespace {

// Holds information needed to create a DiskMountManager::Disk instance.
struct TestDiskInfo {
  const char* source_path;
  const char* mount_path;
  const char* system_path;
  const char* file_path;
  const char* device_label;
  const char* drive_label;
  const char* vendor_id;
  const char* vendor_name;
  const char* product_id;
  const char* product_name;
  const char* fs_uuid;
  const char* system_path_prefix;
  chromeos::DeviceType device_type;
  uint64 size_in_bytes;
  bool is_parent;
  bool is_read_only;
  bool has_media;
  bool on_boot_device;
  bool on_removable_device;
  bool is_hidden;
};

// Holds information to create a DiskMOuntManager::MountPointInfo instance.
struct TestMountPointInfo {
  const char* source_path;
  const char* mount_path;
  chromeos::MountType mount_type;
  chromeos::disks::MountCondition mount_condition;
};

// List of disks held in DiskMountManager at the begining of the test.
const TestDiskInfo kTestDisks[] = {
  {
    "/device/source_path",
    "/device/mount_path",
    "/device/prefix/system_path",
    "/device/file_path",
    "/device/device_label",
    "/device/drive_label",
    "/device/vendor_id",
    "/device/vendor_name",
    "/device/product_id",
    "/device/product_name",
    "/device/fs_uuid",
    "/device/prefix",
    chromeos::DEVICE_TYPE_USB,
    1073741824,  // size in bytes
    false,  // is parent
    false,  // is read only
    true,  // has media
    false,  // is on boot device
    true,  // is on removable device
    false  // is hidden
  },
};

// List of mount points  held in DiskMountManager at the begining of the test.
const TestMountPointInfo kTestMountPoints[] = {
  {
    "/archive/source_path",
    "/archive/mount_path",
    chromeos::MOUNT_TYPE_ARCHIVE,
    chromeos::disks::MOUNT_CONDITION_NONE
  },
  {
    "/device/source_path",
    "/device/mount_path",
    chromeos::MOUNT_TYPE_DEVICE,
    chromeos::disks::MOUNT_CONDITION_NONE
  },
};

// Mocks DiskMountManager observer.
class MockDiskMountManagerObserver : public DiskMountManager::Observer {
 public:
  virtual ~MockDiskMountManagerObserver() {}

  MOCK_METHOD2(OnDiskEvent, void(DiskMountManager::DiskEvent event,
                                 const DiskMountManager::Disk* disk));
  MOCK_METHOD2(OnDeviceEvent, void(DiskMountManager::DeviceEvent event,
                                   const std::string& device_path));
  MOCK_METHOD3(OnMountEvent,
      void(DiskMountManager::MountEvent event,
           chromeos::MountError error_code,
           const DiskMountManager::MountPointInfo& mount_point));
  MOCK_METHOD3(OnFormatEvent,
      void(DiskMountManager::FormatEvent event,
           chromeos::FormatError error_code,
           const std::string& device_path));
};

class DiskMountManagerTest : public testing::Test {
 public:
  DiskMountManagerTest() {}
  virtual ~DiskMountManagerTest() {}

  // Sets up test dbus tread manager and disks mount manager.
  // Initializes disk mount manager disks and mount points.
  // Adds a test observer to the disk mount manager.
  virtual void SetUp() {
    FakeDBusThreadManager* fake_thread_manager = new FakeDBusThreadManager();
    fake_cros_disks_client_ = new FakeCrosDisksClient;
    fake_thread_manager->SetCrosDisksClient(
        scoped_ptr<CrosDisksClient>(fake_cros_disks_client_));

    DBusThreadManager::InitializeForTesting(fake_thread_manager);

    DiskMountManager::Initialize();

    InitDisksAndMountPoints();

    DiskMountManager::GetInstance()->AddObserver(&observer_);
  }

  // Shuts down dbus thread manager and disk moutn manager used in the test.
  virtual void TearDown() {
    DiskMountManager::GetInstance()->RemoveObserver(&observer_);
    DiskMountManager::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  // Checks if disk mount manager contains a mount point with specified moutn
  // path.
  bool HasMountPoint(const std::string& mount_path) {
    const DiskMountManager::MountPointMap& mount_points =
        DiskMountManager::GetInstance()->mount_points();
    return mount_points.find(mount_path) != mount_points.end();
  }

 private:
  // Adds a new disk to the disk mount manager.
  void AddTestDisk(const TestDiskInfo& disk) {
    EXPECT_TRUE(DiskMountManager::GetInstance()->AddDiskForTest(
        new DiskMountManager::Disk(disk.source_path,
                                   disk.mount_path,
                                   disk.system_path,
                                   disk.file_path,
                                   disk.device_label,
                                   disk.drive_label,
                                   disk.vendor_id,
                                   disk.vendor_name,
                                   disk.product_id,
                                   disk.product_name,
                                   disk.fs_uuid,
                                   disk.system_path_prefix,
                                   disk.device_type,
                                   disk.size_in_bytes,
                                   disk.is_parent,
                                   disk.is_read_only,
                                   disk.has_media,
                                   disk.on_boot_device,
                                   disk.on_removable_device,
                                   disk.is_hidden)));
  }

  // Adds a new mount point to the disk mount manager.
  // If the moutn point is a device mount point, disk with its source path
  // should already be added to the disk mount manager.
  void AddTestMountPoint(const TestMountPointInfo& mount_point) {
    EXPECT_TRUE(DiskMountManager::GetInstance()->AddMountPointForTest(
        DiskMountManager::MountPointInfo(mount_point.source_path,
                                         mount_point.mount_path,
                                         mount_point.mount_type,
                                         mount_point.mount_condition)));
  }

  // Adds disks and mount points to disk mount manager.
  void InitDisksAndMountPoints() {
    // Disks should be  added first (when adding device mount points it is
    // expected that the corresponding disk is already added).
    for (size_t i = 0; i < arraysize(kTestDisks); i++)
      AddTestDisk(kTestDisks[i]);

    for (size_t i = 0; i < arraysize(kTestMountPoints); i++)
      AddTestMountPoint(kTestMountPoints[i]);
  }

 protected:
  chromeos::FakeCrosDisksClient* fake_cros_disks_client_;
  MockDiskMountManagerObserver observer_;
  base::MessageLoopForUI message_loop_;
};

// Tests that the observer gets notified on attempt to format non existent mount
// point.
TEST_F(DiskMountManagerTest, Format_NotMounted) {
  EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                       chromeos::FORMAT_ERROR_UNKNOWN,
                                       "/mount/non_existent"))
      .Times(1);
  DiskMountManager::GetInstance()->FormatMountedDevice("/mount/non_existent");
}

// Tests that it is not possible to format archive mount point.
TEST_F(DiskMountManagerTest, Format_Archive) {
  EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                       chromeos::FORMAT_ERROR_UNKNOWN,
                                       "/archive/source_path"))
      .Times(1);

  DiskMountManager::GetInstance()->FormatMountedDevice("/archive/mount_path");
}

// Tests that format fails if the device cannot be unmounted.
TEST_F(DiskMountManagerTest, Format_FailToUnmount) {
  // Before formatting mounted device, the device should be unmounted.
  // In this test unmount will fail, and there should be no attempt to
  // format the device.

  // Set up expectations for observer mock.
  // Observer should be notified that unmount attempt fails and format task
  // failed to start.
  {
    InSequence s;

    EXPECT_CALL(observer_,
        OnMountEvent(DiskMountManager::UNMOUNTING,
                     chromeos::MOUNT_ERROR_INTERNAL,
                     Field(&DiskMountManager::MountPointInfo::mount_path,
                           "/device/mount_path")))
        .Times(1);

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                         chromeos::FORMAT_ERROR_UNKNOWN,
                                         "/device/source_path"))
        .Times(1);
  }

  fake_cros_disks_client_->MakeUnmountFail();
  // Start test.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Cros disks will respond asynchronoulsy, so let's drain the message loop.
  message_loop_.RunUntilIdle();

  EXPECT_EQ(1, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ("/device/mount_path",
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(0, fake_cros_disks_client_->format_call_count());

  // The device mount should still be here.
  EXPECT_TRUE(HasMountPoint("/device/mount_path"));
}

// Tests that observer is notified when cros disks fails to start format
// process.
TEST_F(DiskMountManagerTest, Format_FormatFailsToStart) {
  // Before formatting mounted device, the device should be unmounted.
  // In this test, unmount will succeed, but call to Format method will
  // fail.

  // Set up expectations for observer mock.
  // Observer should be notified that the device was unmounted and format task
  // failed to start.
  {
    InSequence s;

    EXPECT_CALL(observer_,
        OnMountEvent(DiskMountManager::UNMOUNTING,
                     chromeos::MOUNT_ERROR_NONE,
                     Field(&DiskMountManager::MountPointInfo::mount_path,
                           "/device/mount_path")))
        .Times(1);

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                         chromeos::FORMAT_ERROR_UNKNOWN,
                                         "/device/source_path"))
        .Times(1);
  }

  fake_cros_disks_client_->MakeFormatFail();
  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Cros disks will respond asynchronoulsy, so let's drain the message loop.
  message_loop_.RunUntilIdle();

  EXPECT_EQ(1, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ("/device/mount_path",
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(1, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ("/device/source_path",
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat", fake_cros_disks_client_->last_format_filesystem());

  // The device mount should be gone.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));
}

// Tests the case where there are two format requests for the same device.
TEST_F(DiskMountManagerTest, Format_ConcurrentFormatCalls) {
  // Only the first format request should be processed (the second unmount
  // request fails because the device is already unmounted at that point).
  // CrosDisksClient will report that the format process for the first request
  // is successfully started.

  // Set up expectations for observer mock.
  // The observer should get a FORMAT_STARTED event for one format request and a
  // FORMAT_COMPLETED with an error code for the other format request. The
  // formatting will be started only for the first request.
  // There should be only one UNMOUNTING event. The result of the second one
  // should not be reported as the mount point will go away after the first
  // request.
  //
  // Note that in this test the format completion signal will not be simulated,
  // so the observer should not get FORMAT_COMPLETED signal.
  {
    InSequence s;

    EXPECT_CALL(observer_,
        OnMountEvent(DiskMountManager::UNMOUNTING,
                     chromeos::MOUNT_ERROR_NONE,
                     Field(&DiskMountManager::MountPointInfo::mount_path,
                           "/device/mount_path")))
        .Times(1);

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                         chromeos::FORMAT_ERROR_UNKNOWN,
                                         "/device/source_path"))
        .Times(1);

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                         chromeos::FORMAT_ERROR_NONE,
                                         "/device/source_path"))
        .Times(1);
  }

  fake_cros_disks_client_->set_unmount_listener(
      base::Bind(&FakeCrosDisksClient::MakeUnmountFail,
                 base::Unretained(fake_cros_disks_client_)));
  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Cros disks will respond asynchronoulsy, so let's drain the message loop.
  message_loop_.RunUntilIdle();

  EXPECT_EQ(2, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ("/device/mount_path",
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(1, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ("/device/source_path",
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat",
            fake_cros_disks_client_->last_format_filesystem());

  // The device mount should be gone.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));
}

// Tests the case when the format process actually starts and fails.
TEST_F(DiskMountManagerTest, Format_FormatFails) {
  // Both unmount and format device cals are successful in this test.

  // Set up expectations for observer mock.
  // The observer should get notified that the device was unmounted and that
  // formatting has started.
  // After the formatting starts, the test will simulate failing
  // FORMAT_COMPLETED signal, so the observer should also be notified the
  // formatting has failed (FORMAT_COMPLETED event).
  {
    InSequence s;

    EXPECT_CALL(observer_,
        OnMountEvent(DiskMountManager::UNMOUNTING,
                     chromeos::MOUNT_ERROR_NONE,
                     Field(&DiskMountManager::MountPointInfo::mount_path,
                           "/device/mount_path")))
        .Times(1);

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                         chromeos::FORMAT_ERROR_NONE,
                                         "/device/source_path"))
        .Times(1);

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                         chromeos::FORMAT_ERROR_UNKNOWN,
                                         "/device/source_path"))
        .Times(1);
  }

  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Wait for Unmount and Format calls to end.
  message_loop_.RunUntilIdle();

  EXPECT_EQ(1, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ("/device/mount_path",
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(1, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ("/device/source_path",
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat", fake_cros_disks_client_->last_format_filesystem());

  // The device should be unmounted by now.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));

  // Send failing FORMAT_COMPLETED signal.
  // The failure is marked by ! in fromt of the path (but this should change
  // soon).
  fake_cros_disks_client_->SendFormatCompletedEvent(
      chromeos::FORMAT_ERROR_UNKNOWN, "/device/source_path");
}

// Tests the case when formatting completes successfully.
TEST_F(DiskMountManagerTest, Format_FormatSuccess) {
  // Set up cros disks client mocks.
  // Both unmount and format device cals are successful in this test.

  // Set up expectations for observer mock.
  // The observer should receive UNMOUNTING, FORMAT_STARTED and FORMAT_COMPLETED
  // events (all of them without an error set).
  {
    InSequence s;

    EXPECT_CALL(observer_,
        OnMountEvent(DiskMountManager::UNMOUNTING,
                     chromeos::MOUNT_ERROR_NONE,
                     Field(&DiskMountManager::MountPointInfo::mount_path,
                           "/device/mount_path")))
        .Times(1);

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                         chromeos::FORMAT_ERROR_NONE,
                                         "/device/source_path"))
        .Times(1);

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                         chromeos::FORMAT_ERROR_NONE,
                                         "/device/source_path"))
        .Times(1);
  }

  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Wait for Unmount and Format calls to end.
  message_loop_.RunUntilIdle();

  EXPECT_EQ(1, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ("/device/mount_path",
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(1, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ("/device/source_path",
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat", fake_cros_disks_client_->last_format_filesystem());

  // The device should be unmounted by now.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));

  // Simulate cros_disks reporting success.
  fake_cros_disks_client_->SendFormatCompletedEvent(
      chromeos::FORMAT_ERROR_NONE, "/device/source_path");
}

// Tests that it's possible to format the device twice in a row (this may not be
// true if the list of pending formats is not properly cleared).
TEST_F(DiskMountManagerTest, Format_ConsecutiveFormatCalls) {
  // All unmount and format device cals are successful in this test.
  // Each of the should be made twice (once for each formatting task).

  // Set up expectations for observer mock.
  // The observer should receive UNMOUNTING, FORMAT_STARTED and FORMAT_COMPLETED
  // events (all of them without an error set) twice (once for each formatting
  // task).
  // Also, there should be a MOUNTING event when the device remounting is
  // simulated.
  EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_COMPLETED,
                                       chromeos::FORMAT_ERROR_NONE,
                                       "/device/source_path"))
      .Times(2);

  EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                       chromeos::FORMAT_ERROR_NONE,
                                       "/device/source_path"))
      .Times(2);

  EXPECT_CALL(observer_,
      OnMountEvent(DiskMountManager::UNMOUNTING,
                   chromeos::MOUNT_ERROR_NONE,
                   Field(&DiskMountManager::MountPointInfo::mount_path,
                         "/device/mount_path")))
      .Times(2);

  EXPECT_CALL(observer_,
      OnMountEvent(DiskMountManager::MOUNTING,
                   chromeos::MOUNT_ERROR_NONE,
                   Field(&DiskMountManager::MountPointInfo::mount_path,
                         "/device/mount_path")))
      .Times(1);

  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Wait for Unmount and Format calls to end.
  message_loop_.RunUntilIdle();

  EXPECT_EQ(1, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ("/device/mount_path",
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(1, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ("/device/source_path",
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat", fake_cros_disks_client_->last_format_filesystem());

  // The device should be unmounted by now.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));

  // Simulate cros_disks reporting success.
  fake_cros_disks_client_->SendFormatCompletedEvent(
      chromeos::FORMAT_ERROR_NONE, "/device/source_path");

  // Simulate the device remounting.
  fake_cros_disks_client_->SendMountCompletedEvent(
      chromeos::MOUNT_ERROR_NONE,
      "/device/source_path",
      chromeos::MOUNT_TYPE_DEVICE,
      "/device/mount_path");

  EXPECT_TRUE(HasMountPoint("/device/mount_path"));

  // Try formatting again.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Wait for Unmount and Format calls to end.
  message_loop_.RunUntilIdle();

  EXPECT_EQ(2, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ("/device/mount_path",
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(2, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ("/device/source_path",
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat", fake_cros_disks_client_->last_format_filesystem());

  // Simulate cros_disks reporting success.
  fake_cros_disks_client_->SendFormatCompletedEvent(
      chromeos::FORMAT_ERROR_NONE, "/device/source_path");
}

}  // namespace
