// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/system_monitor/media_storage_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

namespace {

// Sample mtp device id and unique id.
const char kMtpDeviceId[] = "mtp:VendorModelSerial:ABC:1233:1237912873";
const char kUniqueId[] = "VendorModelSerial:ABC:1233:1237912873";

}  // namespace

typedef testing::Test MediaStorageUtilTest;

// Test to verify |MediaStorageUtil::MakeDeviceId| functionality using a sample
// mtp device unique id.
TEST_F(MediaStorageUtilTest, MakeMtpDeviceId) {
  std::string device_id =
      MediaStorageUtil::MakeDeviceId(MediaStorageUtil::MTP_OR_PTP, kUniqueId);
  ASSERT_EQ(kMtpDeviceId, device_id);
}

// Test to verify |MediaStorageUtil::CrackDeviceId| functionality using a sample
// mtp device id.
TEST_F(MediaStorageUtilTest, CrackMtpDeviceId) {
  MediaStorageUtil::Type type;
  std::string id;
  ASSERT_TRUE(MediaStorageUtil::CrackDeviceId(kMtpDeviceId, &type, &id));
  ASSERT_EQ(kUniqueId, id);
  ASSERT_EQ(MediaStorageUtil::MTP_OR_PTP, type);
}

}  // namespace chrome
