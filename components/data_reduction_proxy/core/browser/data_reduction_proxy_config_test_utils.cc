// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/base/net_util.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace data_reduction_proxy {

TestDataReductionProxyConfig::TestDataReductionProxyConfig(
    int params_flags,
    unsigned int params_definitions,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventStore* event_store)
    : DataReductionProxyConfig(
        task_runner, task_runner, net_log,
        make_scoped_ptr(
            new TestDataReductionProxyParams(params_flags, params_definitions))
            .Pass(),
        configurator, event_store) {
  network_interfaces_.reset(new net::NetworkInterfaceList());
}

TestDataReductionProxyConfig::~TestDataReductionProxyConfig() {
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
  config_values_ = make_scoped_ptr(
                new TestDataReductionProxyParams(
                    flags,
                    TestDataReductionProxyParams::HAS_EVERYTHING &
                        ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
                        ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN))
                .Pass();
}

TestDataReductionProxyParams* TestDataReductionProxyConfig::test_params() {
  return static_cast<TestDataReductionProxyParams*>(config_values_.get());
}

void TestDataReductionProxyConfig::SetStateForTest(
    bool enabled_by_user,
    bool alternative_enabled_by_user,
    bool restricted_by_carrier,
    bool at_startup) {
  enabled_by_user_ = enabled_by_user;
  alternative_enabled_by_user_ = alternative_enabled_by_user;
  restricted_by_carrier_ = restricted_by_carrier;
  UpdateConfigurator(enabled_by_user_, alternative_enabled_by_user_,
                     restricted_by_carrier_, at_startup);
}

MockDataReductionProxyConfig::MockDataReductionProxyConfig(
    int params_flags,
    unsigned int params_definitions,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    net::NetLog* net_log,
    DataReductionProxyConfigurator* configurator,
    DataReductionProxyEventStore* event_store)
    : TestDataReductionProxyConfig(
        params_flags, params_definitions, task_runner, net_log, configurator,
        event_store) {
}

MockDataReductionProxyConfig::~MockDataReductionProxyConfig() {
}

void MockDataReductionProxyConfig::UpdateConfigurator(bool enabled,
                                                      bool alternative_enabled,
                                                      bool restricted,
                                                      bool at_startup) {
  EXPECT_CALL(*this, LogProxyState(enabled, restricted, at_startup)).Times(1);
  DataReductionProxyConfig::UpdateConfigurator(enabled, alternative_enabled,
                                               restricted, at_startup);
}

void MockDataReductionProxyConfig::HandleSecureProxyCheckResponse(
    const std::string& response, const net::URLRequestStatus& status) {
  DataReductionProxyConfig::HandleSecureProxyCheckResponse(response, status);
}

}  // namespace data_reduction_proxy
