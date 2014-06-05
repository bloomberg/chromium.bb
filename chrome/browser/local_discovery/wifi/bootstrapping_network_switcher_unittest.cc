// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/local_discovery/wifi/bootstrapping_network_switcher.h"
#include "chrome/browser/local_discovery/wifi/mock_wifi_manager.h"
#include "components/onc/onc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace local_discovery {

namespace wifi {

namespace {

class MockableNetworkSwitchCallback {
 public:
  MOCK_METHOD1(OnNetworkSwitch, void(bool success));

  BootstrappingNetworkSwitcher::SuccessCallback callback() {
    return base::Bind(&MockableNetworkSwitchCallback::OnNetworkSwitch,
                      base::Unretained(this));
  }
};

class BootstrappingNetworkSwitcherTest : public ::testing::Test {
 public:
  BootstrappingNetworkSwitcherTest() {
    NetworkProperties network1;
    network1.guid = "SampleGUID1";
    network1.ssid = "SampleSSID1";
    network1.connection_state = onc::connection_state::kNotConnected;

    NetworkProperties network2;
    network2.guid = "SampleGUID2";
    network2.ssid = "SampleSSID2";
    network2.connection_state = onc::connection_state::kConnected;

    network_properties_.push_back(network1);
    network_properties_.push_back(network2);
  }

  ~BootstrappingNetworkSwitcherTest() {}

  NetworkPropertiesList network_properties_;
  StrictMock<MockableNetworkSwitchCallback> mockable_callback_;
  StrictMock<MockWifiManager> mock_wifi_manager_;
  scoped_ptr<BootstrappingNetworkSwitcher> network_switcher_;
};

TEST_F(BootstrappingNetworkSwitcherTest, EndToEndSuccess) {
  network_switcher_.reset(new BootstrappingNetworkSwitcher(
      &mock_wifi_manager_, "SampleConnectSSID", mockable_callback_.callback()));

  EXPECT_CALL(mock_wifi_manager_, GetSSIDListInternal());
  network_switcher_->Connect();

  EXPECT_CALL(mock_wifi_manager_,
              ConfigureAndConnectNetworkInternal("SampleConnectSSID", ""));
  mock_wifi_manager_.CallSSIDListCallback(network_properties_);

  EXPECT_CALL(mockable_callback_, OnNetworkSwitch(true));
  mock_wifi_manager_.CallConfigureAndConnectNetworkCallback(true);

  EXPECT_CALL(mock_wifi_manager_, ConnectToNetworkByIDInternal("SampleGUID2"));
  network_switcher_->Disconnect();
}

TEST_F(BootstrappingNetworkSwitcherTest, MidconnnectCancel) {
  network_switcher_.reset(new BootstrappingNetworkSwitcher(
      &mock_wifi_manager_, "SampleConnectSSID", mockable_callback_.callback()));

  EXPECT_CALL(mock_wifi_manager_, GetSSIDListInternal());
  network_switcher_->Connect();

  EXPECT_CALL(mock_wifi_manager_,
              ConfigureAndConnectNetworkInternal("SampleConnectSSID", ""));
  mock_wifi_manager_.CallSSIDListCallback(network_properties_);

  EXPECT_CALL(mock_wifi_manager_, ConnectToNetworkByIDInternal("SampleGUID2"));
  network_switcher_->Disconnect();
}

TEST_F(BootstrappingNetworkSwitcherTest, Failure) {
  network_switcher_.reset(new BootstrappingNetworkSwitcher(
      &mock_wifi_manager_, "SampleConnectSSID", mockable_callback_.callback()));

  EXPECT_CALL(mock_wifi_manager_, GetSSIDListInternal());
  network_switcher_->Connect();

  EXPECT_CALL(mock_wifi_manager_,
              ConfigureAndConnectNetworkInternal("SampleConnectSSID", ""));
  mock_wifi_manager_.CallSSIDListCallback(network_properties_);

  EXPECT_CALL(mockable_callback_, OnNetworkSwitch(false));
  mock_wifi_manager_.CallConfigureAndConnectNetworkCallback(false);

  network_switcher_->Disconnect();
}

TEST_F(BootstrappingNetworkSwitcherTest, RAII) {
  network_switcher_.reset(new BootstrappingNetworkSwitcher(
      &mock_wifi_manager_, "SampleConnectSSID", mockable_callback_.callback()));

  EXPECT_CALL(mock_wifi_manager_, GetSSIDListInternal());
  network_switcher_->Connect();

  EXPECT_CALL(mock_wifi_manager_,
              ConfigureAndConnectNetworkInternal("SampleConnectSSID", ""));
  mock_wifi_manager_.CallSSIDListCallback(network_properties_);

  EXPECT_CALL(mock_wifi_manager_, ConnectToNetworkByIDInternal("SampleGUID2"));
  network_switcher_.reset();
}

}  // namespace

}  // namespace wifi

}  // namespace local_discovery
