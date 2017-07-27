// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_profile_pref_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/proximity_auth/proximity_auth_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {
namespace {

const char kBluetoothAddress1[] = "11:22:33:44:55:66";
const char kPublicKey1[] = "public key 1";

const char kBluetoothAddress2[] = "22:33:44:55:66:77";
const char kPublicKey2[] = "public key 2";

const int64_t kPasswordEntryTimestampMs1 = 123456789L;
const int64_t kPasswordEntryTimestampMs2 = 987654321L;

const int64_t kPromotionCheckTimestampMs1 = 1111111111L;
const int64_t kPromotionCheckTimestampMs2 = 2222222222L;

const ProximityAuthProfilePrefManager::ProximityThreshold kProximityThreshold1 =
    ProximityAuthProfilePrefManager::ProximityThreshold::kFar;
const ProximityAuthProfilePrefManager::ProximityThreshold kProximityThreshold2 =
    ProximityAuthProfilePrefManager::ProximityThreshold::kVeryFar;

}  //  namespace

class ProximityAuthProfilePrefManagerTest : public testing::Test {
 protected:
  ProximityAuthProfilePrefManagerTest() {}

  void SetUp() override {
    ProximityAuthProfilePrefManager::RegisterPrefs(pref_service_.registry());
  }

  void CheckRemoteBleDevice(const std::string& bluetooth_address,
                            const std::string& public_key,
                            ProximityAuthProfilePrefManager& pref_manager) {
    EXPECT_TRUE(pref_manager.HasDeviceWithAddress(bluetooth_address));
    EXPECT_EQ(pref_manager.GetDevicePublicKey(bluetooth_address), public_key);

    EXPECT_TRUE(pref_manager.HasDeviceWithPublicKey(public_key));
    EXPECT_EQ(pref_manager.GetDeviceAddress(public_key), bluetooth_address);
  }

  sync_preferences::TestingPrefServiceSyncable pref_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthProfilePrefManagerTest);
};

TEST_F(ProximityAuthProfilePrefManagerTest, RegisterPrefs) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  ProximityAuthProfilePrefManager::RegisterPrefs(pref_service.registry());
  EXPECT_TRUE(
      pref_service.FindPreference(prefs::kProximityAuthRemoteBleDevices));
}

TEST_F(ProximityAuthProfilePrefManagerTest, AddDevice) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckRemoteBleDevice(kBluetoothAddress1, kPublicKey1, pref_manager);
}

TEST_F(ProximityAuthProfilePrefManagerTest,
       AddDevice_TwoDevicesWithSameAddress) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckRemoteBleDevice(kBluetoothAddress1, kPublicKey1, pref_manager);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey2);
  CheckRemoteBleDevice(kBluetoothAddress1, kPublicKey2, pref_manager);

  EXPECT_FALSE(pref_manager.HasDeviceWithPublicKey(kPublicKey1));
}

TEST_F(ProximityAuthProfilePrefManagerTest,
       AddDevice_TwoDevicesWithSamePublicKey) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckRemoteBleDevice(kBluetoothAddress1, kPublicKey1, pref_manager);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress2, kPublicKey1);
  CheckRemoteBleDevice(kBluetoothAddress2, kPublicKey1, pref_manager);

  EXPECT_FALSE(pref_manager.HasDeviceWithAddress(kBluetoothAddress1));
}

TEST_F(ProximityAuthProfilePrefManagerTest, RemoveDeviceWithAddress) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckRemoteBleDevice(kBluetoothAddress1, kPublicKey1, pref_manager);

  ASSERT_TRUE(pref_manager.RemoveDeviceWithAddress(kBluetoothAddress1));
  EXPECT_FALSE(pref_manager.HasDeviceWithAddress(kBluetoothAddress1));
  EXPECT_FALSE(pref_manager.HasDeviceWithPublicKey(kPublicKey1));
}

TEST_F(ProximityAuthProfilePrefManagerTest,
       RemoveDeviceWithAddress_DeviceNotPresent) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckRemoteBleDevice(kBluetoothAddress1, kPublicKey1, pref_manager);

  ASSERT_FALSE(pref_manager.RemoveDeviceWithAddress(kBluetoothAddress2));
  EXPECT_TRUE(pref_manager.HasDeviceWithAddress(kBluetoothAddress1));
  EXPECT_TRUE(pref_manager.HasDeviceWithPublicKey(kPublicKey1));
}

TEST_F(ProximityAuthProfilePrefManagerTest, RemoveDeviceWithPublicKey) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckRemoteBleDevice(kBluetoothAddress1, kPublicKey1, pref_manager);

  ASSERT_TRUE(pref_manager.RemoveDeviceWithPublicKey(kPublicKey1));
  EXPECT_FALSE(pref_manager.HasDeviceWithAddress(kBluetoothAddress1));
  EXPECT_FALSE(pref_manager.HasDeviceWithPublicKey(kPublicKey1));
}

TEST_F(ProximityAuthProfilePrefManagerTest,
       RemoveDeviceWithPublicKey_DeviceNotPresent) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckRemoteBleDevice(kBluetoothAddress1, kPublicKey1, pref_manager);

  ASSERT_FALSE(pref_manager.RemoveDeviceWithPublicKey(kPublicKey2));
  EXPECT_TRUE(pref_manager.HasDeviceWithAddress(kBluetoothAddress1));
  EXPECT_TRUE(pref_manager.HasDeviceWithPublicKey(kPublicKey1));
}

TEST_F(ProximityAuthProfilePrefManagerTest, GetPublicKeys) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckRemoteBleDevice(kBluetoothAddress1, kPublicKey1, pref_manager);

  pref_manager.AddOrUpdateDevice(kBluetoothAddress2, kPublicKey2);
  CheckRemoteBleDevice(kBluetoothAddress2, kPublicKey2, pref_manager);

  std::vector<std::string> public_keys = pref_manager.GetPublicKeys();

  // Note: it's not guarantee that the order in |public_key| is the same as
  // insertion, so directly comparing vectors would not work.
  EXPECT_TRUE(public_keys.size() == 2);
  EXPECT_TRUE(std::find(public_keys.begin(), public_keys.end(), kPublicKey1) !=
              public_keys.end());
  EXPECT_TRUE(std::find(public_keys.begin(), public_keys.end(), kPublicKey2) !=
              public_keys.end());
}

TEST_F(ProximityAuthProfilePrefManagerTest, LastPasswordEntryTimestamp) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);
  EXPECT_EQ(0L, pref_manager.GetLastPasswordEntryTimestampMs());
  pref_manager.SetLastPasswordEntryTimestampMs(kPasswordEntryTimestampMs1);
  EXPECT_EQ(kPasswordEntryTimestampMs1,
            pref_manager.GetLastPasswordEntryTimestampMs());
  pref_manager.SetLastPasswordEntryTimestampMs(kPasswordEntryTimestampMs2);
  EXPECT_EQ(kPasswordEntryTimestampMs2,
            pref_manager.GetLastPasswordEntryTimestampMs());
}

TEST_F(ProximityAuthProfilePrefManagerTest, LastPromotionCheckTimestamp) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);
  EXPECT_EQ(0L, pref_manager.GetLastPromotionCheckTimestampMs());
  pref_manager.SetLastPromotionCheckTimestampMs(kPromotionCheckTimestampMs1);
  EXPECT_EQ(kPromotionCheckTimestampMs1,
            pref_manager.GetLastPromotionCheckTimestampMs());
  pref_manager.SetLastPromotionCheckTimestampMs(kPromotionCheckTimestampMs2);
  EXPECT_EQ(kPromotionCheckTimestampMs2,
            pref_manager.GetLastPromotionCheckTimestampMs());
}

TEST_F(ProximityAuthProfilePrefManagerTest, PromotionShownCount) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);
  EXPECT_EQ(0, pref_manager.GetPromotionShownCount());
  pref_manager.SetPromotionShownCount(1);
  EXPECT_EQ(1, pref_manager.GetPromotionShownCount());
  pref_manager.SetPromotionShownCount(2);
  EXPECT_EQ(2, pref_manager.GetPromotionShownCount());
}

TEST_F(ProximityAuthProfilePrefManagerTest, ProximityThreshold) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);
  EXPECT_EQ(1, pref_manager.GetProximityThreshold());
  pref_manager.SetProximityThreshold(kProximityThreshold1);
  EXPECT_EQ(kProximityThreshold1, pref_manager.GetProximityThreshold());
  pref_manager.SetProximityThreshold(kProximityThreshold2);
  EXPECT_EQ(kProximityThreshold2, pref_manager.GetProximityThreshold());
}

TEST_F(ProximityAuthProfilePrefManagerTest, IsChromeOSLoginEnabled) {
  ProximityAuthProfilePrefManager pref_manager(&pref_service_);
  EXPECT_TRUE(pref_manager.IsChromeOSLoginEnabled());

  pref_manager.SetIsChromeOSLoginEnabled(false);
  EXPECT_FALSE(pref_manager.IsChromeOSLoginEnabled());

  pref_manager.SetIsChromeOSLoginEnabled(true);
  EXPECT_TRUE(pref_manager.IsChromeOSLoginEnabled());
}

}  // namespace proximity_auth
