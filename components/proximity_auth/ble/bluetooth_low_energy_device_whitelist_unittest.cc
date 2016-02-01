// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_device_whitelist.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proximity_auth/ble/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {
namespace {

const char kBluetoothAddress1[] = "11:22:33:44:55:66";
const char kPublicKey1[] = "public key 1";

const char kBluetoothAddress2[] = "22:33:44:55:66:77";
const char kPublicKey2[] = "public key 2";

}  //  namespace

class ProximityAuthBluetoothLowEnergyDeviceWhitelistTest
    : public testing::Test {
 protected:
  ProximityAuthBluetoothLowEnergyDeviceWhitelistTest() {}

  void SetUp() override {
    BluetoothLowEnergyDeviceWhitelist::RegisterPrefs(pref_service_.registry());
  }

  void CheckWhitelistedDevice(
      const std::string& bluetooth_address,
      const std::string& public_key,
      BluetoothLowEnergyDeviceWhitelist& device_whitelist) {
    EXPECT_TRUE(device_whitelist.HasDeviceWithAddress(bluetooth_address));
    EXPECT_EQ(device_whitelist.GetDevicePublicKey(bluetooth_address),
              public_key);

    EXPECT_TRUE(device_whitelist.HasDeviceWithPublicKey(public_key));
    EXPECT_EQ(device_whitelist.GetDeviceAddress(public_key), bluetooth_address);
  }

  TestingPrefServiceSimple pref_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthBluetoothLowEnergyDeviceWhitelistTest);
};

TEST_F(ProximityAuthBluetoothLowEnergyDeviceWhitelistTest, RegisterPrefs) {
  TestingPrefServiceSimple pref_service;
  BluetoothLowEnergyDeviceWhitelist::RegisterPrefs(pref_service.registry());
  EXPECT_TRUE(
      pref_service.FindPreference(prefs::kBluetoothLowEnergyDeviceWhitelist));
}

TEST_F(ProximityAuthBluetoothLowEnergyDeviceWhitelistTest, AddDevice) {
  BluetoothLowEnergyDeviceWhitelist device_whitelist(&pref_service_);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckWhitelistedDevice(kBluetoothAddress1, kPublicKey1, device_whitelist);
}

TEST_F(ProximityAuthBluetoothLowEnergyDeviceWhitelistTest,
       AddDevice_TwoDevicesWithSameAddress) {
  BluetoothLowEnergyDeviceWhitelist device_whitelist(&pref_service_);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckWhitelistedDevice(kBluetoothAddress1, kPublicKey1, device_whitelist);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey2);
  CheckWhitelistedDevice(kBluetoothAddress1, kPublicKey2, device_whitelist);

  EXPECT_FALSE(device_whitelist.HasDeviceWithPublicKey(kPublicKey1));
}

TEST_F(ProximityAuthBluetoothLowEnergyDeviceWhitelistTest,
       AddDevice_TwoDevicesWithSamePublicKey) {
  BluetoothLowEnergyDeviceWhitelist device_whitelist(&pref_service_);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckWhitelistedDevice(kBluetoothAddress1, kPublicKey1, device_whitelist);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress2, kPublicKey1);
  CheckWhitelistedDevice(kBluetoothAddress2, kPublicKey1, device_whitelist);

  EXPECT_FALSE(device_whitelist.HasDeviceWithAddress(kBluetoothAddress1));
}

TEST_F(ProximityAuthBluetoothLowEnergyDeviceWhitelistTest,
       RemoveDeviceWithAddress) {
  BluetoothLowEnergyDeviceWhitelist device_whitelist(&pref_service_);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckWhitelistedDevice(kBluetoothAddress1, kPublicKey1, device_whitelist);

  ASSERT_TRUE(device_whitelist.RemoveDeviceWithAddress(kBluetoothAddress1));
  EXPECT_FALSE(device_whitelist.HasDeviceWithAddress(kBluetoothAddress1));
  EXPECT_FALSE(device_whitelist.HasDeviceWithPublicKey(kPublicKey1));
}

TEST_F(ProximityAuthBluetoothLowEnergyDeviceWhitelistTest,
       RemoveDeviceWithAddress_DeviceNotPresent) {
  BluetoothLowEnergyDeviceWhitelist device_whitelist(&pref_service_);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckWhitelistedDevice(kBluetoothAddress1, kPublicKey1, device_whitelist);

  ASSERT_FALSE(device_whitelist.RemoveDeviceWithAddress(kBluetoothAddress2));
  EXPECT_TRUE(device_whitelist.HasDeviceWithAddress(kBluetoothAddress1));
  EXPECT_TRUE(device_whitelist.HasDeviceWithPublicKey(kPublicKey1));
}

TEST_F(ProximityAuthBluetoothLowEnergyDeviceWhitelistTest,
       RemoveDeviceWithPublicKey) {
  BluetoothLowEnergyDeviceWhitelist device_whitelist(&pref_service_);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckWhitelistedDevice(kBluetoothAddress1, kPublicKey1, device_whitelist);

  ASSERT_TRUE(device_whitelist.RemoveDeviceWithPublicKey(kPublicKey1));
  EXPECT_FALSE(device_whitelist.HasDeviceWithAddress(kBluetoothAddress1));
  EXPECT_FALSE(device_whitelist.HasDeviceWithPublicKey(kPublicKey1));
}

TEST_F(ProximityAuthBluetoothLowEnergyDeviceWhitelistTest,
       RemoveDeviceWithPublicKey_DeviceNotPresent) {
  BluetoothLowEnergyDeviceWhitelist device_whitelist(&pref_service_);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckWhitelistedDevice(kBluetoothAddress1, kPublicKey1, device_whitelist);

  ASSERT_FALSE(device_whitelist.RemoveDeviceWithPublicKey(kPublicKey2));
  EXPECT_TRUE(device_whitelist.HasDeviceWithAddress(kBluetoothAddress1));
  EXPECT_TRUE(device_whitelist.HasDeviceWithPublicKey(kPublicKey1));
}

TEST_F(ProximityAuthBluetoothLowEnergyDeviceWhitelistTest, GetPublicKeys) {
  BluetoothLowEnergyDeviceWhitelist device_whitelist(&pref_service_);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress1, kPublicKey1);
  CheckWhitelistedDevice(kBluetoothAddress1, kPublicKey1, device_whitelist);

  device_whitelist.AddOrUpdateDevice(kBluetoothAddress2, kPublicKey2);
  CheckWhitelistedDevice(kBluetoothAddress2, kPublicKey2, device_whitelist);

  std::vector<std::string> public_keys = device_whitelist.GetPublicKeys();

  // Note: it's not guarantee that the order in |public_key| is the same as
  // insertion, so directly comparing vectors would not work.
  EXPECT_TRUE(public_keys.size() == 2);
  EXPECT_TRUE(std::find(public_keys.begin(), public_keys.end(), kPublicKey1) !=
              public_keys.end());
  EXPECT_TRUE(std::find(public_keys.begin(), public_keys.end(), kPublicKey2) !=
              public_keys.end());
}

}  // namespace proximity_auth
