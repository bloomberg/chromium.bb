// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_

#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "net/base/net_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
class URLFetcher;
}

namespace data_reduction_proxy {

class TestDataReductionProxyParams;

// Test version of |DataReductionProxyConfig|, which uses an underlying
// |TestDataReductionProxyParams| to permit overriding of default values
// returning from |DataReductionProxyParams|, as well as exposing methods to
// change the underlying state.
class TestDataReductionProxyConfig : public DataReductionProxyConfig {
 public:
  // Creates a default |TestDataReductionProxyConfig| which uses the following
  // |DataReductionProxyParams| flags:
  // kAllowed | kFallbackAllowed | kPromoAllowed
  TestDataReductionProxyConfig();
  // Creates a |TestDataReductionProxyConfig| with the provided |flags|.
  TestDataReductionProxyConfig(int flags);
  ~TestDataReductionProxyConfig() override;

  void GetNetworkList(net::NetworkInterfaceList* interfaces,
                      int policy) override;

  // Allows tests to reset the params being used for configuration.
  void ResetParamFlagsForTest(int flags);

  // Retrieves the test params being used for the configuration.
  TestDataReductionProxyParams* test_params();

  // Allows tests to set the internal state.
  void SetStateForTest(bool enabled_by_user,
                       bool alternative_enabled_by_user,
                       bool restricted_by_carrier,
                       bool at_startup);

  net::NetworkInterfaceList* interfaces() {
    return network_interfaces_.get();
  }

 private:
  scoped_ptr<net::NetworkInterfaceList> network_interfaces_;
};

// A |TestDataReductionProxyConfig| which permits mocking of methods for
// testing.
class MockDataReductionProxyConfig : public TestDataReductionProxyConfig {
 public:
  // Creates a |MockDataReductionProxyConfig| with the provided |flags|.
  MockDataReductionProxyConfig(int flags);
  ~MockDataReductionProxyConfig() override;

  MOCK_METHOD0(GetURLFetcherForProbe, net::URLFetcher*());
  MOCK_METHOD1(RecordProbeURLFetchResult, void(ProbeURLFetchResult result));
  MOCK_METHOD3(LogProxyState,
               void(bool enabled, bool restricted, bool at_startup));

  // SetProxyConfigs should always call LogProxyState exactly once.
  void SetProxyConfigs(bool enabled,
                       bool alternative_enabled,
                       bool restricted,
                       bool at_startup) override;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
