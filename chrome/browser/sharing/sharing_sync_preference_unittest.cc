// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_sync_preference.h"

#include <memory>

#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kVapidKeyStr[] = "test_vapid_key";
const std::vector<uint8_t> kVapidKey =
    std::vector<uint8_t>(std::begin(kVapidKeyStr), std::end(kVapidKeyStr));

const char kDeviceGuid[] = "test_device";
const char kDeviceFcmToken[] = "test_fcm_token";
const char kDeviceAuthToken[] = "test_auth_token";
const char kDeviceP256dh[] = "test_p256dh";
const int kCapabilities = 1;

class SharingSyncPreferenceTest : public testing::Test {
 protected:
  SharingSyncPreferenceTest() : sharing_sync_preference_(&prefs_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  void SyncDefaultDevice() {
    sharing_sync_preference_.SetSyncDevice(
        kDeviceGuid,
        SharingSyncPreference::Device(kDeviceFcmToken, kDeviceP256dh,
                                      kDeviceAuthToken, kCapabilities));
  }

  base::Optional<SharingSyncPreference::Device> GetDevice(
      const std::string& guid) {
    std::map<std::string, SharingSyncPreference::Device> synced_devices =
        sharing_sync_preference_.GetSyncedDevices();
    auto it = synced_devices.find(guid);
    if (it == synced_devices.end())
      return base::nullopt;
    else
      return std::move(it->second);
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  SharingSyncPreference sharing_sync_preference_;
};

}  // namespace

TEST_F(SharingSyncPreferenceTest, UpdateVapidKeys) {
  EXPECT_EQ(base::nullopt, sharing_sync_preference_.GetVapidKey());
  sharing_sync_preference_.SetVapidKey(kVapidKey);
  EXPECT_EQ(kVapidKey, sharing_sync_preference_.GetVapidKey());
}

TEST_F(SharingSyncPreferenceTest, RemoveDevice) {
  SyncDefaultDevice();
  EXPECT_NE(base::nullopt, GetDevice(kDeviceGuid));
  sharing_sync_preference_.RemoveDevice(kDeviceGuid);
  EXPECT_EQ(base::nullopt, GetDevice(kDeviceGuid));
}

TEST_F(SharingSyncPreferenceTest, SyncDevice) {
  EXPECT_EQ(base::nullopt, GetDevice(kDeviceGuid));
  SyncDefaultDevice();
  base::Optional<SharingSyncPreference::Device> device = GetDevice(kDeviceGuid);

  EXPECT_NE(base::nullopt, device);
  EXPECT_EQ(kDeviceFcmToken, device->fcm_token);
  EXPECT_EQ(kDeviceP256dh, device->p256dh);
  EXPECT_EQ(kDeviceAuthToken, device->auth_secret);
  EXPECT_EQ(kCapabilities, device->capabilities);
}
