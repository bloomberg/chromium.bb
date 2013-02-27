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
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/mock_removable_storage_observer.h"
#include "chrome/browser/storage_monitor/portable_device_watcher_win.h"
#include "chrome/browser/storage_monitor/removable_device_constants.h"
#include "chrome/browser/storage_monitor/removable_device_notifications_window_win.h"
#include "chrome/browser/storage_monitor/test_portable_device_watcher_win.h"
#include "chrome/browser/storage_monitor/test_removable_device_notifications_window_win.h"
#include "chrome/browser/storage_monitor/test_volume_mount_watcher_win.h"
#include "chrome/browser/storage_monitor/volume_mount_watcher_win.h"
#include "content/public/test/test_browser_thread.h"
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

  // Weak pointer; owned by the device notifications class.
  TestVolumeMountWatcherWin* volume_mount_watcher_;

  MockRemovableStorageObserver observer_;

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
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
      volume_mount_watcher_, new TestPortableDeviceWatcherWin));
  window_->Init();
  RunUntilIdle();
  window_->AddObserver(&observer_);
}

void RemovableDeviceNotificationsWindowWinTest::TearDown() {
  RunUntilIdle();
  window_->RemoveObserver(&observer_);
}

void RemovableDeviceNotificationsWindowWinTest::PreAttachDevices() {
  window_.reset();
  volume_mount_watcher_ = new TestVolumeMountWatcherWin;
  volume_mount_watcher_->SetAttachedDevicesFake();

  int expect_attach_calls = 0;
  std::vector<base::FilePath> initial_devices =
      volume_mount_watcher_->GetAttachedDevices();
  for (std::vector<base::FilePath>::const_iterator it = initial_devices.begin();
       it != initial_devices.end(); ++it) {
    std::string unique_id;
    string16 device_name;
    bool removable;
    ASSERT_TRUE(volume_mount_watcher_->GetRawDeviceInfo(
        *it, NULL, &unique_id, &device_name, &removable, NULL));
    if (removable)
      expect_attach_calls++;
  }

  window_.reset(new TestRemovableDeviceNotificationsWindowWin(
      volume_mount_watcher_, new TestPortableDeviceWatcherWin));
  window_->AddObserver(&observer_);
  window_->Init();

  EXPECT_EQ(0u, volume_mount_watcher_->devices_checked().size());

  // This dance is because attachment bounces through a couple of
  // closures, which need to be executed to finish the process.
  RunUntilIdle();
  volume_mount_watcher_->FlushWorkerPoolForTesting();
  RunUntilIdle();

  std::vector<base::FilePath> checked_devices =
      volume_mount_watcher_->devices_checked();
  sort(checked_devices.begin(), checked_devices.end());
  EXPECT_EQ(initial_devices, checked_devices);
  EXPECT_EQ(expect_attach_calls, observer_.attach_calls());
  EXPECT_EQ(0, observer_.detach_calls());
}

void RemovableDeviceNotificationsWindowWinTest::RunUntilIdle() {
  volume_mount_watcher_->FlushWorkerPoolForTesting();
  message_loop_.RunUntilIdle();
}

void RemovableDeviceNotificationsWindowWinTest::
    DoMassStorageDeviceAttachedTest(const DeviceIndices& device_indices) {
  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_unitmask = 0x0;
  volume_broadcast.dbcv_flags = 0x0;

  int expect_attach_calls = 0;
  for (DeviceIndices::const_iterator it = device_indices.begin();
       it != device_indices.end(); ++it) {
    volume_broadcast.dbcv_unitmask |= 0x1 << *it;
    bool removable;
    ASSERT_TRUE(volume_mount_watcher_->GetDeviceInfo(
        VolumeMountWatcherWin::DriveNumberToFilePath(*it),
        NULL, NULL, NULL, &removable, NULL));
    if (removable)
      expect_attach_calls++;
  }
  window_->InjectDeviceChange(DBT_DEVICEARRIVAL,
                              reinterpret_cast<DWORD>(&volume_broadcast));

  RunUntilIdle();
  volume_mount_watcher_->FlushWorkerPoolForTesting();
  RunUntilIdle();

  EXPECT_EQ(expect_attach_calls, observer_.attach_calls());
  EXPECT_EQ(0, observer_.detach_calls());
}

void RemovableDeviceNotificationsWindowWinTest::
    DoMassStorageDevicesDetachedTest(const DeviceIndices& device_indices) {
  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_unitmask = 0x0;
  volume_broadcast.dbcv_flags = 0x0;

  int pre_attach_calls = observer_.attach_calls();
  int expect_detach_calls = 0;
  for (DeviceIndices::const_iterator it = device_indices.begin();
       it != device_indices.end(); ++it) {
    volume_broadcast.dbcv_unitmask |= 0x1 << *it;
    bool removable;
    ASSERT_TRUE(volume_mount_watcher_->GetRawDeviceInfo(
        VolumeMountWatcherWin::DriveNumberToFilePath(*it), NULL, NULL,
        NULL, &removable, NULL));
    if (removable)
      expect_detach_calls++;
  }
  window_->InjectDeviceChange(DBT_DEVICEREMOVECOMPLETE,
                              reinterpret_cast<DWORD>(&volume_broadcast));
  RunUntilIdle();
  EXPECT_EQ(pre_attach_calls, observer_.attach_calls());
  EXPECT_EQ(expect_detach_calls, observer_.detach_calls());
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

  int expect_attach_calls = observer_.attach_calls();
  int expect_detach_calls = observer_.detach_calls();
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
    if (test_attach && !name.empty() && !unique_id.empty())
      expect_attach_calls++;
    else if (!name.empty() && !unique_id.empty())
      expect_detach_calls++;
  }

  window_->InjectDeviceChange(
      test_attach ? DBT_DEVICEARRIVAL : DBT_DEVICEREMOVECOMPLETE,
      reinterpret_cast<DWORD>(dev_interface_broadcast.get()));

  RunUntilIdle();
  EXPECT_EQ(expect_attach_calls, observer_.attach_calls());
  EXPECT_EQ(expect_detach_calls, observer_.detach_calls());
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
  device_indices.push_back(1);  // B
  device_indices.push_back(5);  // F
  device_indices.push_back(7);  // H
  device_indices.push_back(13);  // N
  DoMassStorageDeviceAttachedTest(device_indices);

  string16 location;
  std::string unique_id;
  string16 name;
  bool removable;
  EXPECT_TRUE(window_->volume_mount_watcher()->GetDeviceInfo(
      base::FilePath(ASCIIToUTF16("F:\\")),
      &location, &unique_id, &name, &removable, NULL));
  EXPECT_EQ(ASCIIToUTF16("F:\\"), location);
  EXPECT_EQ("\\\\?\\Volume{F0000000-0000-0000-0000-000000000000}\\", unique_id);
  EXPECT_EQ(ASCIIToUTF16("F:\\ Drive"), name);

  StorageMonitor::StorageInfo info;
  EXPECT_FALSE(window_->GetStorageInfoForPath(
      base::FilePath(ASCIIToUTF16("G:\\")), &info));
  EXPECT_TRUE(window_->GetStorageInfoForPath(
      base::FilePath(ASCIIToUTF16("F:\\")), &info));
  StorageMonitor::StorageInfo info1;
  EXPECT_TRUE(window_->GetStorageInfoForPath(
      base::FilePath(ASCIIToUTF16("F:\\subdir")), &info1));
  StorageMonitor::StorageInfo info2;
  EXPECT_TRUE(window_->GetStorageInfoForPath(
      base::FilePath(ASCIIToUTF16("F:\\subdir\\sub")), &info2));
  EXPECT_EQ(ASCIIToUTF16("F:\\ Drive"), info.name);
  EXPECT_EQ(ASCIIToUTF16("F:\\ Drive"), info1.name);
  EXPECT_EQ(ASCIIToUTF16("F:\\ Drive"), info2.name);
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

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesDetached) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(1);
  device_indices.push_back(5);
  device_indices.push_back(7);
  device_indices.push_back(13);

  DoMassStorageDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest,
       DevicesDetachedHighBoundary) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(25);

  DoMassStorageDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest,
       DevicesDetachedLowBoundary) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(0);

  DoMassStorageDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest,
       DevicesDetachedAdjacentBits) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(0);
  device_indices.push_back(1);
  device_indices.push_back(2);
  device_indices.push_back(3);

  DoMassStorageDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest,
       DuplicateAttachCheckSuppressed) {
  volume_mount_watcher_->BlockDeviceCheckForTesting();
  base::FilePath kAttachedDevicePath =
      VolumeMountWatcherWin::DriveNumberToFilePath(8);  // I:

  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_flags = 0x0;
  volume_broadcast.dbcv_unitmask = 0x100;  // I: drive
  window_->InjectDeviceChange(DBT_DEVICEARRIVAL,
                              reinterpret_cast<DWORD>(&volume_broadcast));

  EXPECT_EQ(0u, volume_mount_watcher_->devices_checked().size());

  // Re-attach the same volume. We haven't released the mock device check
  // event, so there'll be pending calls in the UI thread to finish the
  // device check notification, blocking the duplicate device injection.
  window_->InjectDeviceChange(DBT_DEVICEARRIVAL,
                              reinterpret_cast<DWORD>(&volume_broadcast));

  EXPECT_EQ(0u, volume_mount_watcher_->devices_checked().size());
  volume_mount_watcher_->ReleaseDeviceCheck();
  RunUntilIdle();
  volume_mount_watcher_->ReleaseDeviceCheck();

  // Now let all attach notifications finish running. We'll only get one
  // finish-attach call.
  volume_mount_watcher_->FlushWorkerPoolForTesting();
  RunUntilIdle();

  std::vector<base::FilePath> checked_devices =
      volume_mount_watcher_->devices_checked();
  ASSERT_EQ(1u, checked_devices.size());
  EXPECT_EQ(kAttachedDevicePath, checked_devices[0]);

  // We'll receive a duplicate check now that the first check has fully cleared.
  window_->InjectDeviceChange(DBT_DEVICEARRIVAL,
                              reinterpret_cast<DWORD>(&volume_broadcast));
  volume_mount_watcher_->FlushWorkerPoolForTesting();
  volume_mount_watcher_->ReleaseDeviceCheck();
  RunUntilIdle();

  checked_devices = volume_mount_watcher_->devices_checked();
  ASSERT_EQ(2u, checked_devices.size());
  EXPECT_EQ(kAttachedDevicePath, checked_devices[0]);
  EXPECT_EQ(kAttachedDevicePath, checked_devices[1]);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DeviceInfoForPath) {
  PreAttachDevices();

  // An invalid path.
  EXPECT_FALSE(window_->GetStorageInfoForPath(base::FilePath(L"COM1:\\"),
                                              NULL));

  // An unconnected removable device.
  EXPECT_FALSE(window_->GetStorageInfoForPath(base::FilePath(L"E:\\"), NULL));

  // A connected removable device.
  base::FilePath removable_device(L"F:\\");
  StorageMonitor::StorageInfo device_info;
  EXPECT_TRUE(window_->GetStorageInfoForPath(removable_device, &device_info));

  std::string unique_id;
  string16 device_name;
  string16 location;
  bool removable;
  uint64 total_size_in_bytes;
  ASSERT_TRUE(volume_mount_watcher_->GetDeviceInfo(
      removable_device, &location, &unique_id, &device_name, &removable,
      &total_size_in_bytes));
  EXPECT_TRUE(removable);
  std::string device_id = MediaStorageUtil::MakeDeviceId(
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM, unique_id);
  EXPECT_EQ(device_id, device_info.device_id);
  EXPECT_EQ(device_name, device_info.name);
  EXPECT_EQ(removable_device.value(), device_info.location);
  EXPECT_EQ(1000000, total_size_in_bytes);

  // A fixed device.
  base::FilePath fixed_device(L"N:\\");
  EXPECT_TRUE(window_->GetStorageInfoForPath(fixed_device, &device_info));

  ASSERT_TRUE(volume_mount_watcher_->GetDeviceInfo(
      fixed_device, &location, &unique_id, &device_name, &removable, NULL));
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
