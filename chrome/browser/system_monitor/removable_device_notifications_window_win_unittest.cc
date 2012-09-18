// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/system_monitor/removable_device_notifications_window_win.h"

#include <dbt.h>

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/mock_devices_changed_observer.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using content::BrowserThread;
using chrome::RemovableDeviceNotificationsWindowWin;

// Inputs of 'A:\' - 'Z:\' are valid. 'N:\' is not removable.
bool GetDeviceInfo(const FilePath& device_path, string16* device_location,
                   std::string* unique_id, string16* name, bool* removable) {
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

std::vector<FilePath> GetTestAttachedDevices() {
  std::vector<FilePath> result;
  result.push_back(DriveNumberToFilePath(0));
  result.push_back(DriveNumberToFilePath(1));
  result.push_back(DriveNumberToFilePath(2));
  result.push_back(DriveNumberToFilePath(3));
  result.push_back(DriveNumberToFilePath(5));
  result.push_back(DriveNumberToFilePath(7));
  result.push_back(DriveNumberToFilePath(25));
  return result;
}

}  // namespace

namespace chrome {

class TestRemovableDeviceNotificationsWindowWin
    : public RemovableDeviceNotificationsWindowWin {
 public:
  TestRemovableDeviceNotificationsWindowWin() {
  }

  void InitWithTestData() {
    InitForTest(&GetDeviceInfo, &NoAttachedDevices);
  }

  void InitWithTestDataAndAttachedDevices() {
    InitForTest(&GetDeviceInfo, &GetTestAttachedDevices);
  }

  void InjectDeviceChange(UINT event_type, DWORD data) {
    OnDeviceChange(event_type, data);
  }

 private:
  virtual ~TestRemovableDeviceNotificationsWindowWin() {
  }

  static std::vector<FilePath> NoAttachedDevices() {
    std::vector<FilePath> result;
    return result;
  }
};

class RemovableDeviceNotificationsWindowWinTest : public testing::Test {
 public:
  RemovableDeviceNotificationsWindowWinTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) { }
  virtual ~RemovableDeviceNotificationsWindowWinTest() { }

 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    window_ = new TestRemovableDeviceNotificationsWindowWin;
    window_->InitWithTestData();
    system_monitor_.AddDevicesChangedObserver(&observer_);
  }

  virtual void TearDown() {
    message_loop_.RunAllPending();
    system_monitor_.RemoveDevicesChangedObserver(&observer_);
  }

  void AddAttachExpectation(FilePath drive) {
    std::string unique_id;
    string16 device_name;
    bool removable;
    ASSERT_TRUE(GetDeviceInfo(drive, NULL, &unique_id, &device_name,
                &removable));
    if (removable) {
      MediaStorageUtil::Type type =
          MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM;
      std::string device_id = MediaStorageUtil::MakeDeviceId(type, unique_id);
      EXPECT_CALL(observer_, OnRemovableStorageAttached(device_id, device_name,
                                                        drive.value()))
          .Times(1);
    }
  }

  void PreAttachDevices() {
    window_ = NULL;
    {
      testing::InSequence sequence;
      std::vector<FilePath> initial_devices = GetTestAttachedDevices();
      for (size_t i = 0; i < initial_devices.size(); i++)
        AddAttachExpectation(initial_devices[i]);
    }
    window_ = new TestRemovableDeviceNotificationsWindowWin();
    window_->InitWithTestDataAndAttachedDevices();
    message_loop_.RunAllPending();
  }

  void DoDevicesAttachedTest(const std::vector<int>& device_indices);
  void DoDevicesDetachedTest(const std::vector<int>& device_indices);

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  base::SystemMonitor system_monitor_;
  base::MockDevicesChangedObserver observer_;
  scoped_refptr<TestRemovableDeviceNotificationsWindowWin> window_;
};

void RemovableDeviceNotificationsWindowWinTest::DoDevicesAttachedTest(
    const std::vector<int>& device_indices) {
  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_unitmask = 0x0;
  volume_broadcast.dbcv_flags = 0x0;
  {
    testing::InSequence sequence;
    for (std::vector<int>::const_iterator it = device_indices.begin();
         it != device_indices.end();
         ++it) {
      volume_broadcast.dbcv_unitmask |= 0x1 << *it;
      AddAttachExpectation(DriveNumberToFilePath(*it));
    }
  }
  window_->InjectDeviceChange(DBT_DEVICEARRIVAL,
                              reinterpret_cast<DWORD>(&volume_broadcast));
  message_loop_.RunAllPending();
}

void RemovableDeviceNotificationsWindowWinTest::DoDevicesDetachedTest(
    const std::vector<int>& device_indices) {
  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_unitmask = 0x0;
  volume_broadcast.dbcv_flags = 0x0;
  {
    testing::InSequence sequence;
    for (std::vector<int>::const_iterator it = device_indices.begin();
         it != device_indices.end();
         ++it) {
      volume_broadcast.dbcv_unitmask |= 0x1 << *it;
      std::string unique_id;
      bool removable;
      ASSERT_TRUE(GetDeviceInfo(DriveNumberToFilePath(*it), NULL, &unique_id,
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
  message_loop_.RunAllPending();
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, RandomMessage) {
  window_->InjectDeviceChange(DBT_DEVICEQUERYREMOVE, NULL);
  message_loop_.RunAllPending();
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesAttached) {
  std::vector<int> device_indices;
  device_indices.push_back(1);
  device_indices.push_back(5);
  device_indices.push_back(7);
  device_indices.push_back(13);

  DoDevicesAttachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesAttachedHighBoundary) {
  std::vector<int> device_indices;
  device_indices.push_back(25);

  DoDevicesAttachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesAttachedLowBoundary) {
  std::vector<int> device_indices;
  device_indices.push_back(0);

  DoDevicesAttachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesAttachedAdjacentBits) {
  std::vector<int> device_indices;
  device_indices.push_back(0);
  device_indices.push_back(1);
  device_indices.push_back(2);
  device_indices.push_back(3);

  DoDevicesAttachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesDetached) {
  PreAttachDevices();

  std::vector<int> device_indices;
  device_indices.push_back(1);
  device_indices.push_back(5);
  device_indices.push_back(7);
  device_indices.push_back(13);

  DoDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesDetachedHighBoundary) {
  PreAttachDevices();

  std::vector<int> device_indices;
  device_indices.push_back(25);

  DoDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesDetachedLowBoundary) {
  PreAttachDevices();

  std::vector<int> device_indices;
  device_indices.push_back(0);

  DoDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesDetachedAdjacentBits) {
  PreAttachDevices();

  std::vector<int> device_indices;
  device_indices.push_back(0);
  device_indices.push_back(1);
  device_indices.push_back(2);
  device_indices.push_back(3);

  DoDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DeviceInfoFoPath) {
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
  ASSERT_TRUE(GetDeviceInfo(removable_device, NULL, &unique_id, &device_name,
              &removable));
  EXPECT_TRUE(removable);
  std::string device_id = MediaStorageUtil::MakeDeviceId(
      MediaStorageUtil::REMOVABLE_MASS_STORAGE_NO_DCIM, unique_id);
  EXPECT_EQ(device_id, device_info.device_id);
  EXPECT_EQ(device_name, device_info.name);
  EXPECT_EQ(removable_device.value(), device_info.location);

  // A fixed device.
  FilePath fixed_device(L"N:\\");
  EXPECT_TRUE(window_->GetDeviceInfoForPath(fixed_device, &device_info));

  ASSERT_TRUE(GetDeviceInfo(fixed_device, NULL, &unique_id, &device_name,
              &removable));
  EXPECT_FALSE(removable);
  device_id = MediaStorageUtil::MakeDeviceId(
      MediaStorageUtil::FIXED_MASS_STORAGE, unique_id);
  EXPECT_EQ(device_id, device_info.device_id);
  EXPECT_EQ(device_name, device_info.name);
  EXPECT_EQ(fixed_device.value(), device_info.location);
}

}  // namespace chrome
