// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"

namespace data_reduction_proxy {

TestDataReductionProxyConfigurator::TestDataReductionProxyConfigurator(
    scoped_refptr<base::SequencedTaskRunner> network_task_runner,
    net::NetLog* net_log,
    DataReductionProxyEventStore* event_store)
    : DataReductionProxyConfigurator(network_task_runner, net_log, event_store),
      enabled_(false),
      restricted_(false),
      fallback_restricted_(false) {
}

TestDataReductionProxyConfigurator::~TestDataReductionProxyConfigurator() {
}

void TestDataReductionProxyConfigurator::Enable(
    bool restricted,
    bool fallback_restricted,
    const std::string& primary_origin,
    const std::string& fallback_origin,
    const std::string& ssl_origin) {
  enabled_ = true;
  restricted_ = restricted;
  fallback_restricted_ = fallback_restricted;
  origin_ = primary_origin;
  fallback_origin_ = fallback_origin;
  ssl_origin_ = ssl_origin;
}

void TestDataReductionProxyConfigurator::Disable() {
  enabled_ = false;
  restricted_ = false;
  fallback_restricted_ = false;
  origin_ = std::string();
  fallback_origin_ = std::string();
  ssl_origin_ = std::string();
}

}  // namespace data_reduction_proxy
