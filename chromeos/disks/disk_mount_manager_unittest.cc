// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "chromeos/dbus/mock_cros_disks_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::disks::DiskMountManager;
using chromeos::CrosDisksClient;
using chromeos::DBusThreadManager;
using chromeos::MockCrosDisksClient;
using chromeos::MockDBusThreadManager;
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
    false  // is hidden
  },
};

// List ofmount points  held in DiskMountManager at the begining of the test.
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

// Mocks response from CrosDisksClient::Unmount.
ACTION_P(MockUnmountPath, success) {
  CrosDisksClient::UnmountCallback callback = success ? arg2 : arg3;
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
                                              base::Bind(callback, arg0));
}

// Mocks response from CrosDisksClient::FormatDevice.
ACTION_P(MockFormatDevice, success) {
  if (success) {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
                                                base::Bind(arg2, arg0, true));
  } else {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE, base::Bind(arg3));
  }
}

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
    MockDBusThreadManager* mock_thread_manager = new MockDBusThreadManager();
    DBusThreadManager::InitializeForTesting(mock_thread_manager);

    mock_cros_disks_client_ = mock_thread_manager->mock_cros_disks_client();

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
  MockCrosDisksClient* mock_cros_disks_client_;
  MockDiskMountManagerObserver observer_;
  MessageLoopForUI message_loop_;
};

// Tests that the observer gets notified on attempt to format non existent mount
// point.
TEST_F(DiskMountManagerTest, Format_NotMounted) {
  EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                       chromeos::FORMAT_ERROR_UNKNOWN,
                                       "/mount/non_existent"))
      .Times(1);
  DiskMountManager::GetInstance()->FormatMountedDevice("/mount/non_existent");
}

// Tests that it is not possible to format archive mount point.
TEST_F(DiskMountManagerTest, Format_Archive) {
  EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                       chromeos::FORMAT_ERROR_UNKNOWN,
                                       "/archive/source_path"))
      .Times(1);

  DiskMountManager::GetInstance()->FormatMountedDevice("/archive/mount_path");
}

// Tests that format fails if the device cannot be unmounted.
TEST_F(DiskMountManagerTest, Format_FailToUnmount) {
  // Set up cros disks client mocks.
  // Before formatting mounted device, the device should be unmounted.
  // In this test unmount will fail, and there should be no attempt to
  // format the device.
  EXPECT_CALL(*mock_cros_disks_client_,
              Unmount("/device/mount_path", chromeos::UNMOUNT_OPTIONS_NONE,
                      _, _))
      .WillOnce(MockUnmountPath(false));

  EXPECT_CALL(*mock_cros_disks_client_,
              FormatDevice("/device/mount_path", _, _, _))
      .Times(0);

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

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                         chromeos::FORMAT_ERROR_UNKNOWN,
                                         "/device/source_path"))
        .Times(1);
  }

  // Start test.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Cros disks will respond asynchronoulsy, so let's drain the message loop.
  message_loop_.RunUntilIdle();

  // The device mount should still be here.
  EXPECT_TRUE(HasMountPoint("/device/mount_path"));
}

// Tests that observer is notified when cros disks fails to start format
// process.
TEST_F(DiskMountManagerTest, Format_FormatFailsToStart) {
  // Set up cros disks client mocks.
  // Before formatting mounted device, the device should be unmounted.
  // In this test, unmount will succeed, but call to FormatDevice method will
  // fail.
  EXPECT_CALL(*mock_cros_disks_client_,
              Unmount("/device/mount_path", chromeos::UNMOUNT_OPTIONS_NONE,
                      _, _))
      .WillOnce(MockUnmountPath(true));

  EXPECT_CALL(*mock_cros_disks_client_,
              FormatDevice("/device/source_path", "vfat", _, _))
      .WillOnce(MockFormatDevice(false));

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

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                         chromeos::FORMAT_ERROR_UNKNOWN,
                                         "/device/source_path"))
        .Times(1);
  }

  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Cros disks will respond asynchronoulsy, so let's drain the message loop.
  message_loop_.RunUntilIdle();

  // The device mount should be gone.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));
}

// Tests the case where there are two format requests for the same device.
TEST_F(DiskMountManagerTest, Format_ConcurrentFormatCalls) {
  // Set up cros disks client mocks.
  // Only the first format request should be processed (for the other one there
  // should be an error before device unmount is attempted).
  // CrosDisksClient will report that the format process for the first request
  // is successfully started.
  EXPECT_CALL(*mock_cros_disks_client_,
              Unmount("/device/mount_path", chromeos::UNMOUNT_OPTIONS_NONE,
                      _, _))
      .WillOnce(MockUnmountPath(true));

  EXPECT_CALL(*mock_cros_disks_client_,
              FormatDevice("/device/source_path", "vfat", _, _))
      .WillOnce(MockFormatDevice(true));

  // Set up expectations for observer mock.
  // The observer should get two FORMAT_STARTED events, one for each format
  // request, but with different error codes (the formatting will be started
  // only for the first request).
  // There should alos be one UNMOUNTING event, since the device should get
  // unmounted for the first request.
  //
  // Note that in this test the format completion signal will not be simulated,
  // so the observer should not get FORMAT_COMPLETED signal.
  {
    InSequence s;

    EXPECT_CALL(observer_, OnFormatEvent(DiskMountManager::FORMAT_STARTED,
                                         chromeos::FORMAT_ERROR_UNKNOWN,
                                         "/device/source_path"))
        .Times(1);

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
  }

  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Cros disks will respond asynchronoulsy, so let's drain the message loop.
  message_loop_.RunUntilIdle();

  // The device mount should be gone.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));
}

// Tests the case when the format process actually starts and fails.
TEST_F(DiskMountManagerTest, Format_FormatFails) {
  // Set up cros disks client mocks.
  // Both unmount and format device cals are successfull in this test.
  EXPECT_CALL(*mock_cros_disks_client_,
              Unmount("/device/mount_path", chromeos::UNMOUNT_OPTIONS_NONE,
                      _, _))
      .WillOnce(MockUnmountPath(true));

  EXPECT_CALL(*mock_cros_disks_client_,
              FormatDevice("/device/source_path", "vfat", _, _))
      .WillOnce(MockFormatDevice(true));

  // Set up expectations for observer mock.
  // The observer should get notified that the device was unmounted and that
  // formatting has started.
  // After the formatting starts, the test will simulate failing
  // FORMATTING_FINISHED signal, so the observer should also be notified the
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

  // Wait for Unmount and FormatDevice calls to end.
  message_loop_.RunUntilIdle();

  // The device should be unmounted by now.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));

  // Send failing FORMATTING_FINISHED signal.
  // The failure is marked by ! in fromt of the path (but this should change
  // soon).
  mock_cros_disks_client_->SendMountEvent(
      chromeos::CROS_DISKS_FORMATTING_FINISHED, "!/device/source_path");
}

// Tests the same case as Format_FormatFails, but the FORMATTING_FINISHED event
// is sent with file_path of the formatted device (instead of its device path).
TEST_F(DiskMountManagerTest, Format_FormatFailsAndReturnFilePath) {
  // Set up cros disks client mocks.
  EXPECT_CALL(*mock_cros_disks_client_,
              Unmount("/device/mount_path", chromeos::UNMOUNT_OPTIONS_NONE,
                      _, _))
      .WillOnce(MockUnmountPath(true));

  EXPECT_CALL(*mock_cros_disks_client_,
              FormatDevice("/device/source_path", "vfat", _, _))
      .WillOnce(MockFormatDevice(true));

  // Set up expectations for observer mock.
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

  // Start test.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Wait for Unmount and FormatDevice calls to end.
  message_loop_.RunUntilIdle();

  // The device should be unmounted by now.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));

  // Send failing FORMATTING_FINISHED signal with the device's file path.
  mock_cros_disks_client_->SendMountEvent(
      chromeos::CROS_DISKS_FORMATTING_FINISHED, "!/device/file_path");
}

// Tests the case when formatting completes successfully.
TEST_F(DiskMountManagerTest, Format_FormatSuccess) {
  // Set up cros disks client mocks.
  // Both unmount and format device cals are successfull in this test.
  EXPECT_CALL(*mock_cros_disks_client_,
              Unmount("/device/mount_path", chromeos::UNMOUNT_OPTIONS_NONE,
                      _, _))
      .WillOnce(MockUnmountPath(true));

  EXPECT_CALL(*mock_cros_disks_client_,
              FormatDevice("/device/source_path", "vfat", _, _))
      .WillOnce(MockFormatDevice(true));

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

  // Wait for Unmount and FormatDevice calls to end.
  message_loop_.RunUntilIdle();

  // The device should be unmounted by now.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));

  // Simulate cros_disks reporting success.
  mock_cros_disks_client_->SendMountEvent(
      chromeos::CROS_DISKS_FORMATTING_FINISHED, "/device/source_path");
}

// Tests that it's possible to format the device twice in a row (this may not be
// true if the list of pending formats is not properly cleared).
TEST_F(DiskMountManagerTest, Format_ConsecutiveFormatCalls) {
  // Set up cros disks client mocks.
  // All unmount and format device cals are successfull in this test.
  // Each of the should be made twice (once for each formatting task).
  EXPECT_CALL(*mock_cros_disks_client_,
              Unmount("/device/mount_path", chromeos::UNMOUNT_OPTIONS_NONE,
                      _, _))
      .WillOnce(MockUnmountPath(true))
      .WillOnce(MockUnmountPath(true));

  EXPECT_CALL(*mock_cros_disks_client_,
      FormatDevice("/device/source_path", "vfat", _, _))
      .WillOnce(MockFormatDevice(true))
      .WillOnce(MockFormatDevice(true));

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

  // Wait for Unmount and FormatDevice calls to end.
  message_loop_.RunUntilIdle();

  // The device should be unmounted by now.
  EXPECT_FALSE(HasMountPoint("/device/mount_path"));

  // Simulate cros_disks reporting success.
  mock_cros_disks_client_->SendMountEvent(
      chromeos::CROS_DISKS_FORMATTING_FINISHED, "/device/source_path");

  // Simulate the device remounting.
  mock_cros_disks_client_->SendMountCompletedEvent(
      chromeos::MOUNT_ERROR_NONE,
      "/device/source_path",
      chromeos::MOUNT_TYPE_DEVICE,
      "/device/mount_path");

  EXPECT_TRUE(HasMountPoint("/device/mount_path"));

  // Try formatting again.
  DiskMountManager::GetInstance()->FormatMountedDevice("/device/mount_path");

  // Wait for Unmount and FormatDevice calls to end.
  message_loop_.RunUntilIdle();

  // Simulate cros_disks reporting success.
  mock_cros_disks_client_->SendMountEvent(
      chromeos::CROS_DISKS_FORMATTING_FINISHED, "/device/source_path");
}

}  // namespace

