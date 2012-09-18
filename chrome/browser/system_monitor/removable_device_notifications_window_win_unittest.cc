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
#include "base/string_number_conversions.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/mock_devices_changed_observer.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using content::BrowserThread;

LRESULT GetVolumeName(LPCWSTR drive,
                      LPWSTR volume_name,
                      unsigned int volume_name_length) {
  DCHECK(volume_name_length > wcslen(drive) + 2);
  *volume_name = 'V';
  wcscpy(volume_name + 1, drive);
  return TRUE;
}

}  // namespace

namespace chrome {

using chrome::RemovableDeviceNotificationsWindowWin;
using testing::_;

class RemovableDeviceNotificationsWindowWinTest : public testing::Test {
 public:
  RemovableDeviceNotificationsWindowWinTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE) { }
  virtual ~RemovableDeviceNotificationsWindowWinTest() { }

 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    file_thread_.Start();
    window_ = new RemovableDeviceNotificationsWindowWin(&GetVolumeName);
    system_monitor_.AddDevicesChangedObserver(&observer_);
  }

  virtual void TearDown() {
    system_monitor_.RemoveDevicesChangedObserver(&observer_);
    WaitForFileThread();
  }

  static void PostQuitToUIThread() {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            MessageLoop::QuitClosure());
  }

  static void WaitForFileThread() {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&PostQuitToUIThread));
    MessageLoop::current()->Run();
  }

  void DoDevicesAttachedTest(const std::vector<int>& device_indices);
  void DoDevicesDetachedTest(const std::vector<int>& device_indices);

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  base::SystemMonitor system_monitor_;
  base::MockDevicesChangedObserver observer_;
  scoped_refptr<RemovableDeviceNotificationsWindowWin> window_;
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
      std::wstring drive(L"_:\\");
      drive[0] = 'A' + *it;
      FilePath::StringType name = L"V" + drive;
      std::string device_id = MediaStorageUtil::MakeDeviceId(
          MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM,
          base::IntToString(*it));
      EXPECT_CALL(observer_, OnRemovableStorageAttached(device_id, name, drive))
          .Times(0);
    }
  }
  window_->OnDeviceChange(DBT_DEVICEARRIVAL,
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
      std::string device_id = MediaStorageUtil::MakeDeviceId(
          MediaStorageUtil::REMOVABLE_MASS_STORAGE_WITH_DCIM,
          base::IntToString(*it));
      EXPECT_CALL(observer_, OnRemovableStorageDetached(device_id)).Times(0);
    }
  }
  window_->OnDeviceChange(DBT_DEVICEREMOVECOMPLETE,
                          reinterpret_cast<DWORD>(&volume_broadcast));
  message_loop_.RunAllPending();
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, RandomMessage) {
  window_->OnDeviceChange(DBT_DEVICEQUERYREMOVE, NULL);
  message_loop_.RunAllPending();
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesAttached) {
  std::vector<int> device_indices;
  device_indices.push_back(1);
  device_indices.push_back(5);
  device_indices.push_back(7);

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
  std::vector<int> device_indices;
  device_indices.push_back(1);
  device_indices.push_back(5);
  device_indices.push_back(7);

  DoDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesDetachedHighBoundary) {
  std::vector<int> device_indices;
  device_indices.push_back(25);

  DoDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesDetachedLowBoundary) {
  std::vector<int> device_indices;
  device_indices.push_back(0);

  DoDevicesDetachedTest(device_indices);
}

TEST_F(RemovableDeviceNotificationsWindowWinTest, DevicesDetachedAdjacentBits) {
  std::vector<int> device_indices;
  device_indices.push_back(0);
  device_indices.push_back(1);
  device_indices.push_back(2);
  device_indices.push_back(3);

  DoDevicesDetachedTest(device_indices);
}

}  // namespace chrome
