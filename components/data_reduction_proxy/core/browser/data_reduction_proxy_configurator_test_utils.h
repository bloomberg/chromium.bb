// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_TEST_UTILS_H_

#include <string>
#include <vector>

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"

namespace net {
class NetLog;
class ProxyServer;
}

namespace data_reduction_proxy {

class DataReductionProxyEventCreator;

class TestDataReductionProxyConfigurator
    : public DataReductionProxyConfigurator {
 public:
  TestDataReductionProxyConfigurator(
      net::NetLog* net_log,
      DataReductionProxyEventCreator* event_creator);
  ~TestDataReductionProxyConfigurator() override;

  // Overrides of DataReductionProxyConfigurator
  void Enable(bool secure_transport_restricted,
              const std::vector<net::ProxyServer>& proxies_for_http,
              const std::vector<net::ProxyServer>& proxies_for_https) override;

  void Disable() override;

  void AddHostPatternToBypass(const std::string& pattern) override {}

  void AddURLPatternToBypass(const std::string& pattern) override {}

  bool enabled() const {
    return enabled_;
  }

  bool restricted() const {
    return restricted_;
  }

  const std::vector<net::ProxyServer>& proxies_for_http() const {
    return proxies_for_http_;
  }

  const std::vector<net::ProxyServer>& proxies_for_https() const {
    return proxies_for_https_;
  }

 private:
  // True if the proxy has been enabled, i.e., only after |Enable| has been
  // called. Defaults to false.
  bool enabled_;

  // Describes whether the proxy has been put in a restricted mode. True if
  // |Enable| is called with |secure_transport_restricted| set to true. Defaults
  // to false.
  bool restricted_;

  // The origins that are passed to |Enable|.
  std::vector<net::ProxyServer> proxies_for_http_;
  std::vector<net::ProxyServer> proxies_for_https_;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_TEST_UTILS_H_
