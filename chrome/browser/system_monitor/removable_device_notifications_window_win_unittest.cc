// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/removable_device_notifications_window_win.h"

#include <dbt.h>

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/mock_devices_changed_observer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/browser/system_monitor/portable_device_watcher_win.h"
#include "chrome/browser/system_monitor/removable_device_constants.h"
#include "chrome/browser/system_monitor/volume_mount_watcher_win.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

typedef std::vector<int> DeviceIndices;
typedef std::vector<FilePath> AttachedDevices;

namespace {

using content::BrowserThread;

// MTP device interface path constants.
const char16 kMTPDeviceWithInvalidInfo[] =
    L"\\?\usb#vid_00&pid_00#0&2&1#{0000-0000-0000-0000-0000})";
const char16 kMTPDeviceWithValidInfo[] =
    L"\\?\usb#vid_ff&pid_000f#32&2&1#{abcd-1234-ffde-1112-9172})";
const char16 kMTPDeviceWithMultipleStorageObjects[] =
    L"\\?\usb#vid_ff&pid_18#32&2&1#{ab33-1de4-f22e-1882-9724})";

// Sample MTP device storage information.
const char16 kMTPDeviceFriendlyName[] = L"Camera V1.1";
const char16 kStorageLabelA[] = L"Camera V1.1 (s10001)";
const char16 kStorageLabelB[] = L"Camera V1.1 (s20001)";
const char16 kStorageObjectIdA[] = L"s10001";
const char16 kStorageObjectIdB[] = L"s20001";
const char kStorageUniqueIdA[] =
    "mtp:StorageSerial:SID-{s10001, D, 12378}:123123";
const char kStorageUniqueIdB[] =
    "mtp:StorageSerial:SID-{s20001, S, 2238}:123123";

// Inputs of 'A:\' - 'Z:\' are valid. 'N:\' is not removable.
bool GetMassStorageDeviceDetails(const FilePath& device_path,
                                 string16* device_location,
                                 std::string* unique_id,
                                 string16* name,
                                 bool* removable) {
  if (device_path.value().length() != 3 || device_path.value()[0] < L'A' ||
      device_path.value()[0] > L'Z') {
    return false;
  }

  if (device_location)
    *device_location = device_path.value();
  if (unique_id) {
    *unique_id = "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\";
    (*unique_id)[11] = device_path.value()[0];
  }
  if (name)
    *name = device_path.Append(L" Drive").LossyDisplayName();
  if (removable)
    *removable = device_path.value()[0] != L'N';
  return true;
}

FilePath DriveNumberToFilePath(int drive_number) {
  FilePath::StringType path(L"_:\\");
  path[0] = L'A' + drive_number;
  return FilePath(path);
}

AttachedDevices GetTestAttachedDevices() {
  AttachedDevices result;
  result.push_back(DriveNumberToFilePath(0));
  result.push_back(DriveNumberToFilePath(1));
  result.push_back(DriveNumberToFilePath(2));
  result.push_back(DriveNumberToFilePath(3));
  result.push_back(DriveNumberToFilePath(5));
  result.push_back(DriveNumberToFilePath(7));
  result.push_back(DriveNumberToFilePath(25));
  return result;
}

// Returns the persistent storage unique id of the device specified by the
// |pnp_device_id|. |storage_object_id| specifies the temporary object
// identifier that uniquely identifies the object on the device.
std::string GetMTPStorageUniqueId(const string16& pnp_device_id,
                                  const string16& storage_object_id)  {
  if (storage_object_id == kStorageObjectIdA)
    return kStorageUniqueIdA;
  return (storage_object_id == kStorageObjectIdB) ?
      kStorageUniqueIdB : std::string();
}

// Returns the storage name of the device specified by |pnp_device_id|.
// |storage_object_id| specifies the temporary object identifier that
// uniquely identifies the object on the device.
string16 GetMTPStorageName(const string16& pnp_device_id,
                           const string16& storage_object_id) {
  if (pnp_device_id == kMTPDeviceWithInvalidInfo)
    return string16();

  if (storage_object_id == kStorageObjectIdA)
    return kStorageLabelA;
  return (storage_object_id == kStorageObjectIdB) ?
      kStorageLabelB : string16();
}

// Returns a list of storage object identifiers of the device given a
// |pnp_device_id|.
PortableDeviceWatcherWin::StorageObjectIDs GetMTPStorageObjectIds(
    const string16& pnp_device_id) {
  PortableDeviceWatcherWin::StorageObjectIDs storage_object_ids;
  storage_object_ids.push_back(kStorageObjectIdA);
  if (pnp_device_id == kMTPDeviceWithMultipleStorageObjects)
    storage_object_ids.push_back(kStorageObjectIdB);
  return storage_object_ids;
}

// Gets the MTP device storage details given a |pnp_device_id| and
// |storage_object_id|. On success, returns true and fills in
// |device_location|, |unique_id| and |name|.
void GetMTPStorageDetails(const string16& pnp_device_id,
                          const string16& storage_object_id,
                          string16* device_location,
                          std::string* unique_id,
                          string16* name) {
  std::string storage_unique_id = GetMTPStorageUniqueId(pnp_device_id,
                                                        storage_object_id);

  if (device_location)
    *device_location = UTF8ToUTF16("\\\\" + storage_unique_id);

  if (unique_id)
    *unique_id = storage_unique_id;

  if (name)
    *name = GetMTPStorageName(pnp_device_id, storage_object_id);
}

// Returns a list of device storage details for the given device specified by
// |pnp_device_id|.
PortableDeviceWatcherWin::StorageObjects GetDeviceStorageObjects(
    const string16& pnp_device_id) {
  PortableDeviceWatcherWin::StorageObjects storage_objects;
  PortableDeviceWatcherWin::StorageObjectIDs storage_object_ids =
      GetMTPStorageObjectIds(pnp_device_id);
  for (PortableDeviceWatcherWin::StorageObjectIDs::const_iterator it =
       storage_object_ids.begin(); it != storage_object_ids.end(); ++it) {
    storage_objects.push_back(PortableDeviceWatcherWin::DeviceStorageObject(
        *it, GetMTPStorageUniqueId(pnp_device_id, *it)));
  }
  return storage_objects;
}

}  // namespace


// TestPortableDeviceWatcherWin -----------------------------------------------

class TestPortableDeviceWatcherWin : public PortableDeviceWatcherWin {
 public:
  TestPortableDeviceWatcherWin();
  virtual ~TestPortableDeviceWatcherWin();

 private:
  // PortableDeviceWatcherWin:
  virtual void EnumerateAttachedDevices() OVERRIDE;
  virtual void HandleDeviceAttachEvent(const string16& pnp_device_id) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TestPortableDeviceWatcherWin);
};

TestPortableDeviceWatcherWin::TestPortableDeviceWatcherWin() {
}

TestPortableDeviceWatcherWin::~TestPortableDeviceWatcherWin() {
}

void TestPortableDeviceWatcherWin::EnumerateAttachedDevices() {
}

void TestPortableDeviceWatcherWin::HandleDeviceAttachEvent(
    const string16& pnp_device_id) {
  DeviceDetails device_details = {
      (pnp_device_id != kMTPDeviceWithInvalidInfo) ?
          kMTPDeviceFriendlyName : string16(),
      pnp_device_id,
      GetDeviceStorageObjects(pnp_device_id)
  };
  OnDidHandleDeviceAttachEvent(&device_details, true);
}


// TestVolumeMountWatcherWin --------------------------------------------------

class TestVolumeMountWatcherWin : public VolumeMountWatcherWin {
 public:
  TestVolumeMountWatcherWin();

  void set_pre_attach_devices(bool pre_attach_devices) {
    pre_attach_devices_ = pre_attach_devices;
  }

 private:
  // Private, this class is ref-counted.
  virtual ~TestVolumeMountWatcherWin();

  // VolumeMountWatcherWin:
  virtual bool GetDeviceInfo(const FilePath& device_path,
                             string16* device_location,
                             std::string* unique_id,
                             string16* name,
                             bool* removable) OVERRIDE;
  virtual AttachedDevices GetAttachedDevices() OVERRIDE;

  // Set to true to pre-attach test devices.
  bool pre_attach_devices_;

  DISALLOW_COPY_AND_ASSIGN(TestVolumeMountWatcherWin);
};

TestVolumeMountWatcherWin::TestVolumeMountWatcherWin()
    : pre_attach_devices_(false) {
}

TestVolumeMountWatcherWin::~TestVolumeMountWatcherWin() {
}

bool TestVolumeMountWatcherWin::GetDeviceInfo(const FilePath& device_path,
                                              string16* device_location,
                                              std::string* unique_id,
                                              string16* name,
                                              bool* removable) {
  return GetMassStorageDeviceDetails(device_path, device_location, unique_id,
                                     name, removable);
}

AttachedDevices TestVolumeMountWatcherWin::GetAttachedDevices() {
  return pre_attach_devices_ ?
      GetTestAttachedDevices() : AttachedDevices();
}


// TestRemovableDeviceNotificationsWindowWin -----------------------------------

class TestRemovableDeviceNotificationsWindowWin
    : public RemovableDeviceNotificationsWindowWin {
 public:
  TestRemovableDeviceNotificationsWindowWin(
      TestVolumeMountWatcherWin* volume_mount_watcher,
      TestPortableDeviceWatcherWin* portable_device_watcher);

  virtual ~TestRemovableDeviceNotificationsWindowWin();

  void InitWithTestData(bool pre_attach_devices);
  void InjectDeviceChange(UINT event_type, DWORD data);

 private:
  scoped_refptr<TestVolumeMountWatcherWin> volume_mount_watcher_;
};

TestRemovableDeviceNotificationsWindowWin::
    TestRemovableDeviceNotificationsWindowWin(
    TestVolumeMountWatcherWin* volume_mount_watcher,
    TestPortableDeviceWatcherWin* portable_device_watcher)
    : RemovableDeviceNotificationsWindowWin(volume_mount_watcher,
                                            portable_device_watcher),
      volume_mount_watcher_(volume_mount_watcher) {
  DCHECK(volume_mount_watcher_);
}

TestRemovableDeviceNotificationsWindowWin::
    ~TestRemovableDeviceNotificationsWindowWin() {
}

void TestRemovableDeviceNotificationsWindowWin::InitWithTestData(
    bool pre_attach_devices) {
  volume_mount_watcher_->set_pre_attach_devices(pre_attach_devices);
  Init();
}

void TestRemovableDeviceNotificationsWindowWin::InjectDeviceChange(
    UINT event_type,
    DWORD data) {
  OnDeviceChange(event_type, data);
}


// RemovableDeviceNotificationsWindowWinTest -----------------------------------

class RemovableDeviceNotificationsWindowWinTest : public testing::Test {
 public:
  RemovableDeviceNotificationsWindowWinTest();
  virtual ~RemovableDeviceNotificationsWindowWinTest();

 protected:
  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void AddMassStorageDeviceAttachExpectation(FilePath drive);
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

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  base::SystemMonitor system_monitor_;
  base::MockDevicesChangedObserver observer_;
  scoped_refptr<TestVolumeMountWatcherWin> volume_mount_watcher_;
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
    AddMassStorageDeviceAttachExpectation(FilePath drive) {
  std::string unique_id;
  string16 device_name;
  bool removable;
  ASSERT_TRUE(GetMassStorageDeviceDetails(drive, NULL, &unique_id,
                                          &device_name, &removable));
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
    AttachedDevices initial_devices = GetTestAttachedDevices();
    for (AttachedDevices::const_iterator it = initial_devices.begin();
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
      AddMassStorageDeviceAttachExpectation(DriveNumberToFilePath(*it));
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
      ASSERT_TRUE(GetMassStorageDeviceDetails(DriveNumberToFilePath(*it), NULL,
                                              &unique_id, NULL, &removable));
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
        GetMTPStorageObjectIds(pnp_device_id);
    for (PortableDeviceWatcherWin::StorageObjectIDs::const_iterator it =
         storage_object_ids.begin(); it != storage_object_ids.end(); ++it) {
      std::string unique_id;
      string16 name;
      string16 location;
      GetMTPStorageDetails(pnp_device_id, *it, &location, &unique_id, &name);
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
  ASSERT_TRUE(GetMassStorageDeviceDetails(removable_device, NULL, &unique_id,
                                          &device_name, &removable));
  EXPECT_TRUE(removable);
  std::string device_id = MediaStorageUtil::MakeDeviceId(
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM, unique_id);
  EXPECT_EQ(device_id, device_info.device_id);
  EXPECT_EQ(device_name, device_info.name);
  EXPECT_EQ(removable_device.value(), device_info.location);

  // A fixed device.
  FilePath fixed_device(L"N:\\");
  EXPECT_TRUE(window_->GetDeviceInfoForPath(fixed_device, &device_info));

  ASSERT_TRUE(GetMassStorageDeviceDetails(fixed_device, NULL, &unique_id,
                                          &device_name, &removable));
  EXPECT_FALSE(removable);
  device_id = MediaStorageUtil::MakeDeviceId(
      MediaStorageUtil::FIXED_MASS_STORAGE, unique_id);
  EXPECT_EQ(device_id, device_info.device_id);
  EXPECT_EQ(device_name, device_info.name);
  EXPECT_EQ(fixed_device.value(), device_info.location);
}

// Test to verify basic MTP storage attach and detach notifications.
TEST_F(RemovableDeviceNotificationsWindowWinTest, MTPDeviceBasicAttachDetach) {
  DoMTPDeviceTest(kMTPDeviceWithValidInfo, true);
  DoMTPDeviceTest(kMTPDeviceWithValidInfo, false);
}

// When a MTP storage device with invalid storage label and id is
// attached/detached, there should not be any device attach/detach
// notifications.
TEST_F(RemovableDeviceNotificationsWindowWinTest, MTPDeviceWithInvalidInfo) {
  DoMTPDeviceTest(kMTPDeviceWithInvalidInfo, true);
  DoMTPDeviceTest(kMTPDeviceWithInvalidInfo, false);
}

// Attach a device with two data partitions. Verify that attach/detach
// notifications are sent out for each removable storage.
TEST_F(RemovableDeviceNotificationsWindowWinTest,
       MTPDeviceWithMultipleStorageObjects) {
  DoMTPDeviceTest(kMTPDeviceWithMultipleStorageObjects, true);
  DoMTPDeviceTest(kMTPDeviceWithMultipleStorageObjects, false);
}

// Given a MTP storage persistent id, GetMTPStorageInfo() should fetch the
// device interface path and local storage object identifier.
TEST_F(RemovableDeviceNotificationsWindowWinTest,
       GetMTPStorageInfoFromDeviceId) {
  DoMTPDeviceTest(kMTPDeviceWithValidInfo, true);
  PortableDeviceWatcherWin::StorageObjects storage_objects =
      GetDeviceStorageObjects(kMTPDeviceWithValidInfo);
  for (PortableDeviceWatcherWin::StorageObjects::const_iterator it =
       storage_objects.begin(); it != storage_objects.end(); ++it) {
    string16 pnp_device_id;
    string16 storage_object_id;
    ASSERT_TRUE(GetMTPStorageInfo(it->object_persistent_id, &pnp_device_id,
                                  &storage_object_id));
    EXPECT_EQ(kMTPDeviceWithValidInfo, pnp_device_id);
    EXPECT_EQ(it->object_persistent_id,
              GetMTPStorageUniqueId(pnp_device_id, storage_object_id));
  }

  DoMTPDeviceTest(kMTPDeviceWithValidInfo, false);
}

}  // namespace chrome
