// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MediaTransferProtocolDeviceObserverCros unit tests.

#include "chrome/browser/system_monitor/media_transfer_protocol_device_observer_chromeos.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/mock_devices_changed_observer.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace mtp {

namespace {

// Sample mtp device storage information.
const char kStorageLabel[] = "Camera V1.0";
const char kStorageLocation[] = "/usb:2,2,88888";
const char kStorageName[] = "usb:2,2,88888";
const char kStorageUniqueId[] = "VendorModelSerial:COM:MOD2012:283";

// Returns the mtp device id given the |unique_id|.
std::string GetMtpDeviceId(const std::string& unique_id) {
  return chrome::MediaStorageUtil::MakeDeviceId(
      chrome::MediaStorageUtil::MTP_OR_PTP, unique_id);
}

// Helper function to get the device storage details such as device id, label
// and location. On success, fills in |id|, |label| and |location|.
void GetStorageInfo(const std::string& storage_name,
                    std::string* id,
                    string16* label,
                    std::string* location) {
  if (storage_name != kStorageName) {
    NOTREACHED();
    return;
  }
  if (id)
    *id = GetMtpDeviceId(kStorageUniqueId);
  if (label)
    *label = ASCIIToUTF16(kStorageLabel);
  if (location)
    *location = kStorageLocation;
}

}  // namespace

// A class to test the functionality of MediaTransferProtocolDeviceObserverCros
// member functions.
class MediaTransferProtocolDeviceObserverCrosTest
    : public testing::Test,
      public MediaTransferProtocolDeviceObserverCros {
 public:
  MediaTransferProtocolDeviceObserverCrosTest()
      : MediaTransferProtocolDeviceObserverCros(&GetStorageInfo) {
  }

  virtual ~MediaTransferProtocolDeviceObserverCrosTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    mock_devices_changed_observer_.reset(new base::MockDevicesChangedObserver);
    system_monitor_.AddDevicesChangedObserver(
        mock_devices_changed_observer_.get());
  }

  virtual void TearDown() OVERRIDE {
    system_monitor_.RemoveDevicesChangedObserver(
        mock_devices_changed_observer_.get());
  }

  // Returns the device changed observer object.
  base::MockDevicesChangedObserver& observer() {
    return *mock_devices_changed_observer_;
  }

  // Notifies MediaTransferProtocolDeviceObserverCros about the attachment of
  // mtp storage device given the |storage_name|.
  void MtpStorageAttached(const std::string& storage_name) {
    MediaTransferProtocolDeviceObserverCros::StorageChanged(true, storage_name);
    ui_loop_.RunAllPending();
  }

  // Notifies MediaTransferProtocolDeviceObserverCros about the detachment of
  // mtp storage device given the |storage_name|.
  void MtpStorageDetached(const std::string& storage_name) {
    MediaTransferProtocolDeviceObserverCros::StorageChanged(false,
                                                            storage_name);
    ui_loop_.RunAllPending();
  }

 private:
  // Message loop to run the test on.
  MessageLoop ui_loop_;

  // SystemMonitor and DevicesChangedObserver to hook together to test.
  base::SystemMonitor system_monitor_;
  scoped_ptr<base::MockDevicesChangedObserver> mock_devices_changed_observer_;

  DISALLOW_COPY_AND_ASSIGN(MediaTransferProtocolDeviceObserverCrosTest);
};

// Test to verify basic mtp storage attach and detach notifications.
TEST_F(MediaTransferProtocolDeviceObserverCrosTest, BasicAttachDetach) {
  testing::Sequence mock_sequence;
  std::string device_id = GetMtpDeviceId(kStorageUniqueId);

  EXPECT_CALL(observer(),
              OnRemovableStorageAttached(device_id,
                                         ASCIIToUTF16(kStorageLabel),
                                         kStorageLocation))
      .InSequence(mock_sequence);

  // Attach a mtp storage.
  MtpStorageAttached(kStorageName);

  EXPECT_CALL(observer(), OnRemovableStorageDetached(device_id))
      .InSequence(mock_sequence);

  // Detach the attached storage.
  MtpStorageDetached(kStorageName);
}

}  // namespace mtp
}  // namespace chromeos
