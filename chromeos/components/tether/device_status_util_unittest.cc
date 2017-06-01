// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/device_status_util.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

const char kDoNotSetStringField[] = "doNotSetField";
const int kDoNotSetIntField = -100;

// Creates a DeviceStatus object using the parameters provided. If
// |kDoNotSetStringField| or |kDoNotSetIntField| are passed, these fields will
// not be set in the output.
DeviceStatus CreateFakeDeviceStatus(const std::string& cell_provider_name,
                                    int battery_percentage,
                                    int connection_strength) {
  // TODO(khorimoto): Once a ConnectedWifiSsid field is added as a property of
  // Tether networks, give an option to pass a parameter for that field as well.
  WifiStatus wifi_status;
  wifi_status.set_status_code(
      WifiStatus_StatusCode::WifiStatus_StatusCode_CONNECTED);
  wifi_status.set_ssid("Google A");

  DeviceStatus device_status;
  if (battery_percentage != kDoNotSetIntField) {
    device_status.set_battery_percentage(battery_percentage);
  }
  if (cell_provider_name != kDoNotSetStringField) {
    device_status.set_cell_provider(cell_provider_name);
  }
  if (connection_strength != kDoNotSetIntField) {
    device_status.set_connection_strength(connection_strength);
  }

  device_status.mutable_wifi_status()->CopyFrom(wifi_status);

  return device_status;
}

}  // namespace

class DeviceStatusUtilTest : public testing::Test {
 public:
  DeviceStatusUtilTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceStatusUtilTest);
};

TEST_F(DeviceStatusUtilTest, TestNotPresent) {
  DeviceStatus status =
      CreateFakeDeviceStatus(kDoNotSetStringField /* cell_provider_name */,
                             kDoNotSetIntField /* battery_percentage */,
                             kDoNotSetIntField /* connection_strength */);

  std::string carrier;
  int32_t battery_percentage;
  int32_t signal_strength;

  NormalizeDeviceStatus(status, &carrier, &battery_percentage,
                        &signal_strength);

  EXPECT_EQ("unknown-carrier", carrier);
  EXPECT_EQ(100, battery_percentage);
  EXPECT_EQ(100, signal_strength);
}

TEST_F(DeviceStatusUtilTest, TestEmptyCellProvider) {
  DeviceStatus status = CreateFakeDeviceStatus(
      "" /* cell_provider_name */, kDoNotSetIntField /* battery_percentage */,
      kDoNotSetIntField /* connection_strength */);

  std::string carrier;
  int32_t battery_percentage;
  int32_t signal_strength;

  NormalizeDeviceStatus(status, &carrier, &battery_percentage,
                        &signal_strength);

  EXPECT_EQ("unknown-carrier", carrier);
  EXPECT_EQ(100, battery_percentage);
  EXPECT_EQ(100, signal_strength);
}

TEST_F(DeviceStatusUtilTest, TestBelowMinValue) {
  DeviceStatus status = CreateFakeDeviceStatus(
      "cellProvider" /* cell_provider_name */, -1 /* battery_percentage */,
      -1 /* connection_strength */);

  std::string carrier;
  int32_t battery_percentage;
  int32_t signal_strength;

  NormalizeDeviceStatus(status, &carrier, &battery_percentage,
                        &signal_strength);

  EXPECT_EQ("cellProvider", carrier);
  EXPECT_EQ(0, battery_percentage);
  EXPECT_EQ(0, signal_strength);
}

TEST_F(DeviceStatusUtilTest, TestAboveMaxValue) {
  DeviceStatus status = CreateFakeDeviceStatus(
      "cellProvider" /* cell_provider_name */, 101 /* battery_percentage */,
      5 /* connection_strength */);

  std::string carrier;
  int32_t battery_percentage;
  int32_t signal_strength;

  NormalizeDeviceStatus(status, &carrier, &battery_percentage,
                        &signal_strength);

  EXPECT_EQ("cellProvider", carrier);
  EXPECT_EQ(100, battery_percentage);
  EXPECT_EQ(100, signal_strength);
}

TEST_F(DeviceStatusUtilTest, TestValidValues) {
  DeviceStatus status = CreateFakeDeviceStatus(
      "cellProvider" /* cell_provider_name */, 50 /* battery_percentage */,
      2 /* connection_strength */);

  std::string carrier;
  int32_t battery_percentage;
  int32_t signal_strength;

  NormalizeDeviceStatus(status, &carrier, &battery_percentage,
                        &signal_strength);

  EXPECT_EQ("cellProvider", carrier);
  EXPECT_EQ(50, battery_percentage);
  EXPECT_EQ(50, signal_strength);
}

}  // namespace tether

}  // namespace cryptauth
