// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_scheduler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MakeUnique;
using base::StringPrintf;
using chromeos::disks::DiskMountManager;
using chromeos::CrosDisksClient;
using chromeos::DBusThreadManager;
using chromeos::FakeCrosDisksClient;
using chromeos::MountType;
using chromeos::disks::MountCondition;

namespace {

const char kDevice1SourcePath[] = "/device/source_path";
const char kDevice1MountPath[] = "/device/mount_path";
const char kDevice2SourcePath[] = "/device/source_path2";
const char kReadOnlyDeviceMountPath[] = "/device/read_only_mount_path";
const char kReadOnlyDeviceSourcePath[] = "/device/read_only_source_path";

// Holds information needed to create a DiskMountManager::Disk instance.
struct TestDiskInfo {
  const char* source_path;
  const char* mount_path;
  bool write_disabled_by_policy;
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
  uint64_t size_in_bytes;
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
    kDevice1SourcePath,
    kDevice1MountPath,
    false,  // write_disabled_by_policy
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
    false,       // is parent
    false,       // is read only
    true,        // has media
    false,       // is on boot device
    true,        // is on removable device
    false        // is hidden
  },
  {
    kDevice2SourcePath,
    "",  // not mounted initially
    false,  // write_disabled_by_policy
    "/device/prefix/system_path2",
    "/device/file_path2",
    "/device/device_label2",
    "/device/drive_label2",
    "/device/vendor_id2",
    "/device/vendor_name2",
    "/device/product_id2",
    "/device/product_name2",
    "/device/fs_uuid2",
    "/device/prefix2",
    chromeos::DEVICE_TYPE_SD,
    1073741824,  // size in bytes
    false,       // is parent
    false,       // is read only
    true,        // has media
    false,       // is on boot device
    true,        // is on removable device
    false        // is hidden
  },
  {
    kReadOnlyDeviceSourcePath,
    kReadOnlyDeviceMountPath,
    false,  // write_disabled_by_policy
    "/device/prefix/system_path_3",
    "/device/file_path_3",
    "/device/device_label_3",
    "/device/drive_label_3",
    "/device/vendor_id_3",
    "/device/vendor_name_3",
    "/device/product_id_3",
    "/device/product_name_3",
    "/device/fs_uuid_3",
    "/device/prefix",
    chromeos::DEVICE_TYPE_USB,
    1073741824,  // size in bytes
    false,       // is parent
    true,        // is read only
    true,        // has media
    false,       // is on boot device
    true,        // is on removable device
    false        // is hidden
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
    kDevice1SourcePath,
    kDevice1MountPath,
    chromeos::MOUNT_TYPE_DEVICE,
    chromeos::disks::MOUNT_CONDITION_NONE
  },
  {
    kReadOnlyDeviceSourcePath,
    kReadOnlyDeviceMountPath,
    chromeos::MOUNT_TYPE_DEVICE,
    chromeos::disks::MOUNT_CONDITION_NONE
  },
};

// Represents which function in |DiskMountManager::Observer| was invoked.
enum ObserverEventType {
  DEVICE_EVENT,  // OnDeviceEvent()
  DISK_EVENT,    // OnDiskEvent()
  FORMAT_EVENT,  // OnFormatEvent()
  MOUNT_EVENT    // OnMountEvent()
};

// Represents every event notified to |DiskMountManager::Observer|.
struct ObserverEvent {
 public:
  virtual ObserverEventType type() const = 0;
  virtual ~ObserverEvent() {}
};

// Represents an invocation of |DiskMountManager::Observer::OnDeviceEvent()|.
struct DeviceEvent : public ObserverEvent {
  DiskMountManager::DeviceEvent event;
  std::string device_path;

  DeviceEvent() {}

  DeviceEvent(DiskMountManager::DeviceEvent event,
              const std::string& device_path)
      : event(event), device_path(device_path) {}

  ObserverEventType type() const override { return DEVICE_EVENT; }

  bool operator==(const DeviceEvent& other) const {
    return event == other.event && device_path == other.device_path;
  }

  std::string DebugString() const {
    return StringPrintf("OnDeviceEvent(%d, %s)", event, device_path.c_str());
  }
};

// Represents an invocation of |DiskMountManager::Observer::OnDiskEvent()|.
struct DiskEvent : public ObserverEvent {
  DiskMountManager::DiskEvent event;
  std::unique_ptr<DiskMountManager::Disk> disk;

  DiskEvent(DiskMountManager::DiskEvent event,
            const DiskMountManager::Disk& disk)
      : event(event),
        disk(std::unique_ptr<DiskMountManager::Disk>(
            new DiskMountManager::Disk(disk))) {}

  DiskEvent(DiskEvent&& other)
      : event(other.event), disk(std::move(other.disk)) {}

  ObserverEventType type() const override { return DISK_EVENT; }

  bool operator==(const DiskEvent& other) const {
    return event == other.event && disk == other.disk;
  }

  std::string DebugString() const {
    return StringPrintf("OnDiskEvent(event=%d, device_path=%s, mount_path=%s",
                        event, disk->device_path().c_str(),
                        disk->mount_path().c_str());
  }
};

// Represents an invocation of |DiskMountManager::Observer::OnFormatEvent()|.
struct FormatEvent : public ObserverEvent {
  DiskMountManager::FormatEvent event;
  chromeos::FormatError error_code;
  std::string device_path;

  FormatEvent() {}
  FormatEvent(DiskMountManager::FormatEvent event,
              chromeos::FormatError error_code,
              const std::string& device_path)
      : event(event), error_code(error_code), device_path(device_path) {}

  ObserverEventType type() const override { return FORMAT_EVENT; }

  bool operator==(const FormatEvent& other) const {
    return event == other.event && error_code == other.error_code &&
           device_path == other.device_path;
  }

  std::string DebugString() const {
    return StringPrintf("OnFormatEvent(%d, %d, %s)", event, error_code,
                        device_path.c_str());
  }
};

// Represents an invocation of |DiskMountManager::Observer::OnMountEvent()|.
struct MountEvent : public ObserverEvent {
  DiskMountManager::MountEvent event;
  chromeos::MountError error_code;
  DiskMountManager::MountPointInfo mount_point;

  // Not passed to callback, but read by handlers. So it's captured upon
  // callback.
  std::unique_ptr<DiskMountManager::Disk> disk;

  MountEvent(MountEvent&& other)
      : event(other.event),
        error_code(other.error_code),
        mount_point(other.mount_point),
        disk(std::move(other.disk)) {}
  MountEvent(DiskMountManager::MountEvent event,
             chromeos::MountError error_code,
             const DiskMountManager::MountPointInfo& mount_point,
             const DiskMountManager::Disk& disk)
      : event(event),
        error_code(error_code),
        mount_point(mount_point),
        disk(new DiskMountManager::Disk(disk)) {}

  ObserverEventType type() const override { return MOUNT_EVENT; }

  bool operator==(const MountEvent& other) const;

  std::string DebugString() const {
    return StringPrintf("OnMountEvent(%d, %d, %s, %s, %d, %d)", event,
                        error_code, mount_point.source_path.c_str(),
                        mount_point.mount_path.c_str(), mount_point.mount_type,
                        mount_point.mount_condition);
  }
};

// A mock |Observer| class which records all invocation of the methods invoked
// from DiskMountManager and all the arguments passed to them.
class MockDiskMountManagerObserver : public DiskMountManager::Observer {
 public:
  explicit MockDiskMountManagerObserver(const DiskMountManager* manager)
      : manager_(manager) {}
  ~MockDiskMountManagerObserver() override {}

  // Mock notify methods.
  void OnDeviceEvent(DiskMountManager::DeviceEvent event,
                     const std::string& device_path) override {
    events_.push_back(MakeUnique<DeviceEvent>(event, device_path));
  }

  void OnDiskEvent(DiskMountManager::DiskEvent event,
                   const DiskMountManager::Disk* disk) override {
    // Take a snapshot (copy) of the Disk object at the time of invocation for
    // later verification.
    events_.push_back(MakeUnique<DiskEvent>(event, *disk));
  }

  void OnFormatEvent(DiskMountManager::FormatEvent event,
                     chromeos::FormatError error_code,
                     const std::string& device_path) override {
    events_.push_back(MakeUnique<FormatEvent>(event, error_code, device_path));
  }

  void OnMountEvent(
      DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const DiskMountManager::MountPointInfo& mount_point) override {
    // Take a snapshot (copy) of a Disk object at the time of invocation.
    // It can be verified later besides the arguments.
    events_.push_back(MakeUnique<MountEvent>(
        event, error_code, mount_point,
        *manager_->disks().find(mount_point.source_path)->second));
  }

  // Gets invocation history to be verified by testcases.
  // Verifies if the |index|th invocation is OnDeviceEvent() and returns
  // details.
  const DeviceEvent& GetDeviceEvent(size_t index) {
    DCHECK_GT(events_.size(), index);
    DCHECK_EQ(DEVICE_EVENT, events_[index]->type());
    return static_cast<const DeviceEvent&>(*events_[index]);
  }

  // Verifies if the |index|th invocation is OnDiskEvent() and returns details.
  const DiskEvent& GetDiskEvent(size_t index) {
    DCHECK_GT(events_.size(), index);
    DCHECK_EQ(DISK_EVENT, events_[index]->type());
    return static_cast<const DiskEvent&>(*events_[index]);
  }

  // Verifies if the |index|th invocation is OnFormatEvent() and returns
  // details.
  const FormatEvent& GetFormatEvent(size_t index) {
    DCHECK_GT(events_.size(), index);
    DCHECK_EQ(FORMAT_EVENT, events_[index]->type());
    return static_cast<const FormatEvent&>(*events_[index]);
  }

  // Verifies if the |index|th invocation is OnMountEvent() and returns details.
  const MountEvent& GetMountEvent(size_t index) {
    DCHECK_GT(events_.size(), index);
    DCHECK_EQ(MOUNT_EVENT, events_[index]->type());
    return static_cast<const MountEvent&>(*events_[index]);
  }

  // Returns number of callback invocations happened so far.
  size_t GetEventCount() { return events_.size(); }

  // Counts the number of |MountEvent| recorded so far that matches the given
  // condition.
  size_t CountMountEvents(DiskMountManager::MountEvent mount_event_type,
                          chromeos::MountError error_code,
                          const std::string& mount_path) {
    size_t num_matched = 0;
    for (const auto& it : events_) {
      if (it->type() != MOUNT_EVENT)
        continue;
      const MountEvent& mount_event = static_cast<const MountEvent&>(*it);
      if (mount_event.event == mount_event_type &&
          mount_event.error_code == error_code &&
          mount_event.mount_point.mount_path == mount_path)
        num_matched++;
    }
    return num_matched;
  }

  // Counts the number of |FormatEvent| recorded so far that matches with
  // |format_event|.
  size_t CountFormatEvents(const FormatEvent& exptected_format_event) {
    size_t num_matched = 0;
    for (const auto& it : events_) {
      if (it->type() != FORMAT_EVENT)
        continue;
      if (static_cast<const FormatEvent&>(*it) == exptected_format_event)
        num_matched++;
    }
    return num_matched;
  }

 private:
  // Pointer to the manager object to which this |Observer| is registered.
  const DiskMountManager* manager_;

  // Records all invocations.
  std::vector<std::unique_ptr<ObserverEvent>> events_;
};

// Shift operators of ostream.
// Needed to print values in case of EXPECT_* failure in gtest.
std::ostream& operator<<(std::ostream& stream,
                         const FormatEvent& format_event) {
  return stream << format_event.DebugString();
}

class DiskMountManagerTest : public testing::Test {
 public:
  DiskMountManagerTest() : scoped_task_scheduler_(&message_loop_) {}
  ~DiskMountManagerTest() override {}

  // Sets up test dbus tread manager and disks mount manager.
  // Initializes disk mount manager disks and mount points.
  // Adds a test observer to the disk mount manager.
  void SetUp() override {
    fake_cros_disks_client_ = new FakeCrosDisksClient;
    DBusThreadManager::GetSetterForTesting()->SetCrosDisksClient(
        std::unique_ptr<CrosDisksClient>(fake_cros_disks_client_));

    DiskMountManager::Initialize();

    InitDisksAndMountPoints();

    observer_.reset(
        new MockDiskMountManagerObserver(DiskMountManager::GetInstance()));
    DiskMountManager::GetInstance()->AddObserver(observer_.get());
  }

  // Shuts down dbus thread manager and disk moutn manager used in the test.
  void TearDown() override {
    DiskMountManager::GetInstance()->RemoveObserver(observer_.get());
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
        base::MakeUnique<DiskMountManager::Disk>(
            disk.source_path, disk.mount_path, disk.write_disabled_by_policy,
            disk.system_path, disk.file_path, disk.device_label,
            disk.drive_label, disk.vendor_id, disk.vendor_name, disk.product_id,
            disk.product_name, disk.fs_uuid, disk.system_path_prefix,
            disk.device_type, disk.size_in_bytes, disk.is_parent,
            disk.is_read_only, disk.has_media, disk.on_boot_device,
            disk.on_removable_device, disk.is_hidden)));
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
  std::unique_ptr<MockDiskMountManagerObserver> observer_;

 private:
  base::MessageLoopForUI message_loop_;
  base::test::ScopedTaskScheduler scoped_task_scheduler_;
};

// Tests that the observer gets notified on attempt to format non existent mount
// point.
TEST_F(DiskMountManagerTest, Format_NotMounted) {
  DiskMountManager::GetInstance()->FormatMountedDevice("/mount/non_existent");
  ASSERT_EQ(1U, observer_->GetEventCount());
  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_COMPLETED,
                        chromeos::FORMAT_ERROR_UNKNOWN, "/mount/non_existent"),
            observer_->GetFormatEvent(0));
}

// Tests that the observer gets notified on attempt to format read-only mount
// point.
TEST_F(DiskMountManagerTest, Format_ReadOnly) {
  DiskMountManager::GetInstance()->FormatMountedDevice(
      kReadOnlyDeviceMountPath);
  ASSERT_EQ(1U, observer_->GetEventCount());
  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_COMPLETED,
                        chromeos::FORMAT_ERROR_DEVICE_NOT_ALLOWED,
                        kReadOnlyDeviceMountPath),
            observer_->GetFormatEvent(0));
}

// Tests that it is not possible to format archive mount point.
TEST_F(DiskMountManagerTest, Format_Archive) {
  DiskMountManager::GetInstance()->FormatMountedDevice("/archive/mount_path");
  ASSERT_EQ(1U, observer_->GetEventCount());
  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_COMPLETED,
                        chromeos::FORMAT_ERROR_UNKNOWN, "/archive/source_path"),
            observer_->GetFormatEvent(0));
}

// Tests that format fails if the device cannot be unmounted.
TEST_F(DiskMountManagerTest, Format_FailToUnmount) {
  // Before formatting mounted device, the device should be unmounted.
  // In this test unmount will fail, and there should be no attempt to
  // format the device.

  fake_cros_disks_client_->MakeUnmountFail();
  // Start test.
  DiskMountManager::GetInstance()->FormatMountedDevice(kDevice1MountPath);

  // Cros disks will respond asynchronoulsy, so let's drain the message loop.
  base::RunLoop().RunUntilIdle();

  // Observer should be notified that unmount attempt fails and format task
  // failed to start.
  ASSERT_EQ(2U, observer_->GetEventCount());
  const MountEvent& mount_event = observer_->GetMountEvent(0);
  EXPECT_EQ(DiskMountManager::UNMOUNTING, mount_event.event);
  EXPECT_EQ(chromeos::MOUNT_ERROR_INTERNAL, mount_event.error_code);
  EXPECT_EQ(kDevice1MountPath, mount_event.mount_point.mount_path);

  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_COMPLETED,
                        chromeos::FORMAT_ERROR_UNKNOWN, kDevice1SourcePath),
            observer_->GetFormatEvent(1));
  EXPECT_EQ(1, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ(kDevice1MountPath,
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(0, fake_cros_disks_client_->format_call_count());

  // The device mount should still be here.
  EXPECT_TRUE(HasMountPoint(kDevice1MountPath));
}

// Tests that observer is notified when cros disks fails to start format
// process.
TEST_F(DiskMountManagerTest, Format_FormatFailsToStart) {
  // Before formatting mounted device, the device should be unmounted.
  // In this test, unmount will succeed, but call to Format method will
  // fail.

  fake_cros_disks_client_->MakeFormatFail();
  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice(kDevice1MountPath);

  // Cros disks will respond asynchronoulsy, so let's drain the message loop.
  base::RunLoop().RunUntilIdle();

  // Observer should be notified that the device was unmounted and format task
  // failed to start.
  ASSERT_EQ(2U, observer_->GetEventCount());
  const MountEvent& mount_event = observer_->GetMountEvent(0);
  EXPECT_EQ(DiskMountManager::UNMOUNTING, mount_event.event);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, mount_event.error_code);
  EXPECT_EQ(kDevice1MountPath, mount_event.mount_point.mount_path);

  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_COMPLETED,
                        chromeos::FORMAT_ERROR_UNKNOWN, kDevice1SourcePath),
            observer_->GetFormatEvent(1));

  EXPECT_EQ(1, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ(kDevice1MountPath,
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(1, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ(kDevice1SourcePath,
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat", fake_cros_disks_client_->last_format_filesystem());

  // The device mount should be gone.
  EXPECT_FALSE(HasMountPoint(kDevice1MountPath));
}

// Tests the case where there are two format requests for the same device.
TEST_F(DiskMountManagerTest, Format_ConcurrentFormatCalls) {
  // Only the first format request should be processed (the second unmount
  // request fails because the device is already unmounted at that point).
  // CrosDisksClient will report that the format process for the first request
  // is successfully started.

  fake_cros_disks_client_->set_unmount_listener(
      base::Bind(&FakeCrosDisksClient::MakeUnmountFail,
                 base::Unretained(fake_cros_disks_client_)));
  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice(kDevice1MountPath);
  DiskMountManager::GetInstance()->FormatMountedDevice(kDevice1MountPath);

  // Cros disks will respond asynchronoulsy, so let's drain the message loop.
  base::RunLoop().RunUntilIdle();

  // The observer should get a FORMAT_STARTED event for one format request and a
  // FORMAT_COMPLETED with an error code for the other format request. The
  // formatting will be started only for the first request.
  // There should be only one UNMOUNTING event. The result of the second one
  // should not be reported as the mount point will go away after the first
  // request.
  //
  // Note that in this test the format completion signal will not be simulated,
  // so the observer should not get FORMAT_COMPLETED signal.

  ASSERT_EQ(3U, observer_->GetEventCount());
  const MountEvent& mount_event = observer_->GetMountEvent(0);
  EXPECT_EQ(DiskMountManager::UNMOUNTING, mount_event.event);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, mount_event.error_code);
  EXPECT_EQ(kDevice1MountPath, mount_event.mount_point.mount_path);
  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_COMPLETED,
                        chromeos::FORMAT_ERROR_UNKNOWN, kDevice1SourcePath),
            observer_->GetFormatEvent(1));
  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_STARTED,
                        chromeos::FORMAT_ERROR_NONE, kDevice1SourcePath),
            observer_->GetFormatEvent(2));

  EXPECT_EQ(2, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ(kDevice1MountPath,
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(1, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ(kDevice1SourcePath,
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat",
            fake_cros_disks_client_->last_format_filesystem());

  // The device mount should be gone.
  EXPECT_FALSE(HasMountPoint(kDevice1MountPath));
}

// Verifies a |MountEvent| with the given condition. This function only checks
// the |mount_path| in |MountPointInfo| to make sure to match the event with
// preceding mount invocations.
void VerifyMountEvent(const MountEvent& mount_event,
                      DiskMountManager::MountEvent mount_event_type,
                      chromeos::MountError error_code,
                      const std::string& mount_path) {
  EXPECT_EQ(mount_event_type, mount_event.event);
  EXPECT_EQ(error_code, mount_event.error_code);
  EXPECT_EQ(mount_path, mount_event.mount_point.mount_path);
}

// Tests the case when the format process actually starts and fails.
TEST_F(DiskMountManagerTest, Format_FormatFails) {
  // Both unmount and format device cals are successful in this test.

  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice(kDevice1MountPath);

  // Wait for Unmount and Format calls to end.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ(kDevice1MountPath,
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(1, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ(kDevice1SourcePath,
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat", fake_cros_disks_client_->last_format_filesystem());

  // The device should be unmounted by now.
  EXPECT_FALSE(HasMountPoint(kDevice1MountPath));

  // Send failing FORMAT_COMPLETED signal.
  // The failure is marked by ! in fromt of the path (but this should change
  // soon).
  fake_cros_disks_client_->SendFormatCompletedEvent(
      chromeos::FORMAT_ERROR_UNKNOWN, kDevice1SourcePath);

  // The observer should get notified that the device was unmounted and that
  // formatting has started.
  // After the formatting starts, the test will simulate failing
  // FORMAT_COMPLETED signal, so the observer should also be notified the
  // formatting has failed (FORMAT_COMPLETED event).
  ASSERT_EQ(3U, observer_->GetEventCount());
  VerifyMountEvent(observer_->GetMountEvent(0), DiskMountManager::UNMOUNTING,
                   chromeos::MOUNT_ERROR_NONE, kDevice1MountPath);
  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_STARTED,
                        chromeos::FORMAT_ERROR_NONE, kDevice1SourcePath),
            observer_->GetFormatEvent(1));
  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_COMPLETED,
                        chromeos::FORMAT_ERROR_UNKNOWN, kDevice1SourcePath),
            observer_->GetFormatEvent(2));
}

// Tests the case when formatting completes successfully.
TEST_F(DiskMountManagerTest, Format_FormatSuccess) {
  // Set up cros disks client mocks.
  // Both unmount and format device cals are successful in this test.

  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice(kDevice1MountPath);

  // Wait for Unmount and Format calls to end.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ(kDevice1MountPath,
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(1, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ(kDevice1SourcePath,
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat", fake_cros_disks_client_->last_format_filesystem());

  // The device should be unmounted by now.
  EXPECT_FALSE(HasMountPoint(kDevice1MountPath));

  // Simulate cros_disks reporting success.
  fake_cros_disks_client_->SendFormatCompletedEvent(chromeos::FORMAT_ERROR_NONE,
                                                    kDevice1SourcePath);

  // The observer should receive UNMOUNTING, FORMAT_STARTED and FORMAT_COMPLETED
  // events (all of them without an error set).
  ASSERT_EQ(3U, observer_->GetEventCount());
  VerifyMountEvent(observer_->GetMountEvent(0), DiskMountManager::UNMOUNTING,
                   chromeos::MOUNT_ERROR_NONE, kDevice1MountPath);
  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_STARTED,
                        chromeos::FORMAT_ERROR_NONE, kDevice1SourcePath),
            observer_->GetFormatEvent(1));
  EXPECT_EQ(FormatEvent(DiskMountManager::FORMAT_COMPLETED,
                        chromeos::FORMAT_ERROR_NONE, kDevice1SourcePath),
            observer_->GetFormatEvent(2));
}

// Tests that it's possible to format the device twice in a row (this may not be
// true if the list of pending formats is not properly cleared).
TEST_F(DiskMountManagerTest, Format_ConsecutiveFormatCalls) {
  // All unmount and format device cals are successful in this test.
  // Each of the should be made twice (once for each formatting task).

  // Start the test.
  DiskMountManager::GetInstance()->FormatMountedDevice(kDevice1MountPath);

  // Wait for Unmount and Format calls to end.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ(kDevice1MountPath,
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(1, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ(kDevice1SourcePath,
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat", fake_cros_disks_client_->last_format_filesystem());

  // The device should be unmounted by now.
  EXPECT_FALSE(HasMountPoint(kDevice1MountPath));

  // Simulate cros_disks reporting success.
  fake_cros_disks_client_->SendFormatCompletedEvent(chromeos::FORMAT_ERROR_NONE,
                                                    kDevice1SourcePath);

  // Simulate the device remounting.
  fake_cros_disks_client_->SendMountCompletedEvent(
      chromeos::MOUNT_ERROR_NONE, kDevice1SourcePath,
      chromeos::MOUNT_TYPE_DEVICE, kDevice1MountPath);

  EXPECT_TRUE(HasMountPoint(kDevice1MountPath));

  // Try formatting again.
  DiskMountManager::GetInstance()->FormatMountedDevice(kDevice1MountPath);

  // Wait for Unmount and Format calls to end.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, fake_cros_disks_client_->unmount_call_count());
  EXPECT_EQ(kDevice1MountPath,
            fake_cros_disks_client_->last_unmount_device_path());
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_NONE,
            fake_cros_disks_client_->last_unmount_options());
  EXPECT_EQ(2, fake_cros_disks_client_->format_call_count());
  EXPECT_EQ(kDevice1SourcePath,
            fake_cros_disks_client_->last_format_device_path());
  EXPECT_EQ("vfat", fake_cros_disks_client_->last_format_filesystem());

  // Simulate cros_disks reporting success.
  fake_cros_disks_client_->SendFormatCompletedEvent(chromeos::FORMAT_ERROR_NONE,
                                                    kDevice1SourcePath);

  // The observer should receive UNMOUNTING, FORMAT_STARTED and FORMAT_COMPLETED
  // events (all of them without an error set) twice (once for each formatting
  // task).
  // Also, there should be a MOUNTING event when the device remounting is
  // simulated.
  EXPECT_EQ(7U, observer_->GetEventCount());

  EXPECT_EQ(2U, observer_->CountFormatEvents(FormatEvent(
                    DiskMountManager::FORMAT_COMPLETED,
                    chromeos::FORMAT_ERROR_NONE, kDevice1SourcePath)));

  EXPECT_EQ(2U, observer_->CountFormatEvents(FormatEvent(
                    DiskMountManager::FORMAT_STARTED,
                    chromeos::FORMAT_ERROR_NONE, kDevice1SourcePath)));

  EXPECT_EQ(2U, observer_->CountMountEvents(DiskMountManager::UNMOUNTING,
                                            chromeos::MOUNT_ERROR_NONE,
                                            kDevice1MountPath));

  EXPECT_EQ(1U, observer_->CountMountEvents(DiskMountManager::MOUNTING,
                                            chromeos::MOUNT_ERROR_NONE,
                                            kDevice1MountPath));
}

TEST_F(DiskMountManagerTest, MountPath_RecordAccessMode) {
  DiskMountManager* manager = DiskMountManager::GetInstance();
  const std::string kSourcePath1 = kDevice1SourcePath;
  const std::string kSourcePath2 = kDevice2SourcePath;
  const std::string kSourceFormat = std::string();
  const std::string kMountLabel = std::string();  // N/A for MOUNT_TYPE_DEVICE
  // For MountCompleted. Must be non-empty strings.
  const std::string kMountPath1 = "/media/foo";
  const std::string kMountPath2 = "/media/bar";

  manager->MountPath(kSourcePath1, kSourceFormat, std::string(),
                     chromeos::MOUNT_TYPE_DEVICE,
                     chromeos::MOUNT_ACCESS_MODE_READ_WRITE);
  manager->MountPath(kSourcePath2, kSourceFormat, std::string(),
                     chromeos::MOUNT_TYPE_DEVICE,
                     chromeos::MOUNT_ACCESS_MODE_READ_ONLY);
  // Simulate cros_disks reporting mount completed.
  fake_cros_disks_client_->SendMountCompletedEvent(
      chromeos::MOUNT_ERROR_NONE, kSourcePath1, chromeos::MOUNT_TYPE_DEVICE,
      kMountPath1);
  fake_cros_disks_client_->SendMountCompletedEvent(
      chromeos::MOUNT_ERROR_NONE, kSourcePath2, chromeos::MOUNT_TYPE_DEVICE,
      kMountPath2);

  // Event handlers of observers should be called.
  ASSERT_EQ(2U, observer_->GetEventCount());
  VerifyMountEvent(observer_->GetMountEvent(0), DiskMountManager::MOUNTING,
                   chromeos::MOUNT_ERROR_NONE, kMountPath1);
  // For the 2nd source, the disk (block device) is not read-only but the
  // test will mount it in read-only mode.
  // Observers query |disks_| from |DiskMountManager| in its event handler for
  // a mount completion event. Therefore |disks_| must be updated with correct
  // |read_only| value before notifying to observers.
  const MountEvent& secondMountEvent = observer_->GetMountEvent(1);
  EXPECT_EQ(DiskMountManager::MOUNTING, secondMountEvent.event);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, secondMountEvent.error_code);
  EXPECT_EQ(kMountPath2, secondMountEvent.mount_point.mount_path);
  // Verify if the disk appears read-only at the time of notification to
  // observers.
  EXPECT_TRUE(secondMountEvent.disk->is_read_only());

  // Verify the final state of manager->disks.
  const DiskMountManager::DiskMap& disks = manager->disks();
  ASSERT_GT(disks.count(kSourcePath1), 0U);
  EXPECT_FALSE(disks.find(kSourcePath1)->second->is_read_only());
  ASSERT_GT(disks.count(kSourcePath2), 0U);
  EXPECT_TRUE(disks.find(kSourcePath2)->second->is_read_only());
}

TEST_F(DiskMountManagerTest, MountPath_ReadOnlyDevice) {
  DiskMountManager* manager = DiskMountManager::GetInstance();
  const std::string kSourceFormat = std::string();
  const std::string kMountLabel = std::string();  // N/A for MOUNT_TYPE_DEVICE

  // Attempt to mount a read-only device in read-write mode.
  manager->MountPath(kReadOnlyDeviceSourcePath, kSourceFormat, std::string(),
                     chromeos::MOUNT_TYPE_DEVICE,
                     chromeos::MOUNT_ACCESS_MODE_READ_WRITE);
  // Simulate cros_disks reporting mount completed.
  fake_cros_disks_client_->SendMountCompletedEvent(
      chromeos::MOUNT_ERROR_NONE, kReadOnlyDeviceSourcePath,
      chromeos::MOUNT_TYPE_DEVICE, kReadOnlyDeviceMountPath);

  // Event handlers of observers should be called.
  ASSERT_EQ(1U, observer_->GetEventCount());
  VerifyMountEvent(observer_->GetMountEvent(0), DiskMountManager::MOUNTING,
                   chromeos::MOUNT_ERROR_NONE, kReadOnlyDeviceMountPath);
  const DiskMountManager::DiskMap& disks = manager->disks();
  ASSERT_GT(disks.count(kReadOnlyDeviceSourcePath), 0U);
  // The mounted disk should preserve the read-only flag of the block device.
  EXPECT_TRUE(disks.find(kReadOnlyDeviceSourcePath)->second->is_read_only());
}

TEST_F(DiskMountManagerTest, RemountRemovableDrives) {
  DiskMountManager* manager = DiskMountManager::GetInstance();
  // Initially we have 2 mounted devices.
  // kDevice1MountPath --- read-write device, mounted in read-write mode.
  // kReadOnlyDeviceMountPath --- read-only device, mounted in read-only mode.

  manager->RemountAllRemovableDrives(chromeos::MOUNT_ACCESS_MODE_READ_ONLY);

  // Simulate cros_disks reporting mount completed.
  fake_cros_disks_client_->SendMountCompletedEvent(
      chromeos::MOUNT_ERROR_NONE, kDevice1SourcePath,
      chromeos::MOUNT_TYPE_DEVICE, kDevice1MountPath);

  // Should remount disks that are not read-only by its hardware device.
  ASSERT_EQ(1U, observer_->GetEventCount());
  VerifyMountEvent(observer_->GetMountEvent(0), DiskMountManager::MOUNTING,
                   chromeos::MOUNT_ERROR_NONE, kDevice1MountPath);
  // The disk is remounted in read-only mode.
  EXPECT_TRUE(
      manager->FindDiskBySourcePath(kDevice1SourcePath)->is_read_only());
  // Remounted disk should also appear as read-only to observers.
  EXPECT_TRUE(observer_->GetMountEvent(0).disk->is_read_only());

  // Remount in read-write mode again.
  manager->RemountAllRemovableDrives(chromeos::MOUNT_ACCESS_MODE_READ_WRITE);

  // Simulate cros_disks reporting mount completed.
  fake_cros_disks_client_->SendMountCompletedEvent(
      chromeos::MOUNT_ERROR_NONE, kDevice1SourcePath,
      chromeos::MOUNT_TYPE_DEVICE, kDevice1MountPath);
  // Event handlers of observers should be called.
  ASSERT_EQ(2U, observer_->GetEventCount());
  VerifyMountEvent(observer_->GetMountEvent(1), DiskMountManager::MOUNTING,
                   chromeos::MOUNT_ERROR_NONE, kDevice1MountPath);
  // The read-write device should be remounted in read-write mode.
  EXPECT_FALSE(
      manager->FindDiskBySourcePath(kDevice1SourcePath)->is_read_only());
  // Remounted disk should also appear as writable to observers.
  EXPECT_FALSE(observer_->GetMountEvent(1).disk->is_read_only());
}

}  // namespace
