// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/device_event_router.h"

#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace {

namespace file_manager_private = extensions::api::file_manager_private;
typedef chromeos::disks::DiskMountManager::Disk Disk;

const char kTestDevicePath[] = "/device/test";

struct DeviceEvent {
  extensions::api::file_manager_private::DeviceEventType type;
  std::string device_path;
};

// DeviceEventRouter implementation for testing.
class DeviceEventRouterImpl : public DeviceEventRouter {
 public:
  DeviceEventRouterImpl()
      : DeviceEventRouter(base::TimeDelta::FromSeconds(0)),
        external_storage_disabled(false) {}
  virtual ~DeviceEventRouterImpl() {}

  // DeviceEventRouter overrides.
  virtual void OnDeviceEvent(file_manager_private::DeviceEventType type,
                             const std::string& device_path) OVERRIDE {
    DeviceEvent event;
    event.type = type;
    event.device_path = device_path;
    events.push_back(event);
  }

  // DeviceEventRouter overrides.
  virtual bool IsExternalStorageDisabled() OVERRIDE {
    return external_storage_disabled;
  }

  // List of dispatched events.
  std::vector<DeviceEvent> events;

  // Flag returned by |IsExternalStorageDisabled|.
  bool external_storage_disabled;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceEventRouterImpl);
};

}  // namespace

class DeviceEventRouterTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    device_event_router.reset(new DeviceEventRouterImpl());
  }

  // Creates a disk instance with |device_path| and |mount_path| for testing.
  Disk CreateTestDisk(const std::string& device_path,
                      const std::string& mount_path) {
    return Disk(device_path,
                mount_path,
                "",
                "",
                "",
                "",
                "",
                "",
                "",
                "",
                "",
                device_path,
                chromeos::DEVICE_TYPE_UNKNOWN,
                0,
                false,
                false,
                false,
                false,
                false,
                false);
  }

  // Creates a volume info instance with |device_path| and |mount_path| for
  // testing.
  VolumeInfo CreateTestVolumeInfo(const std::string& device_path,
                                  const std::string& mount_path) {
    VolumeInfo volume_info;
    volume_info.system_path_prefix = base::FilePath(device_path);
    volume_info.mount_path = base::FilePath(mount_path);
    return volume_info;
  }

  scoped_ptr<DeviceEventRouterImpl> device_event_router;

 private:
  base::MessageLoop message_loop_;
};

TEST_F(DeviceEventRouterTest, AddAndRemoveDevice) {
  const Disk disk1 = CreateTestDisk("/device/test", "/mount/path1");
  const Disk disk1_unmounted = CreateTestDisk("/device/test", "");
  const VolumeInfo volumeInfo =
      CreateTestVolumeInfo("/device/test", "/mount/path1");
  device_event_router->OnDeviceAdded("/device/test");
  device_event_router->OnDiskAdded(disk1, true);
  device_event_router->OnVolumeMounted(chromeos::MOUNT_ERROR_NONE, volumeInfo);
  device_event_router->OnVolumeUnmounted(chromeos::MOUNT_ERROR_NONE,
                                         volumeInfo);
  device_event_router->OnDiskRemoved(disk1_unmounted);
  device_event_router->OnDeviceRemoved("/device/test");
  ASSERT_EQ(1u, device_event_router->events.size());
  EXPECT_EQ(file_manager_private::DEVICE_EVENT_TYPE_REMOVED,
            device_event_router->events[0].type);
  EXPECT_EQ("/device/test", device_event_router->events[0].device_path);
}

TEST_F(DeviceEventRouterTest, HardUnplugged) {
  const Disk disk1 = CreateTestDisk("/device/test", "/mount/path1");
  const Disk disk2 = CreateTestDisk("/device/test", "/mount/path2");
  device_event_router->OnDeviceAdded("/device/test");
  device_event_router->OnDiskAdded(disk1, true);
  device_event_router->OnDiskAdded(disk2, true);
  device_event_router->OnDiskRemoved(disk1);
  device_event_router->OnDiskRemoved(disk2);
  device_event_router->OnDeviceRemoved(kTestDevicePath);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, device_event_router->events.size());
  EXPECT_EQ(file_manager_private::DEVICE_EVENT_TYPE_HARD_UNPLUGGED,
            device_event_router->events[0].type);
  EXPECT_EQ("/device/test", device_event_router->events[0].device_path);
  EXPECT_EQ(file_manager_private::DEVICE_EVENT_TYPE_REMOVED,
            device_event_router->events[1].type);
  EXPECT_EQ("/device/test", device_event_router->events[1].device_path);
}

}  // namespace file_manager
