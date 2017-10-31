// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_NETWORK_PROPERTY_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_NETWORK_PROPERTY_H_

#include <string>

namespace data_reduction_proxy {

// State of a proxy server scheme. Tracks if the proxy is allowed or not, and
// why it is not allowed.
enum ProxyState {
  PROXY_STATE_ALLOWED = 0,
  PROXY_STATE_DISALLOWED_BY_CARRIER = 1 << 0,
  PROXY_STATE_DISALLOWED_DUE_TO_CAPTIVE_PORTAL = 1 << 1,
  PROXY_STATE_DISALLOWED_DUE_TO_WARMUP_PROBE_FAILURE = 1 << 2,
  PROXY_STATE_LAST = PROXY_STATE_DISALLOWED_DUE_TO_WARMUP_PROBE_FAILURE
};

// Stores the properties of a single network.
class NetworkProperty {
 public:
  NetworkProperty(ProxyState secure_proxies_state,
                  ProxyState insecure_proxies_state);

  NetworkProperty(const NetworkProperty& other);

  NetworkProperty& operator=(const NetworkProperty& other);

  // Creates NetworkProperty from the serialized string.
  explicit NetworkProperty(const std::string& serialized);

  // Serializes |this| to string which can be persisted on the disk.
  std::string ToString() const;

  // Most recent state of the secure and insecure proxies, respectively.
  ProxyState secure_proxies_state() const;
  ProxyState insecure_proxies_state() const;

 private:
  ProxyState secure_proxies_state_;
  ProxyState insecure_proxies_state_;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_NETWORK_PROPERTY_H_