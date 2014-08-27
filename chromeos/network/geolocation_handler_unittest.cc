// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/network/geolocation_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class GeolocationHandlerTest : public testing::Test {
 public:
  GeolocationHandlerTest() : manager_test_(NULL) {
  }

  virtual ~GeolocationHandlerTest() {
  }

  virtual void SetUp() OVERRIDE {
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::Initialize();
    // Get the test interface for manager / device.
    manager_test_ =
        DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface();
    ASSERT_TRUE(manager_test_);
    geolocation_handler_.reset(new GeolocationHandler());
    geolocation_handler_->Init();
    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    geolocation_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  bool GetWifiAccessPoints() {
    return geolocation_handler_->GetWifiAccessPoints(
        &wifi_access_points_, NULL);
  }

  void AddAccessPoint(int idx) {
    base::DictionaryValue properties;
    std::string mac_address =
        base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
                           idx, 0, 0, 0, 0, 0);
    std::string channel = base::StringPrintf("%d", idx);
    std::string strength = base::StringPrintf("%d", idx * 10);
    properties.SetStringWithoutPathExpansion(
        shill::kGeoMacAddressProperty, mac_address);
    properties.SetStringWithoutPathExpansion(
        shill::kGeoChannelProperty, channel);
    properties.SetStringWithoutPathExpansion(
        shill::kGeoSignalStrengthProperty, strength);
    manager_test_->AddGeoNetwork(shill::kTypeWifi, properties);
    message_loop_.RunUntilIdle();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  scoped_ptr<GeolocationHandler> geolocation_handler_;
  ShillManagerClient::TestInterface* manager_test_;
  WifiAccessPointVector wifi_access_points_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GeolocationHandlerTest);
};

TEST_F(GeolocationHandlerTest, NoAccessPoints) {
  // Inititial call should return false.
  EXPECT_FALSE(GetWifiAccessPoints());
  message_loop_.RunUntilIdle();
  // Second call should return false since there are no devices.
  EXPECT_FALSE(GetWifiAccessPoints());
}

TEST_F(GeolocationHandlerTest, OneAccessPoint) {
  // Add an acces point.
  AddAccessPoint(1);
  message_loop_.RunUntilIdle();
  // Inititial call should return false and request access points.
  EXPECT_FALSE(GetWifiAccessPoints());
  message_loop_.RunUntilIdle();
  // Second call should return true since we have an access point.
  EXPECT_TRUE(GetWifiAccessPoints());
  ASSERT_EQ(1u, wifi_access_points_.size());
  EXPECT_EQ("01:00:00:00:00:00", wifi_access_points_[0].mac_address);
  EXPECT_EQ(1, wifi_access_points_[0].channel);
}

TEST_F(GeolocationHandlerTest, MultipleAccessPoints) {
  // Add several acces points.
  AddAccessPoint(1);
  AddAccessPoint(2);
  AddAccessPoint(3);
  message_loop_.RunUntilIdle();
  // Inititial call should return false and request access points.
  EXPECT_FALSE(GetWifiAccessPoints());
  message_loop_.RunUntilIdle();
  // Second call should return true since we have an access point.
  EXPECT_TRUE(GetWifiAccessPoints());
  ASSERT_EQ(3u, wifi_access_points_.size());
  EXPECT_EQ("02:00:00:00:00:00", wifi_access_points_[1].mac_address);
  EXPECT_EQ(3, wifi_access_points_[2].channel);
}

}  // namespace chromeos
