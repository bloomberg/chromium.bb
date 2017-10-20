// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class SingleThreadTaskRunner;
class TickClock;
}

namespace net {
class NetworkQualityEstimator;
class NetLog;
class ProxyServer;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigurator;
class DataReductionProxyEventCreator;
class DataReductionProxyMutableConfigValues;
class TestDataReductionProxyParams;

// Test version of |DataReductionProxyConfig|, which uses an underlying
// |TestDataReductionProxyParams| to permit overriding of default values
// returning from |DataReductionProxyParams|, as well as exposing methods to
// change the underlying state.
class TestDataReductionProxyConfig : public DataReductionProxyConfig {
 public:
  TestDataReductionProxyConfig(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      net::NetLog* net_log,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventCreator* event_creator);

  // Creates a |TestDataReductionProxyConfig| with the provided |config_values|.
  // This permits any DataReductionProxyConfigValues to be used (such as
  // DataReductionProxyParams or DataReductionProxyMutableConfigValues).
  TestDataReductionProxyConfig(
      std::unique_ptr<DataReductionProxyConfigValues> config_values,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      net::NetLog* net_log,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventCreator* event_creator);

  ~TestDataReductionProxyConfig() override;

  // Allows tests to reset the params being used for configuration.
  void ResetParamFlagsForTest();

  // Retrieves the test params being used for the configuration.
  TestDataReductionProxyParams* test_params();

  // Retrieves the underlying config values.
  // TODO(jeremyim): Rationalize with test_params().
  DataReductionProxyConfigValues* config_values();

  // Resets the Lo-Fi status to default state.
  void ResetLoFiStatusForTest();

  // Sets the |tick_clock_| to |tick_clock|. Ownership of |tick_clock| is not
  // passed to the callee.
  void SetTickClock(base::TickClock* tick_clock);

  base::TimeTicks GetTicksNow() const override;

  bool WasDataReductionProxyUsed(
      const net::URLRequest* request,
      DataReductionProxyTypeInfo* proxy_info) const override;

  // Sets the data reduction proxy as not used. Subsequent calls to
  // WasDataReductionProxyUsed() would return false.
  void SetWasDataReductionProxyNotUsed();

  // Sets the proxy index of the data reduction proxy. Subsequent calls to
  // WasDataReductionProxyUsed are affected.
  void SetWasDataReductionProxyUsedProxyIndex(int proxy_index);

  // Resets the behavior of WasDataReductionProxyUsed() calls.
  void ResetWasDataReductionProxyUsed();

  // Sets if the captive portal probe has been blocked for the current network.
  void SetIsCaptivePortal(bool is_captive_portal);

  void SetConnectionTypeForTesting(
      net::NetworkChangeNotifier::ConnectionType connection_type) {
    connection_type_ = connection_type;
  }

  bool ShouldAddDefaultProxyBypassRules() const override;

  void SetShouldAddDefaultProxyBypassRules(bool add_default_proxy_bypass_rules);

  using DataReductionProxyConfig::UpdateConfigForTesting;

 private:
  bool GetIsCaptivePortal() const override;

  base::TickClock* tick_clock_;

  base::Optional<bool> was_data_reduction_proxy_used_;
  base::Optional<int> proxy_index_;

  // Set to true if the captive portal probe for the current network has been
  // blocked.
  bool is_captive_portal_;

  // True if the default bypass rules should be added. Should be set to false
  // when fetching resources from an embedded test server running on localhost.
  bool add_default_proxy_bypass_rules_;

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyConfig);
};

// A |TestDataReductionProxyConfig| which permits mocking of methods for
// testing.
class MockDataReductionProxyConfig : public TestDataReductionProxyConfig {
 public:
  // Creates a |MockDataReductionProxyConfig|.
  MockDataReductionProxyConfig(
      std::unique_ptr<DataReductionProxyConfigValues> config_values,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      net::NetLog* net_log,
      DataReductionProxyConfigurator* configurator,
      DataReductionProxyEventCreator* event_creator);
  ~MockDataReductionProxyConfig();

  MOCK_METHOD2(SetProxyPrefs, void(bool enabled, bool at_startup));
  MOCK_CONST_METHOD2(IsDataReductionProxy,
                     bool(const net::ProxyServer& proxy_server,
                          DataReductionProxyTypeInfo* proxy_info));
  MOCK_CONST_METHOD2(WasDataReductionProxyUsed,
                     bool(const net::URLRequest*,
                          DataReductionProxyTypeInfo* proxy_info));
  MOCK_CONST_METHOD1(ContainsDataReductionProxy,
                     bool(const net::ProxyConfig::ProxyRules& proxy_rules));
  MOCK_CONST_METHOD2(IsBypassedByDataReductionProxyLocalRules,
                     bool(const net::URLRequest& request,
                          const net::ProxyConfig& data_reduction_proxy_config));
  MOCK_CONST_METHOD3(AreDataReductionProxiesBypassed,
                     bool(const net::URLRequest& request,
                          const net::ProxyConfig& data_reduction_proxy_config,
                          base::TimeDelta* min_retry_delay));
  MOCK_METHOD1(SecureProxyCheck,
               void(SecureProxyCheckerCallback fetcher_callback));
  MOCK_METHOD1(
      IsNetworkQualityProhibitivelySlow,
      bool(const net::NetworkQualityEstimator* network_quality_estimator));

  using DataReductionProxyConfig::UpdateConfigForTesting;

  // Resets the Lo-Fi status to default state.
  void ResetLoFiStatusForTest();
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
