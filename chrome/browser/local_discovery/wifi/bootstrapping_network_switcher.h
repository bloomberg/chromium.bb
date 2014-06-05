// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_BOOTSTRAPPING_NETWORK_SWITCHER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_BOOTSTRAPPING_NETWORK_SWITCHER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/local_discovery/wifi/wifi_manager.h"

namespace local_discovery {

namespace wifi {

class BootstrappingNetworkSwitcher {
 public:
  typedef base::Callback<void(bool success)> SuccessCallback;

  BootstrappingNetworkSwitcher(WifiManager* wifi_manager,
                               const std::string& connect_ssid,
                               const SuccessCallback& callback);
  ~BootstrappingNetworkSwitcher();

  void Connect();

  void Disconnect();

 private:
  void OnGotNetworkList(const NetworkPropertiesList& networks);

  void OnConnectStatus(bool status);

  WifiManager* wifi_manager_;
  std::string connect_ssid_;
  SuccessCallback callback_;

  std::string return_guid_;
  bool connected_;

  base::WeakPtrFactory<BootstrappingNetworkSwitcher> weak_factory_;
};

}  // namespace wifi

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_WIFI_BOOTSTRAPPING_NETWORK_SWITCHER_H_
