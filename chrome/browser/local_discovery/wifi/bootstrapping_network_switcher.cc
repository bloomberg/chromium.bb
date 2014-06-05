// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/wifi/bootstrapping_network_switcher.h"

#include "base/bind.h"
#include "components/onc/onc_constants.h"

namespace local_discovery {

namespace wifi {

BootstrappingNetworkSwitcher::BootstrappingNetworkSwitcher(
    WifiManager* wifi_manager,
    const std::string& connect_ssid,
    const SuccessCallback& callback)
    : wifi_manager_(wifi_manager),
      connect_ssid_(connect_ssid),
      callback_(callback),
      connected_(false),
      weak_factory_(this) {
}

BootstrappingNetworkSwitcher::~BootstrappingNetworkSwitcher() {
  Disconnect();
}

void BootstrappingNetworkSwitcher::Connect() {
  wifi_manager_->GetSSIDList(
      base::Bind(&BootstrappingNetworkSwitcher::OnGotNetworkList,
                 weak_factory_.GetWeakPtr()));
}

void BootstrappingNetworkSwitcher::Disconnect() {
  if (connected_ && !return_guid_.empty()) {
    connected_ = false;
    wifi_manager_->ConnectToNetworkByID(return_guid_,
                                        WifiManager::SuccessCallback());
  }
}

void BootstrappingNetworkSwitcher::OnGotNetworkList(
    const NetworkPropertiesList& networks) {
  for (NetworkPropertiesList::const_iterator i = networks.begin();
       i != networks.end();
       i++) {
    if (i->connection_state == onc::connection_state::kConnected) {
      return_guid_ = i->guid;
      break;
    }
  }

  // We are "connected" while attempting to connect even if we fail
  connected_ = true;

  // Bootstrapping devices should have an empty PSK.
  wifi_manager_->ConfigureAndConnectNetwork(
      connect_ssid_,
      WifiCredentials::FromPSK(std::string()),
      base::Bind(&BootstrappingNetworkSwitcher::OnConnectStatus,
                 weak_factory_.GetWeakPtr()));
}

void BootstrappingNetworkSwitcher::OnConnectStatus(bool status) {
  connected_ = status;
  callback_.Run(status);
}

}  // namespace wifi

}  // namespace local_discovery
