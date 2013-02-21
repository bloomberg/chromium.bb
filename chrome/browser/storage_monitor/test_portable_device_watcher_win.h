// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains a subclass of PortableDeviceWatcherWin to expose some
// functionality for testing.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_TEST_PORTABLE_DEVICE_WATCHER_WIN_H_
#define CHROME_BROWSER_STORAGE_MONITOR_TEST_PORTABLE_DEVICE_WATCHER_WIN_H_

#include <string>

#include "base/string16.h"
#include "chrome/browser/storage_monitor/portable_device_watcher_win.h"

namespace chrome {
namespace test {

class TestPortableDeviceWatcherWin : public PortableDeviceWatcherWin {
 public:
  // MTP device PnP identifiers.
  static const char16 kMTPDeviceWithMultipleStorages[];
  static const char16 kMTPDeviceWithInvalidInfo[];
  static const char16 kMTPDeviceWithValidInfo[];

  // MTP device storage unique identifier.
  static const char kStorageUniqueIdA[];

  TestPortableDeviceWatcherWin();
  virtual ~TestPortableDeviceWatcherWin();

  // Returns the persistent storage unique id of the device specified by the
  // |pnp_device_id|. |storage_object_id| specifies the string ID that uniquely
  // identifies the object on the device.
  static std::string GetMTPStorageUniqueId(const string16& pnp_device_id,
                                           const string16& storage_object_id);

  // Returns a list of storage object identifiers of the media transfer protocol
  // (MTP) device given a |pnp_device_id|.
  static PortableDeviceWatcherWin::StorageObjectIDs GetMTPStorageObjectIds(
      const string16& pnp_device_id);

  // Gets the media transfer protocol (MTP) device storage details given a
  // |pnp_device_id| and |storage_object_id|.
  static void GetMTPStorageDetails(const string16& pnp_device_id,
                                   const string16& storage_object_id,
                                   string16* device_location,
                                   std::string* unique_id,
                                   string16* name);

  // Returns a list of device storage details for the given device specified by
  // |pnp_device_id|.
  static PortableDeviceWatcherWin::StorageObjects GetDeviceStorageObjects(
      const string16& pnp_device_id);

  // Used by MediaFileSystemRegistry unit test.
  void set_use_dummy_mtp_storage_info(bool use_dummy_info) {
    use_dummy_mtp_storage_info_ = use_dummy_info;
  }

 private:
  // PortableDeviceWatcherWin:
  virtual void EnumerateAttachedDevices() OVERRIDE;
  virtual void HandleDeviceAttachEvent(const string16& pnp_device_id) OVERRIDE;
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const OVERRIDE;

  // Set to true to get dummy storage details from
  // GetMTPStorageInfoFromDeviceId().
  bool use_dummy_mtp_storage_info_;

  DISALLOW_COPY_AND_ASSIGN(TestPortableDeviceWatcherWin);
};

}  // namespace test
}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_TEST_PORTABLE_DEVICE_WATCHER_WIN_H_
