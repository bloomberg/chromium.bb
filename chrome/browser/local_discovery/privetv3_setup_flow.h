// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SETUP_FLOW_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SETUP_FLOW_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace local_discovery {

class ServiceDiscoveryClient;

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
namespace wifi {
class WifiManager;
};
#endif

class PrivetV3SetupFlow {
 public:
  class Delegate {
   public:
    typedef base::Callback<void(bool confirm)> ConfirmationCallback;
    typedef base::Callback<void(const std::string& key)> CredentialsCallback;

    virtual ~Delegate() {}

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
    virtual void OnSetupCredentialsNeeded(const std::string& ssid,
                                          const CredentialsCallback& callback);
#endif

    virtual void OnSetupConfirmationNeeded(
        const std::string& confirmation_code,
        const ConfirmationCallback& callback) = 0;

    virtual void OnSetupDone() = 0;

    virtual void OnSetupError() = 0;
  };

  virtual ~PrivetV3SetupFlow() {}

  static scoped_ptr<PrivetV3SetupFlow> CreateMDnsOnlyFlow(
      ServiceDiscoveryClient* service_discovery_client,
      const std::string& service_name);

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
  static scoped_ptr<PrivetV3SetupFlow> CreateWifiFlow(
      ServiceDiscoveryClient* service_discovery_client,
      wifi::WifiManager* wifi_manager,
      // The SSID of the network whose credentials we will be provisioning.
      const std::string& credentials_ssid,
      // The SSID of the device we will be provisioning.
      const std::string& device_ssid);
#endif

  virtual void Start() = 0;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SETUP_FLOW_H_
