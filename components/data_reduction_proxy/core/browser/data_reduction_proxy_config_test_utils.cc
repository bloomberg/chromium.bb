// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "net/base/net_util.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace data_reduction_proxy {

MockDataReductionProxyConfig::MockDataReductionProxyConfig()
    : MockDataReductionProxyConfig(DataReductionProxyParams::kAllowed |
                                   DataReductionProxyParams::kFallbackAllowed |
                                   DataReductionProxyParams::kPromoAllowed) {
}

MockDataReductionProxyConfig::MockDataReductionProxyConfig(int flags)
    : DataReductionProxyConfig(
          scoped_ptr<TestDataReductionProxyParams>(
              new TestDataReductionProxyParams(
                  flags,
                  TestDataReductionProxyParams::HAS_EVERYTHING &
                      ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
                      ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN))
              .Pass()) {
  network_interfaces_.reset(new net::NetworkInterfaceList());
}

MockDataReductionProxyConfig::~MockDataReductionProxyConfig() {
}

void MockDataReductionProxyConfig::SetProxyConfigs(bool enabled,
                                                   bool alternative_enabled,
                                                   bool restricted,
                                                   bool at_startup) {
  EXPECT_CALL(*this, LogProxyState(enabled, restricted, at_startup)).Times(1);
  DataReductionProxyConfig::SetProxyConfigs(enabled, alternative_enabled,
                                            restricted, at_startup);
}

void MockDataReductionProxyConfig::GetNetworkList(
    net::NetworkInterfaceList* interfaces,
    int policy) {
  if (!network_interfaces_.get())
    return;
  for (size_t i = 0; i < network_interfaces_->size(); ++i)
    interfaces->push_back(network_interfaces_->at(i));
}

}  // namespace data_reduction_proxy
