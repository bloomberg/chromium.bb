// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_wifi_hotspot_connector.h"

#include "base/memory/ptr_util.h"
#include "chromeos/network/network_state_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

// static
std::unique_ptr<FakeWifiHotspotConnector> FakeWifiHotspotConnector::Create() {
  return base::WrapUnique(
      new FakeWifiHotspotConnector(NetworkStateHandler::InitializeForTest()));
}

FakeWifiHotspotConnector::FakeWifiHotspotConnector(
    std::unique_ptr<NetworkStateHandler> network_state_handler)
    : WifiHotspotConnector(network_state_handler.get(), nullptr),
      network_state_handler_(std::move(network_state_handler)) {}

FakeWifiHotspotConnector::~FakeWifiHotspotConnector() {}

void FakeWifiHotspotConnector::CallMostRecentCallback(
    const std::string& wifi_guid) {
  EXPECT_FALSE(most_recent_callback_.is_null());
  most_recent_callback_.Run(wifi_guid);
}

void FakeWifiHotspotConnector::ConnectToWifiHotspot(
    const std::string& ssid,
    const std::string& password,
    const WifiHotspotConnector::WifiConnectionCallback& callback) {
  most_recent_ssid_ = ssid;
  most_recent_password_ = password;
  most_recent_callback_ = callback;
}

}  // namespace tether

}  // namespace chromeos
