// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv2_setup_flow.h"

#include "base/logging.h"

namespace local_discovery {

scoped_ptr<PrivetV2SetupFlow> PrivetV2SetupFlow::CreateMDnsOnlyFlow(
    ServiceDiscoveryClient* service_discovery_client,
    const std::string& service_name) {
  NOTIMPLEMENTED();
  return scoped_ptr<PrivetV2SetupFlow>();
}

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
scoped_ptr<PrivetV2SetupFlow> PrivetV2SetupFlow::CreateWifiFlow(
    ServiceDiscoveryClient* service_discovery_client,
    wifi::WifiManager* wifi_manager,
    // The SSID of the network whose credentials we will be provisioning.
    const std::string& credentials_ssid,
    // The SSID of the device we will be provisioning.
    const std::string& device_ssid) {
  NOTIMPLEMENTED();
  return scoped_ptr<PrivetV2SetupFlow>();
}
#endif

}  // namespace local_discovery
