// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_NETWORK_PROPERTIES_MANAGER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_NETWORK_PROPERTIES_MANAGER_H_

#include "base/macros.h"
#include "components/data_reduction_proxy/proto/network_properties.pb.h"

namespace data_reduction_proxy {

// Stores the properties of a single network.
class NetworkPropertiesManager {
 public:
  NetworkPropertiesManager();

  ~NetworkPropertiesManager();

  // Returns true if usage of secure proxies are allowed on the current network.
  bool IsSecureProxyAllowed() const;

  // Returns true if usage of insecure proxies are allowed on the current
  // network.
  bool IsInsecureProxyAllowed() const;

  // Returns true if usage of secure proxies has been disallowed by the carrier
  // on the current network.
  bool IsSecureProxyDisallowedByCarrier() const;

  // Sets the status of whether the usage of secure proxies is disallowed by the
  // carrier on the current network.
  void SetIsSecureProxyDisallowedByCarrier(bool disallowed_by_carrier);

  // Returns true if the current network has a captive portal.
  bool IsCaptivePortal() const;

  // Sets the status of whether the current network has a captive portal or not.
  // If the current network has captive portal, usage of secure proxies is
  // disallowed.
  void SetIsCaptivePortal(bool is_captive_portal);

  // If secure_proxy is true, returns true if the warmup URL probe has failed
  // on secure proxies on the current network. Otherwise, returns true if the
  // warmup URL probe has failed on insecure proxies.
  bool HasWarmupURLProbeFailed(bool secure_proxy) const;

  // Sets the status of whether the fetching of warmup URL failed on the current
  // network. Sets the status for secure proxies if |secure_proxy| is true.
  // Otherwise, sets the status for the insecure proxies.
  void SetHasWarmupURLProbeFailed(bool secure_proxy,
                                  bool warmup_url_probe_failed);

 private:
  // State of the proxies on the current network.
  NetworkProperties network_properties_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPropertiesManager);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_NETWORK_PROPERTIES_MANAGER_H_