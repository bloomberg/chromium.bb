// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <dbt.h>

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/mock_devices_changed_observer.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/browser/system_monitor/portable_device_watcher_win.h"
#include "chrome/browser/system_monitor/removable_device_constants.h"
#include "chrome/browser/system_monitor/test_portable_device_watcher_win.h"
#include "chrome/browser/system_monitor/test_removable_device_notifications_window_win.h"
#include "chrome/browser/system_monitor/test_volume_mount_watcher_win.h"
#include "chrome/browser/system_monitor/volume_mount_watcher_win.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {
namespace test {

using content::BrowserThread;

typedef std::vector<int> DeviceIndices;

// RemovableDeviceNotificationsWindowWinTest -----------------------------------

class RemovableDeviceNotificationsWindowWinTest : public testing::Test {
 public:
  RemovableDeviceNotificationsWindowWinTest();
  virtual ~RemovableDeviceNotificationsWindowWinTest();

 protected:
  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void AddMassStorageDeviceAttachExpectation(const FilePath& drive);
  void PreAttachDevices();

  // Runs all the pending tasks on UI thread, FILE thread and blocking thread.
  void RunUntilIdle();

  void DoMassStorageDeviceAttachedTest(const DeviceIndices& device_indices);
  void DoMassStorageDevicesDetachedTest(const DeviceIndices& device_indices);

  // Injects a device attach or detach change (depending on the value of
  // |test_attach|) and tests that the appropriate handler is called.
  void DoMTPDeviceTest(const string16& pnp_device_id, bool test_attach);

  // Gets the MTP details of the storage specified by the |storage_device_id|.
  // On success, returns true and fills in |pnp_device_id| and
  // |storage_object_id|.
  bool GetMTPStorageInfo(const std::string& storage_device_id,
                         string16* pnp_device_id,
                         string16* storage_object_id);

  scoped_ptr<TestRemovableDeviceNotificationsWindowWin> window_;
  scoped_refptr<TestVolumeMountWatcherWin> volume_mount_watcher_;

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  base::SystemMonitor system_monitor_;
  base::MockDevicesChangedObserver observer_;
};

RemovableDeviceNotificationsWindowWinTest::
    RemovableDeviceNotificationsWindowWinTest()
   : ui_thread_(BrowserThread::UI, &message_loop_),
     file_thread_(BrowserThread::FILE, &message_loop_) {
}

RemovableDeviceNotificationsWindowWinTest::
    ~RemovableDeviceNotificationsWindowWinTest() {
}

void RemovableDeviceNotificationsWindowWinTest::SetUp() {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
  volume_mount_watcher_ = new TestVolumeMountWatcherWin;
  window_.reset(new TestRemovableDeviceNotificationsWindowWin(
      volume_mount_watcher_.get(), new TestPortableDeviceWatcherWin));
  window_->InitWithTestData(false);
  RunUntilIdle();
  system_monitor_.AddDevicesChangedObserver(&observer_);
}

void RemovableDeviceNotificationsWindowWinTest::TearDown() {
  RunUntilIdle();
  system_monitor_.RemoveDevicesChangedObserver(&observer_);
}

void RemovableDeviceNotificationsWindowWinTest::
    AddMassStorageDeviceAttachExpectation(const FilePath& drive) {
  std::string unique_id;
  string16 device_name;
  bool removable;
  ASSERT_TRUE(volume_mount_watcher_->GetDeviceInfo(
      drive, NULL, &unique_id, &device_name, &removable));
  if (removable) {
    MediaStorageUtil::Type type =
        MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
    std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);
    EXPECT_CALL(observer_, OnRemovableStorageAttached(device_id, device_name,
                                                      drive.value()))
        .Times(1);
  }
}

void RemovableDeviceNotificationsWindowWinTest::PreAttachDevices() {
  window_.reset();
  {
    testing::InSequence sequence;
    ASSERT_TRUE(volume_mount_watcher_.get());
    volume_mount_watcher_->set_pre_attach_devices(true);
    std::vector<FilePath> initial_devices =
        volume_mount_watcher_->GetAttachedDevices();
    for (std::vector<FilePath>::const_iterator it = initial_devices.begin();
         it != initial_devices.end(); ++it) {
      AddMassStorageDeviceAttachExpectation(*it);
    }
  }
  window_.reset(new TestRemovableDeviceNotificationsWindowWin(
      volume_mount_watcher_.get(), new TestPortableDeviceWatcherWin));
  window_->InitWithTestData(true);
  RunUntilIdle();
}

void RemovableDeviceNotificationsWindowWinTest::RunUntilIdle() {
  message_loop_.RunUntilIdle();
}

void RemovableDeviceNotificationsWindowWinTest::
    DoMassStorageDeviceAttachedTest(const DeviceIndices& device_indices) {
  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_unitmask = 0x0;
  volume_broadcast.dbcv_flags = 0x0;
  {
    testing::InSequence sequence;
    for (DeviceIndices::const_iterator it = device_indices.begin();
         it != device_indices.end(); ++it) {
      volume_broadcast.dbcv_unitmask |= 0x1 << *it;
      AddMassStorageDeviceAttachExpectation(
          VolumeMountWatcherWin::DriveNumberToFilePath(*it));
    }
  }
  window_->InjectDeviceChange(DBT_DEVICEARRIVAL,
                              reinterpret_cast<DWORD>(&volume_broadcast));
  RunUntilIdle();
}

void RemovableDeviceNotificationsWindowWinTest::
    DoMassStorageDevicesDetachedTest(const DeviceIndices& device_indices) {
  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_unitmask = 0x0;
  volume_broadcast.dbcv_flags = 0x0;
  {
    testing::InSequence sequence;
    for (DeviceIndices::const_iterator it = device_indices.begin();
         it != device_indices.end(); ++it) {
      volume_broadcast.dbcv_unitmask |= 0x1 << *it;
      std::string unique_id;
      bool removable;
      ASSERT_TRUE(volume_mount_watcher_->GetDeviceInfo(
          VolumeMountWatcherWin::DriveNumberToFilePath(*it), NULL, &unique_id,
          NULL, &removable));
      if (removable) {
        MediaStorageUtil::Type type =
            MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
        std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);
        EXPECT_CALL(observer_, OnRemovableStorageDetached(device_id)).Times(1);
      }
    }
  }
  window_->InjectDeviceChange(DBT_DEVICEREMOVECOMPLETE,
                              reinterpret_cast<DWORD>(&volume_broadcast));
  RunUntilIdle();
}

void RemovableDeviceNotificationsWindowWinTest::DoMTPDeviceTest(
    const string16& pnp_device_id, bool test_attach) {
  GUID guidDevInterface = GUID_NULL;
  HRESULT hr = CLSIDFromString(kWPDDevInterfaceGUID, &guidDevInterface);
  if (FAILED(hr))
    return;

  size_t device_id_size = pnp_device_id.size() * sizeof(char16);
  size_t size = sizeof(DEV_BROADCAST_DEVICEINTERFACE) + device_id_size;
  scoped_ptr_malloc<DEV_BROADCAST_DEVICEINTERFACE> dev_interface_broadcast(
      static_cast<DEV_BROADCAST_DEVICEINTERFACE*>(malloc(size)));
  DCHECK(dev_interface_broadcast.get());
  ZeroMemory(dev_interface_broadcast.get(), size);
  dev_interface_broadcast->dbcc_size = size;
  dev_interface_broadcast->dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
  dev_interface_broadcast->dbcc_classguid = guidDevInterface;
  memcpy(dev_interface_broadcast->dbcc_name, pnp_device_id.data(),
         device_id_size);
  {
    testing::InSequence sequence;
    PortableDeviceWatcherWin::StorageObjectIDs storage_object_ids =
        TestPortableDeviceWatcherWin::GetMTPStorageObjectIds(pnp_device_id);
    for (PortableDeviceWatcherWin::StorageObjectIDs::const_iterator it =
         storage_object_ids.begin(); it != storage_object_ids.end(); ++it) {
      std::string unique_id;
      string16 name;
      string16 location;
      TestPortableDeviceWatcherWin::GetMTPStorageDetails(pnp_device_id, *it,
                                                         &location, &unique_id,
                                                         &name);
      if (test_attach) {
        EXPECT_CALL(observer_, OnRemovableStorageAttached(unique_id, name,
                                                          location))
            .Times((name.empty() || unique_id.empty()) ? 0 : 1);
      } else {
        EXPECT_CALL(observer_, OnRemovableStorageDetached(unique_id))
            .Times((name.empty() || unique_id.empty()) ? 0 : 1);
      }
    }
  }
  window_->InjectDeviceChange(
      test_attach ? DBT_DEVICEARRIVAL : DBT_DEVICEREMOVECOMPLETE,
      reinterpret_cast<DWORD>(dev_interface_broadcast.get()));
  RunUntilIdle();
}

bool RemovableDeviceNotificationsWindowWinTest::GetMTPStorageInfo(
    const std::string& storage_device_id,
    string16* pnp_device_id,
    string16* storage_object_id) {
  return window_->GetMTPStorageInfoFromDeviceId(storage_device_id,
                                                pnp_device_id,
                                                storage_object_id);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, RandomMessage) {
  window_->InjectDeviceChange(DBT_DEVICEQUERYREMOVE, NULL);
  RunUntilIdle();
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesAttached) {
  DeviceIndices device_indices;
  device_indices.push_back(1);
  device_indices.push_back(5);
  device_indices.push_back(7);
  device_indices.push_back(13);

  DoMassStorageDeviceAttachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesAttachedHighBoundary) {
  DeviceIndices device_indices;
  device_indices.push_back(25);

  DoMassStorageDeviceAttachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesAttachedLowBoundary) {
  DeviceIndices device_indices;
  device_indices.push_back(0);

  DoMassStorageDeviceAttachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesAttachedAdjacentBits) {
  DeviceIndices device_indices;
  device_indices.push_back(0);
  device_indices.push_back(1);
  device_indices.push_back(2);
  device_indices.push_back(3);

  DoMassStorageDeviceAttachedTest(device_indices);
}

// Disabled until http://crbug.com/155910 is resolved.
TEST_F(RemovableDeviceNotificationsWindowWinTest, DISABLED_DevicesDetached) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(1);
  device_indices.push_back(5);
  device_indices.push_back(7);
  device_indices.push_back(13);

  DoMassStorageDeviceAttachedTest(device_indices);
}

// Disabled until http://crbug.com/155910 is resolved.
TEST_F(RemovableDeviceNotificationsWindowWinTest,
       DISABLED_DevicesDetachedHighBoundary) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(25);

  DoMassStorageDeviceAttachedTest(device_indices);
}

// Disabled until http://crbug.com/155910 is resolved.
TEST_F(RemovableDeviceNotificationsWindowWinTest,
       DISABLED_DevicesDetachedLowBoundary) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(0);

  DoMassStorageDeviceAttachedTest(device_indices);
}

// Disabled until http://crbug.com/155910 is resolved.
TEST_F(RemovableDeviceNotificationsWindowWinTest,
       DISABLED_DevicesDetachedAdjacentBits) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(0);
  device_indices.push_back(1);
  device_indices.push_back(2);
  device_indices.push_back(3);

  DoMassStorageDeviceAttachedTest(device_indices);
}

// Disabled until http://crbug.com/155910 is resolved.
TEST_F(RemovableDeviceNotificationsWindowWinTest, DISABLED_DeviceInfoForPath) {
  PreAttachDevices();

  // An invalid path.
  EXPECT_FALSE(window_->GetDeviceInfoForPath(FilePath(L"COM1:\\"), NULL));

  // An unconnected removable device.
  EXPECT_FALSE(window_->GetDeviceInfoForPath(FilePath(L"E:\\"), NULL));

  // A connected removable device.
  FilePath removable_device(L"F:\\");
  base::SystemMonitor::RemovableStorageInfo device_info;
  EXPECT_TRUE(window_->GetDeviceInfoForPath(removable_device, &device_info));

  std::string unique_id;
  string16 device_name;
  bool removable;
  ASSERT_TRUE(volume_mount_watcher_->GetDeviceInfo(
      removable_device, NULL, &unique_id, &device_name, &removable));
  EXPECT_TRUE(removable);
  std::string device_id = MediaStorageUtil::MakeDeviceId(
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM, unique_id);
  EXPECT_EQ(device_id, device_info.device_id);
  EXPECT_EQ(device_name, device_info.name);
  EXPECT_EQ(removable_device.value(), device_info.location);

  // A fixed device.
  FilePath fixed_device(L"N:\\");
  EXPECT_TRUE(window_->GetDeviceInfoForPath(fixed_device, &device_info));

  ASSERT_TRUE(volume_mount_watcher_->GetDeviceInfo(
      fixed_device, NULL, &unique_id, &device_name, &removable));
  EXPECT_FALSE(removable);
  device_id = MediaStorageUtil::MakeDeviceId(
      MediaStorageUtil::FIXED_MASS_STORAGE, unique_id);
  EXPECT_EQ(device_id, device_info.device_id);
  EXPECT_EQ(device_name, device_info.name);
  EXPECT_EQ(fixed_device.value(), device_info.location);
}

// Test to verify basic MTP storage attach and detach notifications.
TEST_F(RemovableDeviceNotificationsWindowWinTest, MTPDeviceBasicAttachDetach) {
  DoMTPDeviceTest(TestPortableDeviceWatcherWin::kMTPDeviceWithValidInfo, true);
  DoMTPDeviceTest(TestPortableDeviceWatcherWin::kMTPDeviceWithValidInfo, false);
}

// When a MTP storage device with invalid storage label and id is
// attached/detached, there should not be any device attach/detach
// notifications.
TEST_F(RemovableDeviceNotificationsWindowWinTest, MTPDeviceWithInvalidInfo) {
  DoMTPDeviceTest(TestPortableDeviceWatcherWin::kMTPDeviceWithInvalidInfo,
                  true);
  DoMTPDeviceTest(TestPortableDeviceWatcherWin::kMTPDeviceWithInvalidInfo,
                  false);
}

// Attach a device with two data partitions. Verify that attach/detach
// notifications are sent out for each removable storage.
TEST_F(RemovableDeviceNotificationsWindowWinTest,
       MTPDeviceWithMultipleStorageObjects) {
  DoMTPDeviceTest(TestPortableDeviceWatcherWin::kMTPDeviceWithMultipleStorages,
                  true);
  DoMTPDeviceTest(TestPortableDeviceWatcherWin::kMTPDeviceWithMultipleStorages,
                  false);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DriveNumberToFilePath) {
  EXPECT_EQ(L"A:\\", VolumeMountWatcherWin::DriveNumberToFilePath(0).value());
  EXPECT_EQ(L"Y:\\", VolumeMountWatcherWin::DriveNumberToFilePath(24).value());
  EXPECT_EQ(L"", VolumeMountWatcherWin::DriveNumberToFilePath(-1).value());
  EXPECT_EQ(L"", VolumeMountWatcherWin::DriveNumberToFilePath(199).value());
}

// Given a MTP storage persistent id, GetMTPStorageInfo() should fetch the
// device interface path and local storage object identifier.
TEST_F(RemovableDeviceNotificationsWindowWinTest,
       GetMTPStorageInfoFromDeviceId) {
  DoMTPDeviceTest(TestPortableDeviceWatcherWin::kMTPDeviceWithValidInfo, true);
  PortableDeviceWatcherWin::StorageObjects storage_objects =
      TestPortableDeviceWatcherWin::GetDeviceStorageObjects(
          TestPortableDeviceWatcherWin::kMTPDeviceWithValidInfo);
  for (PortableDeviceWatcherWin::StorageObjects::const_iterator it =
           storage_objects.begin();
       it != storage_objects.end(); ++it) {
    string16 pnp_device_id;
    string16 storage_object_id;
    ASSERT_TRUE(GetMTPStorageInfo(it->object_persistent_id, &pnp_device_id,
                                  &storage_object_id));
    EXPECT_EQ(string16(TestPortableDeviceWatcherWin::kMTPDeviceWithValidInfo),
              pnp_device_id);
    EXPECT_EQ(it->object_persistent_id,
              TestPortableDeviceWatcherWin::GetMTPStorageUniqueId(
                  pnp_device_id, storage_object_id));
  }
  DoMTPDeviceTest(TestPortableDeviceWatcherWin::kMTPDeviceWithValidInfo, false);
}

}  // namespace test
}  // namespace chrome
