// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MediaTransferProtocolDeviceObserverChromeOS unit tests.

#include "components/storage_monitor/media_transfer_protocol_device_observer_chromeos.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/mock_removable_storage_observer.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage_monitor {

namespace {

// Sample mtp device storage information.
const char kStorageWithInvalidInfo[] = "usb:2,3:11111";
const char kStorageWithValidInfo[] = "usb:2,2:88888";
const char kStorageVendor[] = "ExampleVendor";
const uint32_t kStorageVendorId = 0x040a;
const char kStorageProduct[] = "ExampleCamera";
const uint32_t kStorageProductId = 0x0160;
const uint32_t kStorageDeviceFlags = 0x0004000;
const uint32_t kStorageType = 3;                         // Fixed RAM
const uint32_t kStorageFilesystemType = 2;               // Generic Hierarchical
const uint32_t kStorageAccessCapability = 0;             // Read-Write
const uint64_t kStorageMaxCapacity = 0x40000000;         // 1G in total
const uint64_t kStorageFreeSpaceInBytes = 0x20000000;    // 512M bytes left
const uint64_t kStorageFreeSpaceInObjects = 0x04000000;  // 64M Objects left
const char kStorageDescription[] = "ExampleDescription";
const char kStorageVolumeIdentifier[] = "ExampleVolumeId";

std::map<std::string, device::mojom::MtpStorageInfo> fake_storage_info_map;

// Helper function to get fake MTP device details.
const device::mojom::MtpStorageInfo* GetFakeMtpStorageInfo(
    const std::string& storage_name) {
  // Fill the map out if it is empty.
  if (fake_storage_info_map.empty()) {
    // Add the invalid MTP storage info.
    auto storage_info = device::mojom::MtpStorageInfo();
    storage_info.storage_name = kStorageWithInvalidInfo;
    fake_storage_info_map.insert(
        std::make_pair(kStorageWithInvalidInfo, storage_info));
    // Add the valid MTP storage info.
    fake_storage_info_map.insert(std::make_pair(
        kStorageWithValidInfo,
        device::mojom::MtpStorageInfo(
            kStorageWithValidInfo, kStorageVendor, kStorageVendorId,
            kStorageProduct, kStorageProductId, kStorageDeviceFlags,
            kStorageType, kStorageFilesystemType, kStorageAccessCapability,
            kStorageMaxCapacity, kStorageFreeSpaceInBytes,
            kStorageFreeSpaceInObjects, kStorageDescription,
            kStorageVolumeIdentifier)));
  }

  const auto it = fake_storage_info_map.find(storage_name);
  return it != fake_storage_info_map.end() ? &it->second : nullptr;
}

class TestMediaTransferProtocolDeviceObserverChromeOS
    : public MediaTransferProtocolDeviceObserverChromeOS {
 public:
  TestMediaTransferProtocolDeviceObserverChromeOS(
      StorageMonitor::Receiver* receiver,
      device::MediaTransferProtocolManager* mtp_manager)
      : MediaTransferProtocolDeviceObserverChromeOS(
            receiver,
            mtp_manager,
            base::BindRepeating(&GetFakeMtpStorageInfo)) {}

  // Notifies MediaTransferProtocolDeviceObserverChromeOS about the attachment
  // of
  // mtp storage device given the |storage_name|.
  void MtpStorageAttached(const std::string& storage_name) {
    StorageChanged(true, storage_name);
    base::RunLoop().RunUntilIdle();
  }

  // Notifies MediaTransferProtocolDeviceObserverChromeOS about the detachment
  // of
  // mtp storage device given the |storage_name|.
  void MtpStorageDetached(const std::string& storage_name) {
    StorageChanged(false, storage_name);
    base::RunLoop().RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMediaTransferProtocolDeviceObserverChromeOS);
};

}  // namespace

// A class to test the functionality of
// MediaTransferProtocolDeviceObserverChromeOS member functions.
class MediaTransferProtocolDeviceObserverChromeOSTest : public testing::Test {
 public:
  MediaTransferProtocolDeviceObserverChromeOSTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  ~MediaTransferProtocolDeviceObserverChromeOSTest() override {}

 protected:
  void SetUp() override {
    mock_storage_observer_.reset(new MockRemovableStorageObserver);
    TestStorageMonitor* monitor = TestStorageMonitor::CreateAndInstall();
    mtp_device_observer_.reset(
        new TestMediaTransferProtocolDeviceObserverChromeOS(
            monitor->receiver(), monitor->media_transfer_protocol_manager()));
    monitor->AddObserver(mock_storage_observer_.get());
  }

  void TearDown() override {
    StorageMonitor* monitor = StorageMonitor::GetInstance();
    monitor->RemoveObserver(mock_storage_observer_.get());
    mtp_device_observer_.reset();
    TestStorageMonitor::Destroy();
  }

  // Returns the device changed observer object.
  MockRemovableStorageObserver& observer() { return *mock_storage_observer_; }

  TestMediaTransferProtocolDeviceObserverChromeOS* mtp_device_observer() {
    return mtp_device_observer_.get();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<TestMediaTransferProtocolDeviceObserverChromeOS>
      mtp_device_observer_;
  std::unique_ptr<MockRemovableStorageObserver> mock_storage_observer_;

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDeviceObserverChromeOSTest);
};

// Test to verify basic mtp storage attach and detach notifications.
TEST_F(MediaTransferProtocolDeviceObserverChromeOSTest, BasicAttachDetach) {

  // Attach a mtp storage.
  mtp_device_observer()->MtpStorageAttached(kStorageWithValidInfo);

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(base::ASCIIToUTF16(kStorageVendor),
            observer().last_attached().vendor_name());
  EXPECT_EQ(base::ASCIIToUTF16(kStorageProduct),
            observer().last_attached().model_name());

  // Detach the attached storage.
  mtp_device_observer()->MtpStorageDetached(kStorageWithValidInfo);

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
}

// When a mtp storage device with invalid storage label and id is
// attached/detached, there should not be any device attach/detach
// notifications.
TEST_F(MediaTransferProtocolDeviceObserverChromeOSTest,
       StorageWithInvalidInfo) {
  // Attach the mtp storage with invalid storage info.
  mtp_device_observer()->MtpStorageAttached(kStorageWithInvalidInfo);

  // Detach the attached storage.
  mtp_device_observer()->MtpStorageDetached(kStorageWithInvalidInfo);

  EXPECT_EQ(0, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
}

}  // namespace storage_monitor
