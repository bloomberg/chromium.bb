// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include "base/command_line.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/capturing_net_log.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace {

const char kSecureProxyCheckWithOKResponse[] = "http://ok.org/";
const char kSecureProxyCheckWithBadResponse[] = "http://bad.org/";

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxyConfigTest : public testing::Test {
 public:
  static void AddTestProxyToCommandLine();

  DataReductionProxyConfigTest() {}
  ~DataReductionProxyConfigTest() override {}

  void SetUp() override {
    test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(DataReductionProxyParams::kAllowed |
                             DataReductionProxyParams::kFallbackAllowed |
                             DataReductionProxyParams::kPromoAllowed)
            .WithParamsDefinitions(
                TestDataReductionProxyParams::HAS_EVERYTHING &
                    ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
                    ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN)
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

  void SetSecureProxyCheckResult(const std::string& test_url,
                                 const std::string& response,
                                 SecureProxyCheckFetchResult result,
                                 bool success,
                                 int expected_calls) {
    if (0 == expected_calls) {
      EXPECT_CALL(*config(), RecordSecureProxyCheckFetchResult(_)).Times(0);
    } else {
      EXPECT_CALL(*config(), RecordSecureProxyCheckFetchResult(result))
          .Times(1);
    }
  }

  void CheckProxyConfigs(bool expected_enabled,
                         bool expected_restricted,
                         bool expected_fallback_restricted) {
    ASSERT_EQ(expected_restricted, configurator()->restricted());
    ASSERT_EQ(expected_fallback_restricted,
              configurator()->fallback_restricted());
    ASSERT_EQ(expected_enabled, configurator()->enabled());
  }

  class TestResponder {
   public:
    void ExecuteCallback(FetcherResponseCallback callback) {
      callback.Run(response, status);
    }

    std::string response;
    net::URLRequestStatus status;
  };

  void CheckSecureProxyCheckOnIPChange(
      const std::string& secure_proxy_check_url,
      const std::string& response,
      bool request_succeeded,
      bool expected_restricted,
      bool expected_fallback_restricted) {
    SetSecureProxyCheckResult(
        secure_proxy_check_url, response, FetchResult(
            !config()->restricted_by_carrier_,
            request_succeeded && (response == "OK")),
        request_succeeded, 1);
    MockDataReductionProxyService* service =
        test_context_->mock_data_reduction_proxy_service();
    TestResponder responder;
    responder.response = response;
    responder.status =
        net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK);
    EXPECT_CALL(*service, SecureProxyCheck(_, _))
        .Times(1)
        .WillRepeatedly(testing::WithArgs<1>(
            testing::Invoke(&responder, &TestResponder::ExecuteCallback)));
    config()->OnIPAddressChanged();
    test_context_->RunUntilIdle();
    CheckProxyConfigs(true, expected_restricted, expected_fallback_restricted);
  }

  SecureProxyCheckFetchResult FetchResult(bool enabled, bool success) {
    if (enabled) {
      if (success)
        return SUCCEEDED_PROXY_ALREADY_ENABLED;
      return FAILED_PROXY_DISABLED;
    }
    if (success)
      return SUCCEEDED_PROXY_ENABLED;
    return FAILED_PROXY_ALREADY_DISABLED;
  }

  void RunUntilIdle() {
    test_context_->RunUntilIdle();
  }

  MockDataReductionProxyConfig* config() {
    return test_context_->mock_config();
  }

  TestDataReductionProxyConfigurator* configurator() {
    return test_context_->test_configurator();
  }

  TestDataReductionProxyParams* params() { return expected_params_.get(); }

 private:
  scoped_ptr<DataReductionProxyTestContext> test_context_;
  scoped_ptr<TestDataReductionProxyParams> expected_params_;
};

TEST_F(DataReductionProxyConfigTest, TestGetDataReductionProxyOrigin) {
  EXPECT_EQ(params()->DefaultOrigin(), config()->params()->origin().ToURI());
}

TEST_F(DataReductionProxyConfigTest, TestGetDataReductionProxyDevOrigin) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyDev, params()->DefaultDevOrigin());
  ResetSettings(true, true, false, true, false);
  EXPECT_EQ(params()->DefaultDevOrigin(), config()->params()->origin().ToURI());
}

TEST_F(DataReductionProxyConfigTest, TestGetDataReductionProxies) {
  DataReductionProxyParams::DataReductionProxyList proxies =
      params()->GetAllowedProxies();

  unsigned int expected_proxy_size = 2u;
  EXPECT_EQ(expected_proxy_size, proxies.size());

  EXPECT_EQ(net::ProxyServer::FromURI(params()->DefaultOrigin(),
                                      net::ProxyServer::SCHEME_HTTP),
            proxies[0]);

  EXPECT_EQ(net::ProxyServer::FromURI(params()->DefaultFallbackOrigin(),
                                      net::ProxyServer::SCHEME_HTTP),
            proxies[1]);
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

  EXPECT_EQ(
      net::ProxyServer::FromURI(
          params()->DefaultAltOrigin(),
          net::ProxyServer::SCHEME_HTTP),
          net::ProxyServer::FromURI(configurator()->origin(),
                                    net::ProxyServer::SCHEME_HTTP));
  EXPECT_TRUE(net::ProxyServer::FromURI(
      params()->DefaultAltFallbackOrigin(),
      net::ProxyServer::SCHEME_HTTP).is_valid());
  EXPECT_TRUE(configurator()->fallback_origin().empty());
  EXPECT_EQ(net::ProxyServer::FromURI(params()->DefaultSSLOrigin(),
                                      net::ProxyServer::SCHEME_HTTP),
            net::ProxyServer::FromURI(configurator()->ssl_origin(),
                                      net::ProxyServer::SCHEME_HTTP));

  config()->UpdateConfigurator(true, false, false, false);
  EXPECT_TRUE(configurator()->enabled());
  EXPECT_TRUE(
      net::HostPortPair::FromString(params()->DefaultOrigin())
          .Equals(net::HostPortPair::FromString(configurator()->origin())));
  EXPECT_TRUE(net::HostPortPair::FromString(params()->DefaultFallbackOrigin())
                  .Equals(net::HostPortPair::FromString(
                      configurator()->fallback_origin())));
  EXPECT_TRUE(configurator()->ssl_origin().empty());

  config()->UpdateConfigurator(false, true, false, false);
  EXPECT_FALSE(configurator()->enabled());
  EXPECT_TRUE(configurator()->origin().empty());
  EXPECT_TRUE(configurator()->fallback_origin().empty());
  EXPECT_TRUE(configurator()->ssl_origin().empty());

  config()->UpdateConfigurator(false, false, false, false);
  EXPECT_FALSE(configurator()->enabled());
  EXPECT_TRUE(configurator()->origin().empty());
  EXPECT_TRUE(configurator()->fallback_origin().empty());
  EXPECT_TRUE(configurator()->ssl_origin().empty());
}

TEST_F(DataReductionProxyConfigTest, TestUpdateConfiguratorHoldback) {
  ResetSettings(true, true, true, true, true);

  config()->UpdateConfigurator(true, true, false, false);
  EXPECT_FALSE(configurator()->enabled());
  EXPECT_EQ("", configurator()->origin());
  EXPECT_EQ("", configurator()->fallback_origin());
  EXPECT_EQ("", configurator()->ssl_origin());
}

TEST_F(DataReductionProxyConfigTest, TestOnIPAddressChanged) {
  // The proxy is enabled initially.
  config()->enabled_by_user_ = true;
  config()->restricted_by_carrier_ = false;
  config()->UpdateConfigurator(true, false, false, true);
  // IP address change triggers a secure proxy check that succeeds. Proxy
  // remains unrestricted.
  CheckSecureProxyCheckOnIPChange(
      kSecureProxyCheckWithOKResponse, "OK", true, false, false);
  // IP address change triggers a secure proxy check that fails. Proxy is
  // restricted.
  CheckSecureProxyCheckOnIPChange(
      kSecureProxyCheckWithBadResponse, "Bad", true, true, false);
  // IP address change triggers a secure proxy check that fails. Proxy remains
  // restricted.
  CheckSecureProxyCheckOnIPChange(
      kSecureProxyCheckWithBadResponse, "Bad", true, true, false);
  // IP address change triggers a secure proxy check that succeeds. Proxy is
  // unrestricted.
  CheckSecureProxyCheckOnIPChange(
      kSecureProxyCheckWithOKResponse, "OK", true, false, false);
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
  CheckProxyConfigs(false, false, false);

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
  CheckSecureProxyCheckOnIPChange(
      kSecureProxyCheckWithOKResponse, "OK", true, false, false);
}

}  // namespace data_reduction_proxy
