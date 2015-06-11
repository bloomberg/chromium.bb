// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/network_quality.h"
#include "net/base/network_quality_estimator.h"
#include "net/http/http_status_code.h"
#include "net/log/test_net_log.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace data_reduction_proxy {

class DataReductionProxyConfigTest : public testing::Test {
 public:
  static void AddTestProxyToCommandLine();

  DataReductionProxyConfigTest() {}
  ~DataReductionProxyConfigTest() override {}

  void SetUp() override {
    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithMockConfig()
                        .WithTestConfigurator()
                        .WithMockDataReductionProxyService()
                        .Build();

    ResetSettings(true, true, false, true, false);

    expected_params_.reset(new TestDataReductionProxyParams(
        DataReductionProxyParams::kAllowed |
            DataReductionProxyParams::kFallbackAllowed |
            DataReductionProxyParams::kPromoAllowed,
        TestDataReductionProxyParams::HAS_EVERYTHING &
            ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
            ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN));
  }

  void ResetSettings(bool allowed,
                     bool fallback_allowed,
                     bool alt_allowed,
                     bool promo_allowed,
                     bool holdback) {
    int flags = 0;
    if (allowed)
      flags |= DataReductionProxyParams::kAllowed;
    if (fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    if (alt_allowed)
      flags |= DataReductionProxyParams::kAlternativeAllowed;
    if (promo_allowed)
      flags |= DataReductionProxyParams::kPromoAllowed;
    if (holdback)
      flags |= DataReductionProxyParams::kHoldback;
    config()->ResetParamFlagsForTest(flags);
    EXPECT_CALL(*config(), LogProxyState(_, _, _)).Times(0);
  }

  void ExpectSecureProxyCheckResult(SecureProxyCheckFetchResult result) {
    EXPECT_CALL(*config(), RecordSecureProxyCheckFetchResult(result)).Times(1);
  }

  void CheckProxyConfigs(bool expected_enabled, bool expected_restricted) {
    ASSERT_EQ(expected_restricted, configurator()->restricted());
    ASSERT_EQ(expected_enabled, configurator()->enabled());
  }

  class TestResponder {
   public:
    void ExecuteCallback(FetcherResponseCallback callback) {
      callback.Run(response, status, http_response_code);
    }

    std::string response;
    net::URLRequestStatus status;
    int http_response_code;
  };

  void CheckSecureProxyCheckOnIPChange(const std::string& response,
                                       SecureProxyCheckFetchResult fetch_result,
                                       bool expected_restricted,
                                       bool expected_fallback_restricted) {
    ExpectSecureProxyCheckResult(fetch_result);
    TestResponder responder;
    responder.response = response;
    responder.status =
        net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK);
    responder.http_response_code = net::HTTP_OK;
    EXPECT_CALL(*config(), SecureProxyCheck(_, _))
        .Times(1)
        .WillRepeatedly(testing::WithArgs<1>(
            testing::Invoke(&responder, &TestResponder::ExecuteCallback)));
    config()->OnIPAddressChanged();
    test_context_->RunUntilIdle();
    CheckProxyConfigs(true, expected_restricted);
  }

  void RunUntilIdle() {
    test_context_->RunUntilIdle();
  }

  scoped_ptr<DataReductionProxyConfig> BuildConfig(
      scoped_ptr<DataReductionProxyParams> params) {
    params->EnableQuic(false);
    return make_scoped_ptr(new DataReductionProxyConfig(
        test_context_->net_log(), params.Pass(), test_context_->configurator(),
        test_context_->event_creator()));
  }

  MockDataReductionProxyConfig* config() {
    return test_context_->mock_config();
  }

  TestDataReductionProxyConfigurator* configurator() {
    return test_context_->test_configurator();
  }

  TestDataReductionProxyParams* params() const {
    return expected_params_.get();
  }

  DataReductionProxyEventCreator* event_creator() const {
    return test_context_->event_creator();
  }

  net::NetLog* net_log() const { return test_context_->net_log(); }

 private:
  base::MessageLoopForIO message_loop_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
  scoped_ptr<TestDataReductionProxyParams> expected_params_;
};

TEST_F(DataReductionProxyConfigTest, TestMaybeDisableIfVPNTrialDisabled) {
  // Simulate a VPN connection.
  config()->interfaces()->clear();
  config()->interfaces()->push_back(net::NetworkInterface(
      "tun0", /* network interface name */
      "tun0", /* network interface friendly name */
      0,      /* interface index */
      net::NetworkChangeNotifier::CONNECTION_WIFI, net::IPAddressNumber(),
      0,                             /* network prefix */
      net::IP_ADDRESS_ATTRIBUTE_NONE /* ip address attribute */
      ));
  EXPECT_TRUE(config()->MaybeDisableIfVPN());
}

TEST_F(DataReductionProxyConfigTest, TestMaybeDisableIfVPNTrialEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "force-fieldtrials", "DataReductionProxyUseDataSaverOnVPN/Enabled");
  EXPECT_FALSE(config()->MaybeDisableIfVPN());
}

TEST_F(DataReductionProxyConfigTest, TestUpdateConfigurator) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAlt, params()->DefaultAltOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAltFallback,
      params()->DefaultAltFallbackOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionSSLProxy, params()->DefaultSSLOrigin());
  ResetSettings(true, true, true, true, false);

  config()->UpdateConfigurator(true, true, false, false);
  EXPECT_TRUE(configurator()->enabled());

  std::vector<net::ProxyServer> expected_http_proxies;
  expected_http_proxies.push_back(net::ProxyServer::FromURI(
      params()->DefaultAltOrigin(), net::ProxyServer::SCHEME_HTTP));
  std::vector<net::ProxyServer> expected_https_proxies;
  expected_https_proxies.push_back(net::ProxyServer::FromURI(
      params()->DefaultSSLOrigin(), net::ProxyServer::SCHEME_HTTP));
  EXPECT_TRUE(
      net::ProxyServer::FromURI(params()->DefaultAltFallbackOrigin(),
                                net::ProxyServer::SCHEME_HTTP).is_valid());
  EXPECT_THAT(expected_http_proxies,
              testing::ContainerEq(configurator()->proxies_for_http()));
  EXPECT_THAT(expected_https_proxies,
              testing::ContainerEq(configurator()->proxies_for_https()));

  config()->UpdateConfigurator(true, false, false, false);
  expected_http_proxies.clear();
  expected_https_proxies.clear();
  EXPECT_TRUE(configurator()->enabled());
  expected_http_proxies.push_back(net::ProxyServer::FromURI(
      params()->DefaultOrigin(), net::ProxyServer::SCHEME_HTTP));
  expected_http_proxies.push_back(net::ProxyServer::FromURI(
      params()->DefaultFallbackOrigin(), net::ProxyServer::SCHEME_HTTP));
  EXPECT_THAT(expected_http_proxies,
              testing::ContainerEq(configurator()->proxies_for_http()));
  EXPECT_THAT(expected_https_proxies,
              testing::ContainerEq(configurator()->proxies_for_https()));

  config()->UpdateConfigurator(false, true, false, false);
  EXPECT_FALSE(configurator()->enabled());
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());

  config()->UpdateConfigurator(false, false, false, false);
  EXPECT_FALSE(configurator()->enabled());
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
}

TEST_F(DataReductionProxyConfigTest, TestUpdateConfiguratorHoldback) {
  ResetSettings(true, true, true, true, true);

  config()->UpdateConfigurator(true, true, false, false);
  EXPECT_FALSE(configurator()->enabled());
  EXPECT_TRUE(configurator()->proxies_for_http().empty());
  EXPECT_TRUE(configurator()->proxies_for_https().empty());
}

TEST_F(DataReductionProxyConfigTest, TestOnIPAddressChanged) {
  // The proxy is enabled initially.
  config()->enabled_by_user_ = true;
  config()->secure_proxy_allowed_ = true;
  config()->UpdateConfigurator(true, false, true, true);
  // IP address change triggers a secure proxy check that succeeds. Proxy
  // remains unrestricted.
  CheckSecureProxyCheckOnIPChange("OK", SUCCEEDED_PROXY_ALREADY_ENABLED, false,
                                  false);
  // IP address change triggers a secure proxy check that fails. Proxy is
  // restricted.
  CheckSecureProxyCheckOnIPChange("Bad", FAILED_PROXY_DISABLED, true, false);
  // IP address change triggers a secure proxy check that fails. Proxy remains
  // restricted.
  CheckSecureProxyCheckOnIPChange("Bad", FAILED_PROXY_ALREADY_DISABLED, true,
                                  false);
  // IP address change triggers a secure proxy check that succeeds. Proxy is
  // unrestricted.
  CheckSecureProxyCheckOnIPChange("OK", SUCCEEDED_PROXY_ENABLED, false, false);
  // Simulate a VPN connection. The proxy should be disabled.
  config()->interfaces()->clear();
  config()->interfaces()->push_back(net::NetworkInterface(
      "tun0", /* network interface name */
      "tun0", /* network interface friendly name */
      0,      /* interface index */
      net::NetworkChangeNotifier::CONNECTION_WIFI,
      net::IPAddressNumber(),        /* IP address */
      0,                             /* network prefix */
      net::IP_ADDRESS_ATTRIBUTE_NONE /* ip address attribute */
      ));
  config()->OnIPAddressChanged();
  RunUntilIdle();
  CheckProxyConfigs(false, false);

  // Check that the proxy is re-enabled if a non-VPN connection is later used.
  config()->interfaces()->clear();
  config()->interfaces()->push_back(net::NetworkInterface(
      "eth0", /* network interface name */
      "eth0", /* network interface friendly name */
      0,      /* interface index */
      net::NetworkChangeNotifier::CONNECTION_WIFI, net::IPAddressNumber(),
      0,                             /* network prefix */
      net::IP_ADDRESS_ATTRIBUTE_NONE /* ip address attribute */
      ));
  CheckSecureProxyCheckOnIPChange("OK", SUCCEEDED_PROXY_ALREADY_ENABLED, false,
                                  false);
}

TEST_F(DataReductionProxyConfigTest,
       TestOnIPAddressChanged_SecureProxyDisabledByDefault) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(
      data_reduction_proxy::switches::kDataReductionProxyStartSecureDisabled);

  // The proxy is enabled initially.
  config()->enabled_by_user_ = true;
  config()->secure_proxy_allowed_ = false;
  config()->UpdateConfigurator(true, false, false, true);

  // IP address change triggers a secure proxy check that succeeds. Proxy
  // becomes unrestricted.
  CheckSecureProxyCheckOnIPChange("OK", SUCCEEDED_PROXY_ENABLED, false, false);
  // IP address change triggers a secure proxy check that fails. Proxy is
  // restricted before the check starts, and remains disabled.
  ExpectSecureProxyCheckResult(PROXY_DISABLED_BEFORE_CHECK);
  CheckSecureProxyCheckOnIPChange("Bad", FAILED_PROXY_ALREADY_DISABLED, true,
                                  false);
  // IP address change triggers a secure proxy check that fails. Proxy remains
  // restricted.
  CheckSecureProxyCheckOnIPChange("Bad", FAILED_PROXY_ALREADY_DISABLED, true,
                                  false);
  // IP address change triggers a secure proxy check that succeeds. Proxy is
  // unrestricted.
  CheckSecureProxyCheckOnIPChange("OK", SUCCEEDED_PROXY_ENABLED, false, false);
  // Simulate a VPN connection. The proxy should be disabled.
  config()->interfaces()->clear();
  config()->interfaces()->push_back(net::NetworkInterface(
      "tun0", /* network interface name */
      "tun0", /* network interface friendly name */
      0,      /* interface index */
      net::NetworkChangeNotifier::CONNECTION_WIFI,
      net::IPAddressNumber(),        /* IP address */
      0,                             /* network prefix */
      net::IP_ADDRESS_ATTRIBUTE_NONE /* ip address attribute */
      ));
  config()->OnIPAddressChanged();
  RunUntilIdle();
  CheckProxyConfigs(false, false);

  // Check that the proxy is re-enabled if a non-VPN connection is later used.
  config()->interfaces()->clear();
  config()->interfaces()->push_back(net::NetworkInterface(
      "eth0", /* network interface name */
      "eth0", /* network interface friendly name */
      0,      /* interface index */
      net::NetworkChangeNotifier::CONNECTION_WIFI, net::IPAddressNumber(),
      0,                             /* network prefix */
      net::IP_ADDRESS_ATTRIBUTE_NONE /* ip address attribute */
      ));
  ExpectSecureProxyCheckResult(PROXY_DISABLED_BEFORE_CHECK);
  CheckSecureProxyCheckOnIPChange("OK", SUCCEEDED_PROXY_ENABLED, false, false);
}

std::string GetRetryMapKeyFromOrigin(std::string origin) {
  // The retry map has the scheme prefix for https but not for http.
  return origin;
}

TEST_F(DataReductionProxyConfigTest, AreProxiesBypassed) {
  const struct {
    // proxy flags
    bool allowed;
    bool fallback_allowed;
    bool alt_allowed;
    // is https request
    bool is_https;
    // proxies in retry map
    bool origin;
    bool fallback_origin;
    bool alt_origin;
    bool alt_fallback_origin;
    bool ssl_origin;

    bool expected_result;
  } tests[]  = {
      { // proxy flags
        false,
        false,
        false,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        false,
        false,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        false,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        true,
        // proxies in retry map
        false,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        false,
        false,
        true,
        // is https request
        true,
        // proxies in retry map
        false,
        false,
        false,
        false,
        true,
        // expected result
        true,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        true,
        // proxies in retry map
        false,
        false,
        false,
        false,
        true,
        // expected result
        true,
      },
      { // proxy flags
        true,
        false,
        false,
        // is https request
        false,
        // proxies in retry map
        true,
        false,
        false,
        false,
        false,
        // expected result
        true,
      },
      { // proxy flags
        true,
        true,
        false,
        // is https request
        false,
        // proxies in retry map
        false,
        true,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        false,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        false,
        true,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        false,
        // is https request
        false,
        // proxies in retry map
        true,
        false,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        false,
        // is https request
        false,
        // proxies in retry map
        true,
        true,
        false,
        false,
        false,
        // expected result
        true,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        true,
        true,
        false,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        true,
        false,
        true,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        true,
        true,
        false,
        // expected result
        false,
      },
      { // proxy flags
        false,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        true,
        true,
        false,
        // expected result
        true,
      },
      { // proxy flags
        false,
        true,
        false,
        // is https request
        false,
        // proxies in retry map
        false,
        false,
        true,
        false,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        true,
        false,
        true,
        true,
        false,
        // expected result
        false,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        false,
        // proxies in retry map
        true,
        true,
        true,
        true,
        true,
        // expected result
        true,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        true,
        // proxies in retry map
        true,
        true,
        true,
        true,
        true,
        // expected result
        true,
      },
      { // proxy flags
        true,
        true,
        true,
        // is https request
        true,
        // proxies in retry map
        true,
        true,
        true,
        true,
        false,
        // expected result
        false,
      },
  };

  // The retry map has the scheme prefix for https but not for http.
  std::string origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultOrigin());
  std::string fallback_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultFallbackOrigin());
  std::string alt_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultAltOrigin());
  std::string alt_fallback_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultAltFallbackOrigin());
  std::string ssl_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultSSLOrigin());

  for (size_t i = 0; i < arraysize(tests); ++i) {
    net::ProxyConfig::ProxyRules rules;
    std::vector<std::string> proxies;

    if (tests[i].allowed)
      proxies.push_back(origin);
    if (tests[i].alt_allowed)
      proxies.push_back(alt_origin);
    if (tests[i].allowed && tests[i].fallback_allowed)
      proxies.push_back(fallback_origin);
    if (tests[i].alt_allowed && tests[i].fallback_allowed)
      proxies.push_back(alt_fallback_origin);

    std::string proxy_rules = "http=" + JoinString(proxies, ",") + ",direct://;"
        + (tests[i].alt_allowed ? ("https=" + ssl_origin + ",direct://;") : "");

    rules.ParseFromString(proxy_rules);

    int flags = 0;
    if (tests[i].allowed)
      flags |= DataReductionProxyParams::kAllowed;
    if (tests[i].alt_allowed)
      flags |= DataReductionProxyParams::kAlternativeAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    unsigned int has_definitions =
        TestDataReductionProxyParams::HAS_EVERYTHING &
        ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
        ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN;
    scoped_ptr<TestDataReductionProxyParams> params(
        new TestDataReductionProxyParams(flags, has_definitions));
    scoped_ptr<DataReductionProxyConfig> config = BuildConfig(params.Pass());

    net::ProxyRetryInfoMap retry_map;
    net::ProxyRetryInfo retry_info;
    retry_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();

    if (tests[i].origin)
      retry_map[origin] = retry_info;
    if (tests[i].fallback_origin)
      retry_map[fallback_origin] = retry_info;
    if (tests[i].alt_origin)
      retry_map[alt_origin] = retry_info;
    if (tests[i].alt_fallback_origin)
      retry_map[alt_fallback_origin] = retry_info;
    if (tests[i].ssl_origin)
      retry_map[ssl_origin] = retry_info;

    bool was_bypassed = config->AreProxiesBypassed(retry_map,
                                                   rules,
                                                   tests[i].is_https,
                                                   NULL);

    EXPECT_EQ(tests[i].expected_result, was_bypassed) << i;
  }
}

TEST_F(DataReductionProxyConfigTest, AreProxiesBypassedRetryDelay) {
  std::string origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultOrigin());
  std::string fallback_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultFallbackOrigin());

  net::ProxyConfig::ProxyRules rules;
  std::vector<std::string> proxies;

  proxies.push_back(origin);
  proxies.push_back(fallback_origin);

  std::string proxy_rules = "http=" + JoinString(proxies, ",") + ",direct://;";

  rules.ParseFromString(proxy_rules);

  int flags = 0;
  flags |= DataReductionProxyParams::kAllowed;
  flags |= DataReductionProxyParams::kFallbackAllowed;
  unsigned int has_definitions =
      TestDataReductionProxyParams::HAS_EVERYTHING &
      ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
      ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN;
  scoped_ptr<TestDataReductionProxyParams> params(
      new TestDataReductionProxyParams(flags, has_definitions));
  scoped_ptr<DataReductionProxyConfig> config = BuildConfig(params.Pass());

  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo retry_info;

  retry_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();
  retry_map[origin] = retry_info;

  retry_info.bad_until = base::TimeTicks();
  retry_map[fallback_origin] = retry_info;

  bool was_bypassed = config->AreProxiesBypassed(retry_map,
                                                 rules,
                                                 false,
                                                 NULL);

  EXPECT_FALSE(was_bypassed);

  base::TimeDelta delay = base::TimeDelta::FromHours(2);
  retry_info.bad_until = base::TimeTicks::Now() + delay;
  retry_info.current_delay = delay;
  retry_map[origin] = retry_info;

  delay = base::TimeDelta::FromHours(1);
  retry_info.bad_until = base::TimeTicks::Now() + delay;
  retry_info.current_delay = delay;
  retry_map[fallback_origin] = retry_info;

  base::TimeDelta min_retry_delay;
  was_bypassed = config->AreProxiesBypassed(retry_map,
                                            rules,
                                            false,
                                            &min_retry_delay);
  EXPECT_TRUE(was_bypassed);
  EXPECT_EQ(delay, min_retry_delay);
}

TEST_F(DataReductionProxyConfigTest, IsDataReductionProxyWithParams) {
  const struct {
    net::HostPortPair host_port_pair;
    bool fallback_allowed;
    bool alt_fallback_allowed;
    bool set_dev_origin;
    bool expected_result;
    net::HostPortPair expected_first;
    net::HostPortPair expected_second;
    bool expected_is_fallback;
    bool expected_is_alternative;
    bool expected_is_ssl;
  } tests[] = {
      {net::ProxyServer::FromURI(TestDataReductionProxyParams::DefaultOrigin(),
                                 net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       true,
       true,
       false,
       true,
       net::ProxyServer::FromURI(TestDataReductionProxyParams::DefaultOrigin(),
                                 net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       false,
       false,
       false},
      {net::ProxyServer::FromURI(TestDataReductionProxyParams::DefaultOrigin(),
                                 net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       false,
       false,
       false,
       true,
       net::ProxyServer::FromURI(TestDataReductionProxyParams::DefaultOrigin(),
                                 net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       net::HostPortPair::FromURL(GURL()),
       false,
       false,
       false},
      {net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       true,
       true,
       false,
       true,
       net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       net::HostPortPair::FromURL(GURL()),
       true,
       false,
       false},
      {net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       false,
       false,
       false,
       false,
       net::HostPortPair::FromURL(GURL()),
       net::HostPortPair::FromURL(GURL()),
       false,
       false,
       false},
      {net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultAltOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       true,
       true,
       false,
       true,
       net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultAltOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       false,
       true,
       false},
      {net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultAltOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       false,
       false,
       false,
       true,
       net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultAltOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       net::HostPortPair::FromURL(GURL()),
       false,
       true,
       false},
      {net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       true,
       true,
       false,
       true,
       net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       net::HostPortPair::FromURL(GURL()),
       true,
       true,
       false},
      {net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       false,
       false,
       false,
       false,
       net::HostPortPair::FromURL(GURL()),
       net::HostPortPair::FromURL(GURL()),
       false,
       false,
       false},
      {net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultSSLOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       true,
       true,
       false,
       true,
       net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultSSLOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       net::HostPortPair::FromURL(GURL()),
       false,
       true,
       true},
      {net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultDevOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       true,
       true,
       true,
       true,
       net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultDevOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultDevFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP).host_port_pair(),
       false,
       false,
       false},
      {net::ProxyServer::FromURI(TestDataReductionProxyParams::DefaultOrigin(),
                                 net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       true,
       true,
       true,
       false,
       net::HostPortPair::FromURL(GURL()),
       net::HostPortPair::FromURL(GURL()),
       false,
       false,
       false},
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    int flags = DataReductionProxyParams::kAllowed |
                DataReductionProxyParams::kAlternativeAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    if (tests[i].alt_fallback_allowed)
      flags |= DataReductionProxyParams::kAlternativeFallbackAllowed;
    unsigned int has_definitions = TestDataReductionProxyParams::HAS_EVERYTHING;
    if (!tests[i].set_dev_origin) {
      has_definitions &= ~TestDataReductionProxyParams::HAS_DEV_ORIGIN;
      has_definitions &= ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN;
    }
    scoped_ptr<TestDataReductionProxyParams> params(
        new TestDataReductionProxyParams(flags, has_definitions));
    DataReductionProxyTypeInfo proxy_type_info;
    scoped_ptr<DataReductionProxyConfig> config(new DataReductionProxyConfig(
        net_log(), params.Pass(), configurator(), event_creator()));
    EXPECT_EQ(
        tests[i].expected_result,
        config->IsDataReductionProxy(tests[i].host_port_pair, &proxy_type_info))
        << i;
    EXPECT_EQ(
        net::ProxyServer::FromURI(tests[i].expected_first.ToString(),
                                  net::ProxyServer::SCHEME_HTTP).is_valid(),
        proxy_type_info.proxy_servers.size() >= 1 &&
            proxy_type_info.proxy_servers[0].is_valid())
        << i;
    if (proxy_type_info.proxy_servers.size() >= 1 &&
        proxy_type_info.proxy_servers[0].is_valid()) {
      EXPECT_TRUE(tests[i].expected_first.Equals(
          proxy_type_info.proxy_servers[0].host_port_pair()))
          << i;
    }
    EXPECT_EQ(
        net::ProxyServer::FromURI(tests[i].expected_second.ToString(),
                                  net::ProxyServer::SCHEME_HTTP).is_valid(),
        proxy_type_info.proxy_servers.size() >= 2 &&
            proxy_type_info.proxy_servers[1].is_valid())
        << i;
    if (proxy_type_info.proxy_servers.size() >= 2 &&
        proxy_type_info.proxy_servers[1].is_valid()) {
      EXPECT_TRUE(tests[i].expected_second.Equals(
          proxy_type_info.proxy_servers[1].host_port_pair()))
          << i;
    }

    EXPECT_EQ(tests[i].expected_is_fallback, proxy_type_info.is_fallback) << i;
    EXPECT_EQ(tests[i].expected_is_alternative, proxy_type_info.is_alternative)
        << i;
    EXPECT_EQ(tests[i].expected_is_ssl, proxy_type_info.is_ssl) << i;
  }
}

TEST_F(DataReductionProxyConfigTest, IsDataReductionProxyWithMutableConfig) {
  std::vector<net::ProxyServer> proxies_for_http;
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      "https://origin.net:443", net::ProxyServer::SCHEME_HTTP));
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      "http://origin.net:80", net::ProxyServer::SCHEME_HTTP));
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      "quic://anotherorigin.net:443", net::ProxyServer::SCHEME_HTTP));
  const struct {
    net::HostPortPair host_port_pair;
    bool expected_result;
    net::ProxyServer expected_first;
    net::ProxyServer expected_second;
    net::ProxyServer expected_third;
    bool expected_is_fallback;
  } tests[] = {
      {
       proxies_for_http[0].host_port_pair(),
       true,
       proxies_for_http[0],
       proxies_for_http[1],
       proxies_for_http[2],
       false,
      },
      {
       proxies_for_http[1].host_port_pair(),
       true,
       proxies_for_http[1],
       proxies_for_http[2],
       net::ProxyServer(),
       true,
      },
      {
       proxies_for_http[2].host_port_pair(),
       true,
       proxies_for_http[2],
       net::ProxyServer(),
       net::ProxyServer(),
       true,
      },
      {
       net::HostPortPair(),
       false,
       net::ProxyServer(),
       net::ProxyServer(),
       net::ProxyServer(),
       false,
      },
      {
       net::HostPortPair::FromString("https://otherorigin.net:443"),
       false,
       net::ProxyServer(),
       net::ProxyServer(),
       net::ProxyServer(),
       false,
      },
  };

  scoped_ptr<DataReductionProxyMutableConfigValues> config_values =
      DataReductionProxyMutableConfigValues::CreateFromParams(params());
  config_values->UpdateValues(proxies_for_http);
  scoped_ptr<DataReductionProxyConfig> config(new DataReductionProxyConfig(
      net_log(), config_values.Pass(), configurator(), event_creator()));
  for (size_t i = 0; i < arraysize(tests); ++i) {
    DataReductionProxyTypeInfo proxy_type_info;
    EXPECT_EQ(tests[i].expected_result,
              config->IsDataReductionProxy(tests[i].host_port_pair,
                                           &proxy_type_info));
    std::vector<net::ProxyServer> expected_proxy_servers;
    if (tests[i].expected_first.is_valid())
      expected_proxy_servers.push_back(tests[i].expected_first);
    if (tests[i].expected_second.is_valid())
      expected_proxy_servers.push_back(tests[i].expected_second);
    if (tests[i].expected_third.is_valid())
      expected_proxy_servers.push_back(tests[i].expected_third);
    EXPECT_THAT(proxy_type_info.proxy_servers,
                testing::ContainerEq(expected_proxy_servers))
        << i;
    EXPECT_FALSE(proxy_type_info.is_alternative) << i;
    EXPECT_FALSE(proxy_type_info.is_ssl) << i;
  }
}

TEST_F(DataReductionProxyConfigTest, LoFiOn) {
  const struct {
    bool lofi_switch_enabled;
    bool lofi_enabled_field_trial_group;
    bool network_prohibitively_slow;
    bool expect_lofi_header;

  } tests[] = {
      {
       // Lo-Fi off.
       false,
       false,
       false,
       false,
      },
      {
       // In enabled field trial group but network quality is not bad.
       false,
       true,
       false,
       false,
      },
      {
       // Not in enabled field trial group and network quality is bad.
       false,
       false,
       true,
       false,
      },
      {
       // In enabled field trial group and network quality is bad.
       false,
       true,
       true,
       true,
      },
      {
       // Lo-Fi enabled through command line switch.
       true,
       false,
       false,
       true,
      },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    config()->ResetLoFiStatusForTest();
    if (tests[i].lofi_switch_enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    }

    EXPECT_CALL(*config(), IsIncludedInLoFiEnabledFieldTrial())
        .WillRepeatedly(
            testing::Return(tests[i].lofi_enabled_field_trial_group));

    EXPECT_CALL(*config(), IsNetworkQualityProhibitivelySlow(_))
        .WillRepeatedly(testing::Return(tests[i].network_prohibitively_slow));

    config()->UpdateLoFiStatusOnMainFrameRequest(false, nullptr);

    EXPECT_EQ(tests[i].expect_lofi_header,
              config()->ShouldUseLoFiHeaderForRequests())
        << i;
  }
}

TEST_F(DataReductionProxyConfigTest, LoFiStatusTransition) {
  const struct {
    bool lofi_switch_enabled;
  } tests[] = {
      {
       false,
      },
      {
       true,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    config()->ResetLoFiStatusForTest();
    if (tests[i].lofi_switch_enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    } else {
      EXPECT_CALL(*config(), IsIncludedInLoFiEnabledFieldTrial())
          .WillRepeatedly(testing::Return(true));

      EXPECT_CALL(*config(), IsNetworkQualityProhibitivelySlow(_))
          .WillRepeatedly(testing::Return(true));
    }

    // First, set the status to enabled.
    config()->UpdateLoFiStatusOnMainFrameRequest(false, nullptr);
    EXPECT_EQ(true, config()->ShouldUseLoFiHeaderForRequests()) << i;

    // Full page reload with Lo-Fi disabled (main frame request).
    config()->UpdateLoFiStatusOnMainFrameRequest(true, nullptr);
    EXPECT_FALSE(config()->ShouldUseLoFiHeaderForRequests()) << i;

    // New page load (main frame request).
    config()->UpdateLoFiStatusOnMainFrameRequest(false, nullptr);
    EXPECT_EQ(true, config()->ShouldUseLoFiHeaderForRequests()) << i;
  }
}

// Overrides net::NetworkQualityEstimator::GetEstimate() for testing purposes.
class TestNetworkQualityEstimator : public net::NetworkQualityEstimator {
 public:
  TestNetworkQualityEstimator() : rtt_(base::TimeDelta()), kbps_(0) {}

  ~TestNetworkQualityEstimator() override {}

  net::NetworkQuality GetEstimate() const override {
    return net::NetworkQuality(rtt_, 0.0, kbps_, 0.0);
  }

  void SetRtt(base::TimeDelta rtt) { rtt_ = rtt; }

  void SetKbps(uint64_t kbps) { kbps_ = kbps; }

 private:
  base::TimeDelta rtt_;
  uint64_t kbps_;
};

TEST_F(DataReductionProxyConfigTest, AutoLoFiParams) {
  DataReductionProxyConfig config(nullptr, nullptr, configurator(),
                                  event_creator());
  variations::testing::ClearAllVariationParams();
  std::map<std::string, std::string> variation_params;

  int rtt_msec = 120;
  char rtt[20];
  base::strings::SafeSPrintf(rtt, "%d", rtt_msec);
  variation_params["rtt_msec"] = rtt;

  variation_params["kbps"] = "240";

  int hysteresis_sec = 360;
  char hysteresis[20];
  base::strings::SafeSPrintf(hysteresis, "%d", hysteresis_sec);
  variation_params["hysteresis_period_seconds"] = hysteresis;

  variation_params["spurious_field"] = "480";

  ASSERT_TRUE(variations::AssociateVariationParams(
      DataReductionProxyParams::GetLoFiFieldTrialName(), "Enabled",
      variation_params));

  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(
      DataReductionProxyParams::GetLoFiFieldTrialName(), "Enabled");

  config.PopulateAutoLoFiParams();

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(rtt_msec),
            config.auto_lofi_minimum_rtt_);
  EXPECT_EQ(240U, config.auto_lofi_maximum_kbps_);
  EXPECT_EQ(base::TimeDelta::FromSeconds(hysteresis_sec),
            config.auto_lofi_hysteresis_);

  TestNetworkQualityEstimator test_network_quality_estimator;

  // RTT is higher than threshold. Network is slow.
  test_network_quality_estimator.SetRtt(
      base::TimeDelta::FromMilliseconds(rtt_msec + 1));
  EXPECT_TRUE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Network quality improved. RTT is lower than the threshold. However,
  // network should still be marked as slow because of hysteresis.
  test_network_quality_estimator.SetRtt(
      base::TimeDelta::FromMilliseconds(rtt_msec - 1));
  EXPECT_TRUE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Change the last update time to be older than the hysteresis duration.
  // Checking network quality afterwards should show that network is no longer
  // slow.
  config.network_quality_last_updated_ =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(hysteresis_sec + 1);
  EXPECT_FALSE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Changing the RTT has no effect because of hysteresis.
  test_network_quality_estimator.SetRtt(
      base::TimeDelta::FromMilliseconds(rtt_msec + 1));
  EXPECT_FALSE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Change in connection type changes the network quality despite hysteresis.
  config.connection_type_ = net::NetworkChangeNotifier::CONNECTION_WIFI;
  EXPECT_TRUE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));
}

}  // namespace data_reduction_proxy
