// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MediaTransferProtocolDeviceObserverLinux unit tests.

#include "chrome/browser/storage_monitor/media_transfer_protocol_device_observer_linux.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/storage_monitor/mock_removable_storage_observer.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/storage_monitor/test_storage_monitor.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

namespace {

// Sample mtp device storage information.
const char kStorageLabel[] = "Camera V1.0";
const char kStorageLocation[] = "/usb:2,2,88888";
const char kStorageUniqueId[] = "VendorModelSerial:COM:MOD2012:283";
const char kStorageWithInvalidInfo[] = "usb:2,3,11111";
const char kStorageWithValidInfo[] = "usb:2,2,88888";

// Returns the mtp device id given the |unique_id|.
std::string GetMtpDeviceId(const std::string& unique_id) {
  return MediaStorageUtil::MakeDeviceId(MediaStorageUtil::MTP_OR_PTP,
                                        unique_id);
}

// Helper function to get the device storage details such as device id, label
// and location. On success, fills in |id|, |label| and |location|.
void GetStorageInfo(const std::string& storage_name,
                    std::string* id,
                    string16* label,
                    std::string* location) {
  if (storage_name == kStorageWithInvalidInfo)
    return;  // Do not set any storage details.

  ASSERT_EQ(kStorageWithValidInfo, storage_name);

  *id = GetMtpDeviceId(kStorageUniqueId);
  *label = ASCIIToUTF16(kStorageLabel);
  *location = kStorageLocation;
}

class TestMediaTransferProtocolDeviceObserverLinux
    : public MediaTransferProtocolDeviceObserverLinux {
 public:
  TestMediaTransferProtocolDeviceObserverLinux(
      StorageMonitor::Receiver* receiver)
      : MediaTransferProtocolDeviceObserverLinux(receiver, &GetStorageInfo) {
  }

  // Notifies MediaTransferProtocolDeviceObserverLinux about the attachment of
  // mtp storage device given the |storage_name|.
  void MtpStorageAttached(const std::string& storage_name) {
    StorageChanged(true, storage_name);
    base::RunLoop().RunUntilIdle();
  }

  // Notifies MediaTransferProtocolDeviceObserverLinux about the detachment of
  // mtp storage device given the |storage_name|.
  void MtpStorageDetached(const std::string& storage_name) {
    StorageChanged(false, storage_name);
    base::RunLoop().RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMediaTransferProtocolDeviceObserverLinux);
};

}  // namespace

// A class to test the functionality of MediaTransferProtocolDeviceObserverLinux
// member functions.
class MediaTransferProtocolDeviceObserverLinuxTest : public testing::Test {
 public:
  MediaTransferProtocolDeviceObserverLinuxTest()
      : message_loop_(MessageLoop::TYPE_IO),
        file_thread_(content::BrowserThread::FILE, &message_loop_) {}

  virtual ~MediaTransferProtocolDeviceObserverLinuxTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    mock_storage_observer_.reset(new MockRemovableStorageObserver);
    mtp_device_observer_.reset(
        new TestMediaTransferProtocolDeviceObserverLinux(monitor_.receiver()));
    monitor_.AddObserver(mock_storage_observer_.get());
  }

  virtual void TearDown() OVERRIDE {
    monitor_.RemoveObserver(mock_storage_observer_.get());
    mtp_device_observer_.reset();
  }

  // Returns the device changed observer object.
  MockRemovableStorageObserver& observer() {
    return *mock_storage_observer_;
  }

  TestMediaTransferProtocolDeviceObserverLinux* mtp_device_observer() {
    return mtp_device_observer_.get();
  }

 private:
  MessageLoop message_loop_;
  content::TestBrowserThread file_thread_;

  chrome::test::TestStorageMonitor monitor_;
  scoped_ptr<TestMediaTransferProtocolDeviceObserverLinux> mtp_device_observer_;
  scoped_ptr<MockRemovableStorageObserver> mock_storage_observer_;

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDeviceObserverLinuxTest);
};

// Test to verify basic mtp storage attach and detach notifications.
TEST_F(MediaTransferProtocolDeviceObserverLinuxTest, BasicAttachDetach) {
  std::string device_id = GetMtpDeviceId(kStorageUniqueId);

  // Attach a mtp storage.
  mtp_device_observer()->MtpStorageAttached(kStorageWithValidInfo);

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(device_id, observer().last_attached().device_id);
  EXPECT_EQ(ASCIIToUTF16(kStorageLabel), observer().last_attached().name);
  EXPECT_EQ(kStorageLocation, observer().last_attached().location);

  // Detach the attached storage.
  mtp_device_observer()->MtpStorageDetached(kStorageWithValidInfo);

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(device_id, observer().last_detached().device_id);
}

// When a mtp storage device with invalid storage label and id is
// attached/detached, there should not be any device attach/detach
// notifications.
TEST_F(MediaTransferProtocolDeviceObserverLinuxTest, StorageWithInvalidInfo) {
  // Attach the mtp storage with invalid storage info.
  mtp_device_observer()->MtpStorageAttached(kStorageWithInvalidInfo);

  // Detach the attached storage.
  mtp_device_observer()->MtpStorageDetached(kStorageWithInvalidInfo);

  EXPECT_EQ(0, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
}

}  // namespace chrome
