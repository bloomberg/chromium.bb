// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"

namespace data_reduction_proxy {

TestDataReductionProxyConfigurator::TestDataReductionProxyConfigurator(
    net::NetLog* net_log,
    DataReductionProxyEventCreator* event_creator)
    : DataReductionProxyConfigurator(net_log, event_creator),
      enabled_(false),
      restricted_(false) {
}

TestDataReductionProxyConfigurator::~TestDataReductionProxyConfigurator() {
}

void TestDataReductionProxyConfigurator::Enable(
    bool secure_transport_restricted,
    const std::vector<net::ProxyServer>& proxies_for_http,
    const std::vector<net::ProxyServer>& proxies_for_https) {
  enabled_ = true;
  restricted_ = secure_transport_restricted;
  proxies_for_http_ = proxies_for_http;
  proxies_for_https_ = proxies_for_https;
}

void TestDataReductionProxyConfigurator::Disable() {
  enabled_ = false;
  restricted_ = false;
  proxies_for_http_ = std::vector<net::ProxyServer>();
  proxies_for_https_ = std::vector<net::ProxyServer>();
}

}  // namespace data_reduction_proxy
