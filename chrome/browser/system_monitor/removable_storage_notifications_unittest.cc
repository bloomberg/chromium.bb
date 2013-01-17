// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/mock_removable_storage_observer.h"
#include "chrome/browser/system_monitor/removable_storage_notifications.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

class TestStorageNotifications : public RemovableStorageNotifications {
 public:
  TestStorageNotifications() {
  }

  ~TestStorageNotifications() {}

  virtual bool GetDeviceInfoForPath(
      const FilePath& path,
      base::SystemMonitor::RemovableStorageInfo* device_info) const OVERRIDE {
    return false;
  }
  virtual uint64 GetStorageSize(const std::string& location) const OVERRIDE {
    return 0;
  }

#if defined(OS_WIN)
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const OVERRIDE {
    return false;
  }
#endif

  void ProcessAttach(const std::string& id,
                     const string16& name,
                     const FilePath::StringType& location) {
    RemovableStorageNotifications::ProcessAttach(id, name, location);
  }

  void ProcessDetach(const std::string& id) {
    RemovableStorageNotifications::ProcessDetach(id);
  }
};

TEST(RemovableStorageNotificationsTest, DeviceAttachDetachNotifications) {
  MessageLoop message_loop;
  const string16 kDeviceName = ASCIIToUTF16("media device");
  const std::string kDeviceId1 = "1";
  const std::string kDeviceId2 = "2";
  MockRemovableStorageObserver observer1;
  MockRemovableStorageObserver observer2;
  TestStorageNotifications notifications;
  notifications.AddObserver(&observer1);
  notifications.AddObserver(&observer2);

  notifications.ProcessAttach(kDeviceId1,
                              kDeviceName,
                              FILE_PATH_LITERAL("path"));
  message_loop.RunUntilIdle();

  EXPECT_EQ(kDeviceId1, observer1.last_attached().device_id);
  EXPECT_EQ(kDeviceName, observer1.last_attached().name);
  EXPECT_EQ(FILE_PATH_LITERAL("path"), observer1.last_attached().location);
  EXPECT_EQ(kDeviceId1, observer2.last_attached().device_id);
  EXPECT_EQ(kDeviceName, observer2.last_attached().name);
  EXPECT_EQ(FILE_PATH_LITERAL("path"), observer2.last_attached().location);
  EXPECT_EQ(1, observer1.attach_calls());
  EXPECT_EQ(0, observer1.detach_calls());

  notifications.ProcessDetach(kDeviceId1);
  notifications.ProcessDetach(kDeviceId2);
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

  notifications.RemoveObserver(&observer1);
  notifications.RemoveObserver(&observer2);
}

TEST(RemovableStorageNotificationsTest, GetAttachedStorageEmpty) {
  MessageLoop message_loop;
  TestStorageNotifications notifications;
  std::vector<RemovableStorageNotifications::StorageInfo> devices =
      notifications.GetAttachedStorage();
  EXPECT_EQ(0U, devices.size());
}

TEST(RemovableStorageNotificationsTest,
     GetRemovableStorageAttachDetach) {
  MessageLoop message_loop;
  TestStorageNotifications notifications;
  const std::string kDeviceId1 = "42";
  const string16 kDeviceName1 = ASCIIToUTF16("test");
  const FilePath kDevicePath1(FILE_PATH_LITERAL("/testfoo"));
  notifications.ProcessAttach(kDeviceId1, kDeviceName1, kDevicePath1.value());
  message_loop.RunUntilIdle();
  std::vector<RemovableStorageNotifications::StorageInfo> devices =
      notifications.GetAttachedStorage();
  ASSERT_EQ(1U, devices.size());
  EXPECT_EQ(kDeviceId1, devices[0].device_id);
  EXPECT_EQ(kDeviceName1, devices[0].name);
  EXPECT_EQ(kDevicePath1.value(), devices[0].location);

  const std::string kDeviceId2 = "44";
  const string16 kDeviceName2 = ASCIIToUTF16("test2");
  const FilePath kDevicePath2(FILE_PATH_LITERAL("/testbar"));
  notifications.ProcessAttach(kDeviceId2, kDeviceName2, kDevicePath2.value());
  message_loop.RunUntilIdle();
  devices = notifications.GetAttachedStorage();
  ASSERT_EQ(2U, devices.size());
  EXPECT_EQ(kDeviceId1, devices[0].device_id);
  EXPECT_EQ(kDeviceName1, devices[0].name);
  EXPECT_EQ(kDevicePath1.value(), devices[0].location);
  EXPECT_EQ(kDeviceId2, devices[1].device_id);
  EXPECT_EQ(kDeviceName2, devices[1].name);
  EXPECT_EQ(kDevicePath2.value(), devices[1].location);

  notifications.ProcessDetach(kDeviceId1);
  message_loop.RunUntilIdle();
  devices = notifications.GetAttachedStorage();
  ASSERT_EQ(1U, devices.size());
  EXPECT_EQ(kDeviceId2, devices[0].device_id);
  EXPECT_EQ(kDeviceName2, devices[0].name);
  EXPECT_EQ(kDevicePath2.value(), devices[0].location);

  notifications.ProcessDetach(kDeviceId2);
  message_loop.RunUntilIdle();
  devices = notifications.GetAttachedStorage();
  EXPECT_EQ(0U, devices.size());
}

}  // namespace chrome
