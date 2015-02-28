// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "net/base/net_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class NetLog;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigurator;
class DataReductionProxyEventStore;
class TestDataReductionProxyParams;

// Test version of |DataReductionProxyConfig|, which uses an underlying
// |TestDataReductionProxyParams| to permit overriding of default values
// returning from |DataReductionProxyParams|, as well as exposing methods to
// change the underlying state.
class TestDataReductionProxyConfig : public DataReductionProxyConfig {
 public:
  // Creates a |TestDataReductionProxyConfig| with the provided |params_flags|.
  TestDataReductionProxyConfig(
      int params_flags,
      unsigned int params_definitions,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      net::NetLog* net_log,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventStore* event_store);
  ~TestDataReductionProxyConfig() override;

  void GetNetworkList(net::NetworkInterfaceList* interfaces,
                      int policy) override;

  // Allows tests to reset the params being used for configuration.
  void ResetParamFlagsForTest(int flags);

  // Allows tests to reset the params being used for configuration.
  void ResetParamsForTest(scoped_ptr<TestDataReductionProxyParams> params);

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
  // Creates a |MockDataReductionProxyConfig| with the provided |params_flags|.
  MockDataReductionProxyConfig(
      int params_flags,
      unsigned int params_definitions,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      net::NetLog* net_log,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventStore* event_store);
  ~MockDataReductionProxyConfig();

  MOCK_METHOD1(RecordSecureProxyCheckFetchResult,
               void(SecureProxyCheckFetchResult result));
  MOCK_METHOD3(LogProxyState,
               void(bool enabled, bool restricted, bool at_startup));
  MOCK_METHOD3(SetProxyPrefs,
               void(bool enabled, bool alternative_enabled, bool at_startup));
  MOCK_METHOD1(ContainsDataReductionProxy,
               bool(const net::ProxyConfig::ProxyRules& proxy_rules));

  // UpdateConfigurator should always call LogProxyState exactly once.
  void UpdateConfigurator(bool enabled,
                          bool alternative_enabled,
                          bool restricted,
                          bool at_startup) override;

  // HandleSecureProxyCheckResponse should always call
  // RecordSecureProxyCheckFetchResult exactly once.
  void HandleSecureProxyCheckResponse(
      const std::string& response,
      const net::URLRequestStatus& status) override;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
