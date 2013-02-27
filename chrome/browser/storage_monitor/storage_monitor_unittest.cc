// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/mock_removable_storage_observer.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/storage_monitor/test_storage_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

TEST(StorageMonitorTest, DeviceAttachDetachNotifications) {
  MessageLoop message_loop;
  const string16 kDeviceName = ASCIIToUTF16("media device");
  const std::string kDeviceId1 = "1";
  const std::string kDeviceId2 = "2";
  MockRemovableStorageObserver observer1;
  MockRemovableStorageObserver observer2;
  test::TestStorageMonitor monitor;
  monitor.AddObserver(&observer1);
  monitor.AddObserver(&observer2);

  monitor.receiver()->ProcessAttach(StorageMonitor::StorageInfo(
      kDeviceId1, kDeviceName, FILE_PATH_LITERAL("path")));
  message_loop.RunUntilIdle();

  EXPECT_EQ(kDeviceId1, observer1.last_attached().device_id);
  EXPECT_EQ(kDeviceName, observer1.last_attached().name);
  EXPECT_EQ(FILE_PATH_LITERAL("path"), observer1.last_attached().location);
  EXPECT_EQ(kDeviceId1, observer2.last_attached().device_id);
  EXPECT_EQ(kDeviceName, observer2.last_attached().name);
  EXPECT_EQ(FILE_PATH_LITERAL("path"), observer2.last_attached().location);
  EXPECT_EQ(1, observer1.attach_calls());
  EXPECT_EQ(0, observer1.detach_calls());

  monitor.receiver()->ProcessDetach(kDeviceId1);
  monitor.receiver()->ProcessDetach(kDeviceId2);
  message_loop.RunUntilIdle();

  EXPECT_EQ(kDeviceId1, observer1.last_detached().device_id);
  EXPECT_EQ(kDeviceName, observer1.last_detached().name);
  EXPECT_EQ(FILE_PATH_LITERAL("path"), observer1.last_detached().location);
  EXPECT_EQ(kDeviceId1, observer2.last_detached().device_id);
  EXPECT_EQ(kDeviceName, observer2.last_detached().name);
  EXPECT_EQ(FILE_PATH_LITERAL("path"), observer2.last_detached().location);

  EXPECT_EQ(1, observer1.attach_calls());
  EXPECT_EQ(1, observer2.attach_calls());

  // The kDeviceId2 won't be notified since it was never attached.
  EXPECT_EQ(1, observer1.detach_calls());
  EXPECT_EQ(1, observer2.detach_calls());

  monitor.RemoveObserver(&observer1);
  monitor.RemoveObserver(&observer2);
}

TEST(StorageMonitorTest, GetAttachedStorageEmpty) {
  MessageLoop message_loop;
  test::TestStorageMonitor monitor;
  std::vector<StorageMonitor::StorageInfo> devices =
      monitor.GetAttachedStorage();
  EXPECT_EQ(0U, devices.size());
}

TEST(StorageMonitorTest,
     GetRemovableStorageAttachDetach) {
  MessageLoop message_loop;
  test::TestStorageMonitor monitor;
  const std::string kDeviceId1 = "42";
  const string16 kDeviceName1 = ASCIIToUTF16("test");
  const base::FilePath kDevicePath1(FILE_PATH_LITERAL("/testfoo"));
  monitor.receiver()->ProcessAttach(StorageMonitor::StorageInfo(
      kDeviceId1, kDeviceName1, kDevicePath1.value()));
  message_loop.RunUntilIdle();
  std::vector<StorageMonitor::StorageInfo> devices =
      monitor.GetAttachedStorage();
  ASSERT_EQ(1U, devices.size());
  EXPECT_EQ(kDeviceId1, devices[0].device_id);
  EXPECT_EQ(kDeviceName1, devices[0].name);
  EXPECT_EQ(kDevicePath1.value(), devices[0].location);

  const std::string kDeviceId2 = "44";
  const string16 kDeviceName2 = ASCIIToUTF16("test2");
  const base::FilePath kDevicePath2(FILE_PATH_LITERAL("/testbar"));
  monitor.receiver()->ProcessAttach(StorageMonitor::StorageInfo(
      kDeviceId2, kDeviceName2, kDevicePath2.value()));
  message_loop.RunUntilIdle();
  devices = monitor.GetAttachedStorage();
  ASSERT_EQ(2U, devices.size());
  EXPECT_EQ(kDeviceId1, devices[0].device_id);
  EXPECT_EQ(kDeviceName1, devices[0].name);
  EXPECT_EQ(kDevicePath1.value(), devices[0].location);
  EXPECT_EQ(kDeviceId2, devices[1].device_id);
  EXPECT_EQ(kDeviceName2, devices[1].name);
  EXPECT_EQ(kDevicePath2.value(), devices[1].location);

  monitor.receiver()->ProcessDetach(kDeviceId1);
  message_loop.RunUntilIdle();
  devices = monitor.GetAttachedStorage();
  ASSERT_EQ(1U, devices.size());
  EXPECT_EQ(kDeviceId2, devices[0].device_id);
  EXPECT_EQ(kDeviceName2, devices[0].name);
  EXPECT_EQ(kDevicePath2.value(), devices[0].location);

  monitor.receiver()->ProcessDetach(kDeviceId2);
  message_loop.RunUntilIdle();
  devices = monitor.GetAttachedStorage();
  EXPECT_EQ(0U, devices.size());
}

}  // namespace chrome
