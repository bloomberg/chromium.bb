// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <vector>

#include "base/guid.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/api/signedin_devices/signedin_devices_api.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::TestExtensionPrefs;
using browser_sync::DeviceInfo;
using testing::Return;

namespace extensions {

TEST(SignedinDevicesAPITest, GetSignedInDevices) {
  ProfileSyncServiceMock pss_mock;
  base::MessageLoop message_loop_;
  TestExtensionPrefs extension_prefs(
      message_loop_.message_loop_proxy().get());

  // Add a couple of devices and make sure we get back public ids for them.
  std::string extension_name = "test";
  scoped_refptr<Extension> extension_test =
      extension_prefs.AddExtension(extension_name);

  DeviceInfo device_info1(
      base::GenerateGUID(),
      "abc Device", "XYZ v1", "XYZ SyncAgent v1",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX);

  DeviceInfo device_info2(
      base::GenerateGUID(),
      "def Device", "XYZ v2", "XYZ SyncAgent v2",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX);

  std::vector<DeviceInfo*> devices;
  devices.push_back(&device_info1);
  devices.push_back(&device_info2);

  EXPECT_CALL(pss_mock, GetAllSignedInDevicesMock()).
              WillOnce(Return(&devices));

  ScopedVector<DeviceInfo> output1 = GetAllSignedInDevices(
      extension_test.get()->id(),
      &pss_mock,
      extension_prefs.prefs());

  std::string public_id1 = device_info1.public_id();
  std::string public_id2 = device_info2.public_id();

  EXPECT_FALSE(public_id1.empty());
  EXPECT_FALSE(public_id2.empty());
  EXPECT_NE(public_id1, public_id2);

  // Now clear output1 so its destructor will not destroy the pointers for
  // |device_info1| and |device_info2|.
  output1.weak_clear();

  // Add a third device and make sure the first 2 ids are retained and a new
  // id is generated for the third device.
  DeviceInfo device_info3(
      base::GenerateGUID(),
      "def Device", "jkl v2", "XYZ SyncAgent v2",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX);

  devices.push_back(&device_info3);

  EXPECT_CALL(pss_mock, GetAllSignedInDevicesMock()).
              WillOnce(Return(&devices));

  ScopedVector<DeviceInfo> output2 = GetAllSignedInDevices(
      extension_test.get()->id(),
      &pss_mock,
      extension_prefs.prefs());

  EXPECT_EQ(device_info1.public_id(), public_id1);
  EXPECT_EQ(device_info2.public_id(), public_id2);

  std::string public_id3 = device_info3.public_id();
  EXPECT_FALSE(public_id3.empty());
  EXPECT_NE(public_id3, public_id1);
  EXPECT_NE(public_id3, public_id2);

  // Now clear output2 so that its destructor does not destroy the
  // |DeviceInfo| pointers.
  output2.weak_clear();
}

}  // namespace extensions
