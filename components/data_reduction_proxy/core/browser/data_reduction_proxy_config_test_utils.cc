// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/base/net_util.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace net {
class NetworkQualityEstimator;
}

namespace data_reduction_proxy {

TestDataReductionProxyConfig::TestDataReductionProxyConfig(
    int params_flags,
    unsigned int params_definitions,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : TestDataReductionProxyConfig(
          make_scoped_ptr(new TestDataReductionProxyParams(params_flags,
                                                           params_definitions))
              .Pass(),
          net_log,
          configurator,
          event_creator) {
}

TestDataReductionProxyConfig::TestDataReductionProxyConfig(
    scoped_ptr<DataReductionProxyConfigValues> config_values,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : DataReductionProxyConfig(net_log,
                               config_values.Pass(),
                               configurator,
                               event_creator),
      network_quality_prohibitively_slow_(false) {
  network_interfaces_.reset(new net::NetworkInterfaceList());
}

TestDataReductionProxyConfig::~TestDataReductionProxyConfig() {
}

bool TestDataReductionProxyConfig::IsNetworkQualityProhibitivelySlow(
    const net::NetworkQualityEstimator* network_quality_estimator) {
  return network_quality_prohibitively_slow_;
}

void TestDataReductionProxyConfig::GetNetworkList(
    net::NetworkInterfaceList* interfaces,
    int policy) {
  for (size_t i = 0; i < network_interfaces_->size(); ++i)
    interfaces->push_back(network_interfaces_->at(i));
}

void TestDataReductionProxyConfig::EnableQuic(bool enable) {
  test_params()->EnableQuic(enable);
}

void TestDataReductionProxyConfig::ResetParamFlagsForTest(int flags) {
  config_values_ =
      make_scoped_ptr(
          new TestDataReductionProxyParams(
              flags,
              TestDataReductionProxyParams::HAS_EVERYTHING &
                  ~TestDataReductionProxyParams::HAS_SSL_ORIGIN &
                  ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
                  ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN))
          .Pass();
}

TestDataReductionProxyParams* TestDataReductionProxyConfig::test_params() {
  return static_cast<TestDataReductionProxyParams*>(config_values_.get());
}

DataReductionProxyConfigValues* TestDataReductionProxyConfig::config_values() {
  return config_values_.get();
}

void TestDataReductionProxyConfig::SetStateForTest(
    bool enabled_by_user,
    bool secure_proxy_allowed) {
  enabled_by_user_ = enabled_by_user;
  secure_proxy_allowed_ = secure_proxy_allowed;
}

void TestDataReductionProxyConfig::ResetLoFiStatusForTest() {
  lofi_off_ = false;
}

void TestDataReductionProxyConfig::SetNetworkProhibitivelySlow(
    bool network_quality_prohibitively_slow) {
  network_quality_prohibitively_slow_ = network_quality_prohibitively_slow;
}

MockDataReductionProxyConfig::MockDataReductionProxyConfig(
    scoped_ptr<DataReductionProxyConfigValues> config_values,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventCreator* event_creator)
    : TestDataReductionProxyConfig(config_values.Pass(),
                                   net_log,
                                   configurator,
                                   event_creator) {
}

MockDataReductionProxyConfig::~MockDataReductionProxyConfig() {
}

void MockDataReductionProxyConfig::UpdateConfigurator(
    bool enabled,
    bool secure_proxy_allowed) {
  DataReductionProxyConfig::UpdateConfigurator(enabled, secure_proxy_allowed);
}

void MockDataReductionProxyConfig::ResetLoFiStatusForTest() {
  lofi_off_ = false;
}

}  // namespace data_reduction_proxy
