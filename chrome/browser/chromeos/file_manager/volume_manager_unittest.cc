// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/volume_manager.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/storage_monitor/storage_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace {

class LoggingObserver : public VolumeManagerObserver {
 public:
  struct Event {
    enum EventType {
      DISK_ADDED,
      DISK_REMOVED,
      DEVICE_ADDED,
      DEVICE_REMOVED,
      VOLUME_MOUNTED,
      VOLUME_UNMOUNTED,
      FORMAT_STARTED,
      FORMAT_COMPLETED,
    } type;

    // Available on DEVICE_ADDED, DEVICE_REMOVED, VOLUME_MOUNTED,
    // VOLUME_UNMOUNTED, FORMAT_STARTED and FORMAT_COMPLETED.
    std::string device_path;

    // Available on DISK_ADDED.
    bool mounting;

    // Available on VOLUME_MOUNTED and VOLUME_UNMOUNTED.
    chromeos::MountError mount_error;

    // Available on FORMAT_STARTED and FORMAT_COMPLETED.
    bool success;
  };

  LoggingObserver() {}
  virtual ~LoggingObserver() {}

  const std::vector<Event>& events() const { return events_; }

  // VolumeManagerObserver overrides.
  virtual void OnDiskAdded(const chromeos::disks::DiskMountManager::Disk& disk,
                           bool mounting) OVERRIDE {
    Event event;
    event.type = Event::DISK_ADDED;
    event.device_path = disk.device_path();  // Keep only device_path.
    event.mounting = mounting;
    events_.push_back(event);
  }

  virtual void OnDiskRemoved(
      const chromeos::disks::DiskMountManager::Disk& disk) OVERRIDE {
    Event event;
    event.type = Event::DISK_REMOVED;
    event.device_path = disk.device_path();  // Keep only device_path.
    events_.push_back(event);
  }

  virtual void OnDeviceAdded(const std::string& device_path) OVERRIDE {
    Event event;
    event.type = Event::DEVICE_ADDED;
    event.device_path = device_path;
    events_.push_back(event);
  }

  virtual void OnDeviceRemoved(const std::string& device_path) OVERRIDE {
    Event event;
    event.type = Event::DEVICE_REMOVED;
    event.device_path = device_path;
    events_.push_back(event);
  }

  virtual void OnVolumeMounted(chromeos::MountError error_code,
                               const VolumeInfo& volume_info) OVERRIDE {
    Event event;
    event.type = Event::VOLUME_MOUNTED;
    event.device_path = volume_info.source_path.AsUTF8Unsafe();
    event.mount_error = error_code;
    events_.push_back(event);
  }

  virtual void OnVolumeUnmounted(chromeos::MountError error_code,
                                 const VolumeInfo& volume_info) OVERRIDE {
    Event event;
    event.type = Event::VOLUME_UNMOUNTED;
    event.device_path = volume_info.source_path.AsUTF8Unsafe();
    event.mount_error = error_code;
    events_.push_back(event);
  }

  virtual void OnFormatStarted(
      const std::string& device_path, bool success) OVERRIDE {
    Event event;
    event.type = Event::FORMAT_STARTED;
    event.device_path = device_path;
    event.success = success;
    events_.push_back(event);
  }

  virtual void OnFormatCompleted(
      const std::string& device_path, bool success) OVERRIDE {
    Event event;
    event.type = Event::FORMAT_COMPLETED;
    event.device_path = device_path;
    event.success = success;
    events_.push_back(event);
  }

 private:
  std::vector<Event> events_;

  DISALLOW_COPY_AND_ASSIGN(LoggingObserver);
};

}  // namespace

class VolumeManagerTest : public testing::Test {
 protected:
  // Helper class that contains per-profile objects.
  class ProfileEnvironment {
   public:
    ProfileEnvironment(chromeos::PowerManagerClient* power_manager_client,
                       chromeos::disks::DiskMountManager* disk_manager)
        : profile_(new TestingProfile),
          extension_registry_(
              new extensions::ExtensionRegistry(profile_.get())),
          file_system_provider_service_(
              new chromeos::file_system_provider::Service(
                  profile_.get(),
                  extension_registry_.get())),
          volume_manager_(
              new VolumeManager(profile_.get(),
                                NULL,  // DriveIntegrationService
                                power_manager_client,
                                disk_manager,
                                file_system_provider_service_.get())) {
      file_system_provider_service_->SetFileSystemFactoryForTesting(base::Bind(
          &chromeos::file_system_provider::FakeProvidedFileSystem::Create));
    }

    Profile* profile() const { return profile_.get(); }
    VolumeManager* volume_manager() const { return volume_manager_.get(); }

   private:
    scoped_ptr<TestingProfile> profile_;
    scoped_ptr<extensions::ExtensionRegistry> extension_registry_;
    scoped_ptr<chromeos::file_system_provider::Service>
        file_system_provider_service_;
    scoped_ptr<VolumeManager> volume_manager_;
  };

  virtual void SetUp() OVERRIDE {
    power_manager_client_.reset(new chromeos::FakePowerManagerClient);
    disk_mount_manager_.reset(new FakeDiskMountManager);
    main_profile_.reset(new ProfileEnvironment(power_manager_client_.get(),
                                               disk_mount_manager_.get()));
  }

  Profile* profile() const { return main_profile_->profile(); }
  VolumeManager* volume_manager() const {
    return main_profile_->volume_manager();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<chromeos::FakePowerManagerClient> power_manager_client_;
  scoped_ptr<FakeDiskMountManager> disk_mount_manager_;
  scoped_ptr<ProfileEnvironment> main_profile_;
};

TEST_F(VolumeManagerTest, OnDriveFileSystemMountAndUnmount) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFileSystemMounted();

  ASSERT_EQ(1U, observer.events().size());
  LoggingObserver::Event event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, event.type);
  EXPECT_EQ(drive::util::GetDriveMountPointPath(profile()).AsUTF8Unsafe(),
            event.device_path);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, event.mount_error);

  volume_manager()->OnFileSystemBeingUnmounted();

  ASSERT_EQ(2U, observer.events().size());
  event = observer.events()[1];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED, event.type);
  EXPECT_EQ(drive::util::GetDriveMountPointPath(profile()).AsUTF8Unsafe(),
            event.device_path);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, event.mount_error);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDriveFileSystemUnmountWithoutMount) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);
  volume_manager()->OnFileSystemBeingUnmounted();

  // Unmount event for non-mounted volume is not reported.
  ASSERT_EQ(0U, observer.events().size());
  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDiskEvent_Hidden) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  const bool kIsHidden = true;
  const chromeos::disks::DiskMountManager::Disk kDisk(
      "device1", "", "", "", "", "", "", "", "", "", "", "",
      chromeos::DEVICE_TYPE_UNKNOWN, 0, false, false, false, false, false,
      kIsHidden);

  volume_manager()->OnDiskEvent(
      chromeos::disks::DiskMountManager::DISK_ADDED, &kDisk);
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->OnDiskEvent(
      chromeos::disks::DiskMountManager::DISK_REMOVED, &kDisk);
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->OnDiskEvent(
      chromeos::disks::DiskMountManager::DISK_CHANGED, &kDisk);
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDiskEvent_Added) {
  // Enable external storage.
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, false);

  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  const chromeos::disks::DiskMountManager::Disk kEmptyDevicePathDisk(
      "",  // empty device path.
      "", "", "", "", "", "", "", "", "", "", "",
      chromeos::DEVICE_TYPE_UNKNOWN, 0, false, false, false, false, false,
      false);
  volume_manager()->OnDiskEvent(
      chromeos::disks::DiskMountManager::DISK_ADDED, &kEmptyDevicePathDisk);
  EXPECT_EQ(0U, observer.events().size());

  const bool kHasMedia = true;
  const chromeos::disks::DiskMountManager::Disk kMediaDisk(
      "device1", "", "", "", "", "", "", "", "", "", "", "",
      chromeos::DEVICE_TYPE_UNKNOWN, 0, false, false, kHasMedia, false, false,
      false);
  volume_manager()->OnDiskEvent(
      chromeos::disks::DiskMountManager::DISK_ADDED, &kMediaDisk);
  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::DISK_ADDED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_TRUE(event.mounting);

  ASSERT_EQ(1U, disk_mount_manager_->mount_requests().size());
  const FakeDiskMountManager::MountRequest& mount_request =
      disk_mount_manager_->mount_requests()[0];
  EXPECT_EQ("device1", mount_request.source_path);
  EXPECT_EQ("", mount_request.source_format);
  EXPECT_EQ("", mount_request.mount_label);
  EXPECT_EQ(chromeos::MOUNT_TYPE_DEVICE, mount_request.type);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDiskEvent_AddedNonMounting) {
  // Enable external storage.
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, false);

  // Device which is already mounted.
  {
    LoggingObserver observer;
    volume_manager()->AddObserver(&observer);

    const bool kHasMedia = true;
    const chromeos::disks::DiskMountManager::Disk kMountedMediaDisk(
        "device1", "mounted", "", "", "", "", "", "", "", "", "", "",
        chromeos::DEVICE_TYPE_UNKNOWN, 0, false, false,
        kHasMedia, false, false, false);
    volume_manager()->OnDiskEvent(
        chromeos::disks::DiskMountManager::DISK_ADDED, &kMountedMediaDisk);
    ASSERT_EQ(1U, observer.events().size());
    const LoggingObserver::Event& event = observer.events()[0];
    EXPECT_EQ(LoggingObserver::Event::DISK_ADDED, event.type);
    EXPECT_EQ("device1", event.device_path);
    EXPECT_FALSE(event.mounting);

    ASSERT_EQ(0U, disk_mount_manager_->mount_requests().size());

    volume_manager()->RemoveObserver(&observer);
  }

  // Device without media.
  {
    LoggingObserver observer;
    volume_manager()->AddObserver(&observer);

    const bool kWithoutMedia = false;
    const chromeos::disks::DiskMountManager::Disk kNoMediaDisk(
        "device1", "", "", "", "", "", "", "", "", "", "", "",
        chromeos::DEVICE_TYPE_UNKNOWN, 0, false, false,
        kWithoutMedia, false, false, false);
    volume_manager()->OnDiskEvent(
        chromeos::disks::DiskMountManager::DISK_ADDED, &kNoMediaDisk);
    ASSERT_EQ(1U, observer.events().size());
    const LoggingObserver::Event& event = observer.events()[0];
    EXPECT_EQ(LoggingObserver::Event::DISK_ADDED, event.type);
    EXPECT_EQ("device1", event.device_path);
    EXPECT_FALSE(event.mounting);

    ASSERT_EQ(0U, disk_mount_manager_->mount_requests().size());

    volume_manager()->RemoveObserver(&observer);
  }

  // External storage is disabled.
  {
    profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, true);

    LoggingObserver observer;
    volume_manager()->AddObserver(&observer);

    const bool kHasMedia = true;
    const chromeos::disks::DiskMountManager::Disk kMediaDisk(
        "device1", "", "", "", "", "", "", "", "", "", "", "",
        chromeos::DEVICE_TYPE_UNKNOWN, 0, false, false,
        kHasMedia, false, false, false);
    volume_manager()->OnDiskEvent(
        chromeos::disks::DiskMountManager::DISK_ADDED, &kMediaDisk);
    ASSERT_EQ(1U, observer.events().size());
    const LoggingObserver::Event& event = observer.events()[0];
    EXPECT_EQ(LoggingObserver::Event::DISK_ADDED, event.type);
    EXPECT_EQ("device1", event.device_path);
    EXPECT_FALSE(event.mounting);

    ASSERT_EQ(0U, disk_mount_manager_->mount_requests().size());

    volume_manager()->RemoveObserver(&observer);
  }
}

TEST_F(VolumeManagerTest, OnDiskEvent_Removed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  const chromeos::disks::DiskMountManager::Disk kMountedDisk(
      "device1", "mount_path", "", "", "", "", "", "", "", "", "", "",
      chromeos::DEVICE_TYPE_UNKNOWN, 0, false, false, false, false, false,
      false);
  volume_manager()->OnDiskEvent(
      chromeos::disks::DiskMountManager::DISK_REMOVED, &kMountedDisk);

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::DISK_REMOVED, event.type);
  EXPECT_EQ("device1", event.device_path);

  ASSERT_EQ(1U, disk_mount_manager_->unmount_requests().size());
  const FakeDiskMountManager::UnmountRequest& unmount_request =
      disk_mount_manager_->unmount_requests()[0];
  EXPECT_EQ("mount_path", unmount_request.mount_path);
  EXPECT_EQ(chromeos::UNMOUNT_OPTIONS_LAZY, unmount_request.options);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDiskEvent_RemovedNotMounted) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  const chromeos::disks::DiskMountManager::Disk kNotMountedDisk(
      "device1", "", "", "", "", "", "", "", "", "", "", "",
      chromeos::DEVICE_TYPE_UNKNOWN, 0, false, false, false, false, false,
      false);
  volume_manager()->OnDiskEvent(
      chromeos::disks::DiskMountManager::DISK_REMOVED, &kNotMountedDisk);

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::DISK_REMOVED, event.type);
  EXPECT_EQ("device1", event.device_path);

  ASSERT_EQ(0U, disk_mount_manager_->unmount_requests().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDiskEvent_Changed) {
  // Changed event should cause mounting (if possible).
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  const chromeos::disks::DiskMountManager::Disk kDisk(
      "device1", "", "", "", "", "", "", "", "", "", "", "",
      chromeos::DEVICE_TYPE_UNKNOWN, 0, false, false, true, false, false,
      false);
  volume_manager()->OnDiskEvent(
      chromeos::disks::DiskMountManager::DISK_CHANGED, &kDisk);

  EXPECT_EQ(1U, observer.events().size());
  EXPECT_EQ(1U, disk_mount_manager_->mount_requests().size());
  EXPECT_EQ(0U, disk_mount_manager_->unmount_requests().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDeviceEvent_Added) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnDeviceEvent(
      chromeos::disks::DiskMountManager::DEVICE_ADDED, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::DEVICE_ADDED, event.type);
  EXPECT_EQ("device1", event.device_path);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDeviceEvent_Removed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnDeviceEvent(
      chromeos::disks::DiskMountManager::DEVICE_REMOVED, "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::DEVICE_REMOVED, event.type);
  EXPECT_EQ("device1", event.device_path);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnDeviceEvent_Scanned) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnDeviceEvent(
      chromeos::disks::DiskMountManager::DEVICE_SCANNED, "device1");

  // SCANNED event is just ignored.
  EXPECT_EQ(0U, observer.events().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnMountEvent_MountingAndUnmounting) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  const chromeos::disks::DiskMountManager::MountPointInfo kMountPoint(
      "device1",
      "mount1",
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);

  volume_manager()->OnMountEvent(chromeos::disks::DiskMountManager::MOUNTING,
                                chromeos::MOUNT_ERROR_NONE,
                                kMountPoint);

  ASSERT_EQ(1U, observer.events().size());
  LoggingObserver::Event event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, event.mount_error);

  volume_manager()->OnMountEvent(chromeos::disks::DiskMountManager::UNMOUNTING,
                                chromeos::MOUNT_ERROR_NONE,
                                kMountPoint);

  ASSERT_EQ(2U, observer.events().size());
  event = observer.events()[1];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, event.mount_error);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnMountEvent_Remounting) {
  scoped_ptr<chromeos::disks::DiskMountManager::Disk> disk(
      new chromeos::disks::DiskMountManager::Disk(
          "device1", "", "", "", "", "", "", "", "", "", "uuid1", "",
          chromeos::DEVICE_TYPE_UNKNOWN, 0,
          false, false, false, false, false, false));
  disk_mount_manager_->AddDiskForTest(disk.release());
  disk_mount_manager_->MountPath(
      "device1", "", "", chromeos::MOUNT_TYPE_DEVICE);

  const chromeos::disks::DiskMountManager::MountPointInfo kMountPoint(
      "device1",
      "mount1",
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);

  volume_manager()->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      kMountPoint);

  LoggingObserver observer;

  // Emulate system suspend and then resume.
  {
    power_manager_client_->SendSuspendImminent();
    power_manager_client_->SendSuspendDone();

    // After resume, the device is unmounted and then mounted.
    volume_manager()->OnMountEvent(
        chromeos::disks::DiskMountManager::UNMOUNTING,
        chromeos::MOUNT_ERROR_NONE,
        kMountPoint);

    // Observe what happened for the mount event.
    volume_manager()->AddObserver(&observer);

    volume_manager()->OnMountEvent(
        chromeos::disks::DiskMountManager::MOUNTING,
        chromeos::MOUNT_ERROR_NONE,
        kMountPoint);
  }

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_EQ(chromeos::MOUNT_ERROR_NONE, event.mount_error);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnMountEvent_UnmountingWithoutMounting) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  const chromeos::disks::DiskMountManager::MountPointInfo kMountPoint(
      "device1",
      "mount1",
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);

  volume_manager()->OnMountEvent(chromeos::disks::DiskMountManager::UNMOUNTING,
                                chromeos::MOUNT_ERROR_NONE,
                                kMountPoint);

  // Unmount event for a disk not mounted in this manager is not reported.
  ASSERT_EQ(0U, observer.events().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_Started) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(
      chromeos::disks::DiskMountManager::FORMAT_STARTED,
      chromeos::FORMAT_ERROR_NONE,
      "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_TRUE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_StartFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(
      chromeos::disks::DiskMountManager::FORMAT_STARTED,
      chromeos::FORMAT_ERROR_UNKNOWN,
      "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_STARTED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_FALSE(event.success);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_Completed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(
      chromeos::disks::DiskMountManager::FORMAT_COMPLETED,
      chromeos::FORMAT_ERROR_NONE,
      "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_TRUE(event.success);

  // When "format" is successfully done, VolumeManager requests to mount it.
  ASSERT_EQ(1U, disk_mount_manager_->mount_requests().size());
  const FakeDiskMountManager::MountRequest& mount_request =
      disk_mount_manager_->mount_requests()[0];
  EXPECT_EQ("device1", mount_request.source_path);
  EXPECT_EQ("", mount_request.source_format);
  EXPECT_EQ("", mount_request.mount_label);
  EXPECT_EQ(chromeos::MOUNT_TYPE_DEVICE, mount_request.type);

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnFormatEvent_CompletedFailed) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  volume_manager()->OnFormatEvent(
      chromeos::disks::DiskMountManager::FORMAT_COMPLETED,
      chromeos::FORMAT_ERROR_UNKNOWN,
      "device1");

  ASSERT_EQ(1U, observer.events().size());
  const LoggingObserver::Event& event = observer.events()[0];
  EXPECT_EQ(LoggingObserver::Event::FORMAT_COMPLETED, event.type);
  EXPECT_EQ("device1", event.device_path);
  EXPECT_FALSE(event.success);

  EXPECT_EQ(0U, disk_mount_manager_->mount_requests().size());

  volume_manager()->RemoveObserver(&observer);
}

TEST_F(VolumeManagerTest, OnExternalStorageDisabledChanged) {
  // Here create two mount points.
  disk_mount_manager_->MountPath(
      "mount1", "", "", chromeos::MOUNT_TYPE_DEVICE);
  disk_mount_manager_->MountPath(
      "mount2", "", "", chromeos::MOUNT_TYPE_DEVICE);

  // Initially, there are two mount points.
  ASSERT_EQ(2U, disk_mount_manager_->mount_points().size());
  ASSERT_EQ(0U, disk_mount_manager_->unmount_requests().size());

  // Emulate to set kExternalStorageDisabled to false.
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, false);
  volume_manager()->OnExternalStorageDisabledChanged();

  // Expect no effects.
  EXPECT_EQ(2U, disk_mount_manager_->mount_points().size());
  EXPECT_EQ(0U, disk_mount_manager_->unmount_requests().size());

  // Emulate to set kExternalStorageDisabled to true.
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, true);
  volume_manager()->OnExternalStorageDisabledChanged();

  // The all mount points should be unmounted.
  EXPECT_EQ(0U, disk_mount_manager_->mount_points().size());

  EXPECT_EQ(2U, disk_mount_manager_->unmount_requests().size());
  const FakeDiskMountManager::UnmountRequest& unmount_request1 =
      disk_mount_manager_->unmount_requests()[0];
  EXPECT_EQ("mount1", unmount_request1.mount_path);

  const FakeDiskMountManager::UnmountRequest& unmount_request2 =
      disk_mount_manager_->unmount_requests()[1];
  EXPECT_EQ("mount2", unmount_request2.mount_path);
}

TEST_F(VolumeManagerTest, ExternalStorageDisabledPolicyMultiProfile) {
  ProfileEnvironment secondary(power_manager_client_.get(),
                               disk_mount_manager_.get());
  volume_manager()->Initialize();
  secondary.volume_manager()->Initialize();

  // Simulates the case that the main profile has kExternalStorageDisabled set
  // as false, and the secondary profile has the config set to true.
  profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled, false);
  secondary.profile()->GetPrefs()->SetBoolean(prefs::kExternalStorageDisabled,
                                              true);

  LoggingObserver main_observer, secondary_observer;
  volume_manager()->AddObserver(&main_observer);
  secondary.volume_manager()->AddObserver(&secondary_observer);

  // Add 1 disk.
  const chromeos::disks::DiskMountManager::Disk kMediaDisk(
      "device1", "", "", "", "", "", "", "", "", "", "", "",
      chromeos::DEVICE_TYPE_UNKNOWN, 0, false, false, true, false, false,
      false);
  volume_manager()->OnDiskEvent(
      chromeos::disks::DiskMountManager::DISK_ADDED, &kMediaDisk);
  secondary.volume_manager()->OnDiskEvent(
      chromeos::disks::DiskMountManager::DISK_ADDED, &kMediaDisk);

  // The profile with external storage enabled should have mounted the volume.
  bool has_volume_mounted = false;
  for (size_t i = 0; i < main_observer.events().size(); ++i) {
    if (main_observer.events()[i].type ==
        LoggingObserver::Event::VOLUME_MOUNTED)
      has_volume_mounted = true;
  }
  EXPECT_TRUE(has_volume_mounted);

  // The other profiles with external storage disabled should have not.
  has_volume_mounted = false;
  for (size_t i = 0; i < secondary_observer.events().size(); ++i) {
    if (secondary_observer.events()[i].type ==
        LoggingObserver::Event::VOLUME_MOUNTED)
      has_volume_mounted = true;
  }
  EXPECT_FALSE(has_volume_mounted);

  volume_manager()->RemoveObserver(&main_observer);
  secondary.volume_manager()->RemoveObserver(&secondary_observer);
}

TEST_F(VolumeManagerTest, GetVolumeInfoList) {
  volume_manager()->Initialize();  // Adds "Downloads"
  std::vector<VolumeInfo> info_list = volume_manager()->GetVolumeInfoList();
  ASSERT_EQ(1u, info_list.size());
  EXPECT_EQ("downloads:Downloads", info_list[0].volume_id);
  EXPECT_EQ(VOLUME_TYPE_DOWNLOADS_DIRECTORY, info_list[0].type);
}

TEST_F(VolumeManagerTest, FindVolumeInfoById) {
  volume_manager()->Initialize();  // Adds "Downloads"
  VolumeInfo volume_info;
  ASSERT_FALSE(volume_manager()->FindVolumeInfoById(
      "nonexistent", &volume_info));
  ASSERT_TRUE(volume_manager()->FindVolumeInfoById(
      "downloads:Downloads", &volume_info));
  EXPECT_EQ("downloads:Downloads", volume_info.volume_id);
  EXPECT_EQ(VOLUME_TYPE_DOWNLOADS_DIRECTORY, volume_info.type);
}

TEST_F(VolumeManagerTest, ArchiveSourceFiltering) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  // Mount a USB stick.
  volume_manager()->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      chromeos::disks::DiskMountManager::MountPointInfo(
          "/removable/usb",
          "/removable/usb",
          chromeos::MOUNT_TYPE_DEVICE,
          chromeos::disks::MOUNT_CONDITION_NONE));

  // Mount a zip archive in the stick.
  volume_manager()->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      chromeos::disks::DiskMountManager::MountPointInfo(
          "/removable/usb/1.zip",
          "/archive/1",
          chromeos::MOUNT_TYPE_ARCHIVE,
          chromeos::disks::MOUNT_CONDITION_NONE));
  VolumeInfo volume_info;
  ASSERT_TRUE(volume_manager()->FindVolumeInfoById("archive:1", &volume_info));
  EXPECT_EQ("/archive/1", volume_info.mount_path.AsUTF8Unsafe());
  EXPECT_EQ(2u, observer.events().size());

  // Mount a zip archive in the previous zip archive.
  volume_manager()->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      chromeos::disks::DiskMountManager::MountPointInfo(
          "/archive/1/2.zip",
          "/archive/2",
          chromeos::MOUNT_TYPE_ARCHIVE,
          chromeos::disks::MOUNT_CONDITION_NONE));
  ASSERT_TRUE(volume_manager()->FindVolumeInfoById("archive:2", &volume_info));
  EXPECT_EQ("/archive/2", volume_info.mount_path.AsUTF8Unsafe());
  EXPECT_EQ(3u, observer.events().size());

  // A zip file is mounted from other profile. It must be ignored in the current
  // VolumeManager.
  volume_manager()->OnMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_NONE,
      chromeos::disks::DiskMountManager::MountPointInfo(
          "/other/profile/drive/folder/3.zip",
          "/archive/3",
          chromeos::MOUNT_TYPE_ARCHIVE,
          chromeos::disks::MOUNT_CONDITION_NONE));
  EXPECT_FALSE(volume_manager()->FindVolumeInfoById("archive:3", &volume_info));
  EXPECT_EQ(3u, observer.events().size());
}

TEST_F(VolumeManagerTest, MTPPlugAndUnplug) {
  LoggingObserver observer;
  volume_manager()->AddObserver(&observer);

  storage_monitor::StorageInfo info(
      storage_monitor::StorageInfo::MakeDeviceId(
          storage_monitor::StorageInfo::MTP_OR_PTP, "dummy-device-id"),
      FILE_PATH_LITERAL("/dummy/device/location"),
      base::UTF8ToUTF16("label"),
      base::UTF8ToUTF16("vendor"),
      base::UTF8ToUTF16("model"),
      12345 /* size */);

  storage_monitor::StorageInfo non_mtp_info(
      storage_monitor::StorageInfo::MakeDeviceId(
          storage_monitor::StorageInfo::IPHOTO, "dummy-device-id2"),
      FILE_PATH_LITERAL("/dummy/device/location2"),
      base::UTF8ToUTF16("label2"),
      base::UTF8ToUTF16("vendor2"),
      base::UTF8ToUTF16("model2"),
      12345 /* size */);

  // Attach
  volume_manager()->OnRemovableStorageAttached(info);
  ASSERT_EQ(1u, observer.events().size());
  EXPECT_EQ(LoggingObserver::Event::VOLUME_MOUNTED, observer.events()[0].type);

  VolumeInfo volume_info;
  ASSERT_TRUE(volume_manager()->FindVolumeInfoById("mtp:model", &volume_info));
  EXPECT_EQ(VOLUME_TYPE_MTP, volume_info.type);

  // Non MTP events from storage monitor are ignored.
  volume_manager()->OnRemovableStorageAttached(non_mtp_info);
  EXPECT_EQ(1u, observer.events().size());

  // Detach
  volume_manager()->OnRemovableStorageDetached(info);
  ASSERT_EQ(2u, observer.events().size());
  EXPECT_EQ(LoggingObserver::Event::VOLUME_UNMOUNTED,
            observer.events()[1].type);

  EXPECT_FALSE(volume_manager()->FindVolumeInfoById("mtp:model", &volume_info));
}

}  // namespace file_manager
