// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/geolocation_handler.h"

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class GeolocationHandlerTest : public testing::Test {
 public:
  GeolocationHandlerTest() : manager_test_(NULL) {
  }

  ~GeolocationHandlerTest() override {}

  void SetUp() override {
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::Initialize();
    // Get the test interface for manager / device.
    manager_test_ =
        DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface();
    ASSERT_TRUE(manager_test_);
    geolocation_handler_.reset(new GeolocationHandler());
    geolocation_handler_->Init();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    geolocation_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  bool GetWifiAccessPoints() {
    return geolocation_handler_->GetWifiAccessPoints(&wifi_access_points_,
                                                     nullptr);
  }

  bool GetCellTowers() {
    return geolocation_handler_->GetNetworkInformation(nullptr, &cell_towers_);
  }

  // This should remain in sync with the format of shill (chromeos) dict entries
  void AddAccessPoint(int idx) {
    base::DictionaryValue properties;
    std::string mac_address =
        base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
                           idx, 0, 0, 0, 0, 0);
    std::string channel = base::IntToString(idx);
    std::string strength = base::IntToString(idx * 10);
    properties.SetStringWithoutPathExpansion(shill::kGeoMacAddressProperty,
                                             mac_address);
    properties.SetStringWithoutPathExpansion(shill::kGeoChannelProperty,
                                             channel);
    properties.SetStringWithoutPathExpansion(shill::kGeoSignalStrengthProperty,
                                             strength);
    manager_test_->AddGeoNetwork(shill::kGeoWifiAccessPointsProperty,
                                 properties);
    base::RunLoop().RunUntilIdle();
  }

  // This should remain in sync with the format of shill (chromeos) dict entries
  void AddCellTower(int idx) {
    base::DictionaryValue properties;
    // Multiplications are intended solely to differentiate the various fields
    // in a predictable way, while preserving 3 digits for MCC and MNC.
    std::string ci = base::IntToString(idx);
    std::string lac = base::IntToString(idx * 10);
    std::string mcc = base::IntToString(idx * 100);
    std::string mnc = base::IntToString(idx * 100 + 1);

    properties.SetStringWithoutPathExpansion(shill::kGeoCellIdProperty, ci);
    properties.SetStringWithoutPathExpansion(
        shill::kGeoLocationAreaCodeProperty, lac);
    properties.SetStringWithoutPathExpansion(
        shill::kGeoMobileCountryCodeProperty, mcc);
    properties.SetStringWithoutPathExpansion(
        shill::kGeoMobileNetworkCodeProperty, mnc);

    manager_test_->AddGeoNetwork(shill::kGeoCellTowersProperty, properties);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  std::unique_ptr<GeolocationHandler> geolocation_handler_;
  ShillManagerClient::TestInterface* manager_test_;
  WifiAccessPointVector wifi_access_points_;
  CellTowerVector cell_towers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GeolocationHandlerTest);
};

TEST_F(GeolocationHandlerTest, NoAccessPoints) {
  // Inititial call should return false.
  EXPECT_FALSE(GetWifiAccessPoints());
  EXPECT_FALSE(GetCellTowers());
  base::RunLoop().RunUntilIdle();
  // Second call should return false since there are no devices.
  EXPECT_FALSE(GetWifiAccessPoints());
  EXPECT_FALSE(GetCellTowers());
}

TEST_F(GeolocationHandlerTest, OneAccessPoint) {
  // Add an access point.
  AddAccessPoint(1);
  base::RunLoop().RunUntilIdle();
  // Inititial call should return false and request access points.
  EXPECT_FALSE(GetWifiAccessPoints());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have an access point.
  EXPECT_TRUE(GetWifiAccessPoints());
  ASSERT_EQ(1u, wifi_access_points_.size());
  EXPECT_EQ("01:00:00:00:00:00", wifi_access_points_[0].mac_address);
  EXPECT_EQ(1, wifi_access_points_[0].channel);
}

TEST_F(GeolocationHandlerTest, MultipleAccessPoints) {
  // Add several access points.
  AddAccessPoint(1);
  AddAccessPoint(2);
  AddAccessPoint(3);
  base::RunLoop().RunUntilIdle();
  // Inititial call should return false and request access points.
  EXPECT_FALSE(GetWifiAccessPoints());
  EXPECT_FALSE(GetCellTowers());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have an access point.
  EXPECT_TRUE(GetWifiAccessPoints());
  ASSERT_EQ(3u, wifi_access_points_.size());
  EXPECT_EQ("02:00:00:00:00:00", wifi_access_points_[1].mac_address);
  EXPECT_EQ(3, wifi_access_points_[2].channel);
}

TEST_F(GeolocationHandlerTest, OneCellTower) {
  // Add a cell tower.
  AddCellTower(1);
  base::RunLoop().RunUntilIdle();
  // Inititial call should return false and request towers.
  EXPECT_FALSE(GetCellTowers());
  EXPECT_FALSE(GetWifiAccessPoints());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have a cell tower.
  EXPECT_TRUE(GetCellTowers());
  EXPECT_FALSE(GetWifiAccessPoints());
  ASSERT_EQ(1u, cell_towers_.size());
  EXPECT_EQ("1", cell_towers_[0].ci);
  EXPECT_EQ("10", cell_towers_[0].lac);
  EXPECT_EQ("100", cell_towers_[0].mcc);
  EXPECT_EQ("101", cell_towers_[0].mnc);
}

TEST_F(GeolocationHandlerTest, MultipleCellTowers) {
  // Add several cell towers.
  AddCellTower(1);
  AddCellTower(2);
  AddCellTower(3);
  base::RunLoop().RunUntilIdle();
  // Inititial call should return false and request cell towers.
  EXPECT_FALSE(GetWifiAccessPoints());
  EXPECT_FALSE(GetCellTowers());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have a cell tower.
  EXPECT_FALSE(GetWifiAccessPoints());
  EXPECT_TRUE(GetCellTowers());
  ASSERT_EQ(3u, cell_towers_.size());
  EXPECT_EQ("20", cell_towers_[1].lac);
  EXPECT_EQ("301", cell_towers_[2].mnc);
}

TEST_F(GeolocationHandlerTest, MultipleGeolocations) {
  // Add both a cell tower and wifi AP.
  AddCellTower(1);
  AddCellTower(2);
  AddAccessPoint(1);
  AddAccessPoint(2);
  base::RunLoop().RunUntilIdle();
  // Inititial call should return false and request towers.
  EXPECT_FALSE(GetCellTowers());
  EXPECT_FALSE(GetWifiAccessPoints());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have a cell tower.
  EXPECT_TRUE(GetCellTowers());
  EXPECT_TRUE(GetWifiAccessPoints());
  ASSERT_EQ(2u, wifi_access_points_.size());
  EXPECT_EQ("02:00:00:00:00:00", wifi_access_points_[1].mac_address);
  EXPECT_EQ(1, wifi_access_points_[0].channel);

  ASSERT_EQ(2u, cell_towers_.size());
  EXPECT_EQ("2", cell_towers_[1].ci);
  EXPECT_EQ("10", cell_towers_[0].lac);
  EXPECT_EQ("200", cell_towers_[1].mcc);
  EXPECT_EQ("101", cell_towers_[0].mnc);
}

}  // namespace chromeos
