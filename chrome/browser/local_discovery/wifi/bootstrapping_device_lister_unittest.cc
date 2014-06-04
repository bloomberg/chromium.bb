// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/local_discovery/wifi/bootstrapping_device_lister.h"
#include "chrome/browser/local_discovery/wifi/mock_wifi_manager.h"
#include "components/onc/onc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;
using testing::Mock;

namespace local_discovery {

namespace wifi {

namespace {

class MockableUpdateCallback {
 public:
  void UpdateCallback(bool available,
                      const BootstrappingDeviceDescription& description) {
    UpdateCallbackInternal(available,
                           description.device_network_id,
                           description.device_ssid,
                           description.device_name,
                           description.device_kind,
                           description.connection_status);
  }

  MOCK_METHOD6(UpdateCallbackInternal,
               void(bool available,
                    const std::string& network_id,
                    const std::string& ssid,
                    const std::string& name,
                    const std::string& kind,
                    BootstrappingDeviceDescription::ConnectionStatus status));

  BootstrappingDeviceLister::UpdateCallback callback() {
    return base::Bind(&MockableUpdateCallback::UpdateCallback,
                      base::Unretained(this));
  }
};

class BootstrappingDeviceListerTest : public ::testing::Test {
 public:
  BootstrappingDeviceListerTest()
      : lister_(&mock_wifi_manager_, mockable_callback_.callback()) {}

  ~BootstrappingDeviceListerTest() {}

  StrictMock<MockableUpdateCallback> mockable_callback_;
  StrictMock<MockWifiManager> mock_wifi_manager_;
  BootstrappingDeviceLister lister_;
};

TEST_F(BootstrappingDeviceListerTest, ListSingleDevice) {
  EXPECT_CALL(mock_wifi_manager_, GetSSIDListInternal());
  lister_.Start();
  Mock::VerifyAndClearExpectations(&mock_wifi_manager_);

  std::vector<NetworkProperties> network_property_list;

  NetworkProperties network;
  network.guid = "MyInternalID";
  network.ssid = "MyDevice@camNprv";
  network.connection_state = onc::connection_state::kNotConnected;

  network_property_list.push_back(network);

  NetworkProperties network2;
  network2.guid = "MyInternalID2";
  network2.ssid = "SomeRandomNetwork";
  network2.connection_state = onc::connection_state::kNotConnected;

  network_property_list.push_back(network2);

  EXPECT_CALL(
      mockable_callback_,
      UpdateCallbackInternal(true,
                             "MyInternalID",
                             "MyDevice@camNprv",
                             "MyDevice",
                             "camera",
                             BootstrappingDeviceDescription::NOT_CONFIGURED));
  mock_wifi_manager_.CallSSIDListCallback(network_property_list);
}

TEST_F(BootstrappingDeviceListerTest, AddRemoveDevice) {
  EXPECT_CALL(mock_wifi_manager_, GetSSIDListInternal());
  lister_.Start();
  Mock::VerifyAndClearExpectations(&mock_wifi_manager_);

  std::vector<NetworkProperties> network_property_list;

  NetworkProperties network;
  network.guid = "MyInternalID";
  network.ssid = "MyDevice@camNprv";
  network.connection_state = onc::connection_state::kNotConnected;

  network_property_list.push_back(network);

  std::vector<NetworkProperties> network_property_list2;

  NetworkProperties network2;
  network2.guid = "MyInternalID2";
  network2.ssid = "MyDevice2@priFprv";
  network2.connection_state = onc::connection_state::kNotConnected;

  network_property_list2.push_back(network2);

  EXPECT_CALL(
      mockable_callback_,
      UpdateCallbackInternal(true,
                             "MyInternalID",
                             "MyDevice@camNprv",
                             "MyDevice",
                             "camera",
                             BootstrappingDeviceDescription::NOT_CONFIGURED));
  mock_wifi_manager_.CallSSIDListCallback(network_property_list);

  Mock::VerifyAndClearExpectations(&mock_wifi_manager_);

  EXPECT_CALL(
      mockable_callback_,
      UpdateCallbackInternal(false,
                             "MyInternalID",
                             "MyDevice@camNprv",
                             "MyDevice",
                             "camera",
                             BootstrappingDeviceDescription::NOT_CONFIGURED));
  EXPECT_CALL(mockable_callback_,
              UpdateCallbackInternal(true,
                                     "MyInternalID2",
                                     "MyDevice2@priFprv",
                                     "MyDevice2",
                                     "printer",
                                     BootstrappingDeviceDescription::OFFLINE));
  mock_wifi_manager_.CallNetworkListObservers(network_property_list2);
}

TEST_F(BootstrappingDeviceListerTest, EdgeCases) {
  EXPECT_CALL(mock_wifi_manager_, GetSSIDListInternal());
  lister_.Start();
  Mock::VerifyAndClearExpectations(&mock_wifi_manager_);

  std::vector<NetworkProperties> network_property_list;

  NetworkProperties network;
  network.guid = "MyInternalID";
  network.ssid = "MyDevice@camprv";
  network.connection_state = onc::connection_state::kNotConnected;

  NetworkProperties network2;
  network2.guid = "MyInternalID2";
  network2.ssid = "MyDevice2@unkNprv";
  network2.connection_state = onc::connection_state::kNotConnected;

  NetworkProperties network3;
  network3.guid = "MyInternalID3";
  network3.ssid = "MyDevice3camNprv";
  network3.connection_state = onc::connection_state::kNotConnected;

  NetworkProperties network4;
  network4.guid = "MyInternalID4";
  network4.ssid = "MyDevice4@camNnpr";
  network4.connection_state = onc::connection_state::kNotConnected;

  NetworkProperties network5;
  network5.guid = "MyInternalID5";
  network5.ssid = "MyDevice5@With@At@Signs@camOprv";
  network5.connection_state = onc::connection_state::kNotConnected;

  network_property_list.push_back(network);
  network_property_list.push_back(network2);
  network_property_list.push_back(network3);
  network_property_list.push_back(network4);
  network_property_list.push_back(network5);

  EXPECT_CALL(
      mockable_callback_,
      UpdateCallbackInternal(true,
                             "MyInternalID2",
                             "MyDevice2@unkNprv",
                             "MyDevice2",
                             "device",
                             BootstrappingDeviceDescription::NOT_CONFIGURED));

  EXPECT_CALL(mockable_callback_,
              UpdateCallbackInternal(true,
                                     "MyInternalID5",
                                     "MyDevice5@With@At@Signs@camOprv",
                                     "MyDevice5@With@At@Signs",
                                     "camera",
                                     BootstrappingDeviceDescription::ONLINE));
  mock_wifi_manager_.CallSSIDListCallback(network_property_list);
}

}  // namespace

}  // namespace wifi

}  // namespace local_discovery
