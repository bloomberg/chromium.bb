// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/capturing_net_log.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace {

const char kProbeURLWithOKResponse[] = "http://ok.org/";
const char kProbeURLWithBadResponse[] = "http://bad.org/";

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxyConfigTest : public testing::Test {
 public:
  static void AddTestProxyToCommandLine();

  DataReductionProxyConfigTest() {}
  ~DataReductionProxyConfigTest() override {}

  void SetUp() override {
    task_runner_ = new base::TestSimpleTaskRunner();
    request_context_ = new net::TestURLRequestContextGetter(task_runner_.get());
    event_store_.reset(new DataReductionProxyEventStore(task_runner_.get()));
    configurator_.reset(new TestDataReductionProxyConfigurator(
        task_runner_.get(), &net_log_, event_store_.get()));

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
    config_.reset(new MockDataReductionProxyConfig(flags));
    MockDataReductionProxyConfig* config =
        static_cast<MockDataReductionProxyConfig*>(config_.get());
    EXPECT_CALL(*config, GetURLFetcherForProbe()).Times(0);
    EXPECT_CALL(*config, LogProxyState(_, _, _)).Times(0);
    config_->InitDataReductionProxyConfig(
        task_runner_.get(), &net_log_, request_context_.get(),
        configurator_.get(), event_store_.get());
  }

  void SetProbeResult(const std::string& test_url,
                      const std::string& response,
                      ProbeURLFetchResult result,
                      bool success,
                      int expected_calls) {
    if (0 == expected_calls) {
      EXPECT_CALL(*config(), GetURLFetcherForProbe()).Times(0);
      EXPECT_CALL(*config(), RecordProbeURLFetchResult(_)).Times(0);
    } else {
      EXPECT_CALL(*config(), RecordProbeURLFetchResult(result)).Times(1);
      EXPECT_CALL(*config(), GetURLFetcherForProbe())
          .Times(expected_calls)
          .WillRepeatedly(Return(new net::FakeURLFetcher(
              GURL(test_url), config(), response,
              success ? net::HTTP_OK : net::HTTP_INTERNAL_SERVER_ERROR,
              success ? net::URLRequestStatus::SUCCESS
                      : net::URLRequestStatus::FAILED)));
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

  void CheckProbeOnIPChange(const std::string& probe_url,
                            const std::string& response,
                            bool request_succeeded,
                            bool expected_restricted,
                            bool expected_fallback_restricted) {
    SetProbeResult(probe_url, response,
                   FetchResult(!config_->restricted_by_carrier_,
                               request_succeeded && (response == "OK")),
                   request_succeeded, 1);
    config_->OnIPAddressChanged();
    base::MessageLoop::current()->RunUntilIdle();
    CheckProxyConfigs(true, expected_restricted, expected_fallback_restricted);
  }

  ProbeURLFetchResult FetchResult(bool enabled, bool success) {
    if (enabled) {
      if (success)
        return SUCCEEDED_PROXY_ALREADY_ENABLED;
      return FAILED_PROXY_DISABLED;
    }
    if (success)
      return SUCCEEDED_PROXY_ENABLED;
    return FAILED_PROXY_ALREADY_DISABLED;
  }

  void CheckInitDataReductionProxy(bool enabled_at_startup) {
    config_->InitDataReductionProxyConfig(
        task_runner_.get(), &net_log_, request_context_.get(),
        configurator_.get(), event_store_.get());

    base::MessageLoop::current()->RunUntilIdle();
  }

  MockDataReductionProxyConfig* config() { return config_.get(); }

  TestDataReductionProxyConfigurator* configurator() {
    return configurator_.get();
  }

  TestDataReductionProxyParams* params() { return expected_params_.get(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  scoped_ptr<TestDataReductionProxyConfigurator> configurator_;
  scoped_ptr<MockDataReductionProxyConfig> config_;
  scoped_ptr<TestDataReductionProxyParams> expected_params_;
  base::Time last_update_time_;
  net::CapturingNetLog net_log_;
  scoped_ptr<DataReductionProxyEventStore> event_store_;
};

TEST_F(DataReductionProxyConfigTest, TestGetDataReductionProxyOrigin) {
  std::string result = config()->params()->origin().spec();
  EXPECT_EQ(GURL(params()->DefaultOrigin()), GURL(result));
}

TEST_F(DataReductionProxyConfigTest, TestGetDataReductionProxyDevOrigin) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyDev, params()->DefaultDevOrigin());
  ResetSettings(true, true, false, true, false);
  std::string result = config()->params()->origin().spec();
  EXPECT_EQ(GURL(params()->DefaultDevOrigin()), GURL(result));
}

TEST_F(DataReductionProxyConfigTest, TestGetDataReductionProxies) {
  DataReductionProxyParams::DataReductionProxyList proxies =
      params()->GetAllowedProxies();

  unsigned int expected_proxy_size = 2u;
  EXPECT_EQ(expected_proxy_size, proxies.size());

  net::HostPortPair expected_origin =
      net::HostPortPair::FromURL(GURL(params()->DefaultOrigin()));
  net::HostPortPair expected_fallback_origin =
      net::HostPortPair::FromURL(GURL(params()->DefaultFallbackOrigin()));
  EXPECT_EQ(expected_origin.host(), proxies[0].host());
  EXPECT_EQ(expected_origin.port(), proxies[0].EffectiveIntPort());
  EXPECT_EQ(expected_fallback_origin.host(), proxies[1].host());
  EXPECT_EQ(expected_fallback_origin.port(), proxies[1].EffectiveIntPort());
}

TEST_F(DataReductionProxyConfigTest, TestSetProxyConfigs) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAlt, params()->DefaultAltOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAltFallback,
      params()->DefaultAltFallbackOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionSSLProxy, params()->DefaultSSLOrigin());
  ResetSettings(true, true, true, true, false);

  config()->SetProxyConfigs(true, true, false, false);
  EXPECT_TRUE(configurator()->enabled());
  EXPECT_TRUE(
      net::HostPortPair::FromString(params()->DefaultAltOrigin())
          .Equals(net::HostPortPair::FromString(configurator()->origin())));
  EXPECT_TRUE(
      net::HostPortPair::FromString(params()->DefaultAltFallbackOrigin())
          .Equals(net::HostPortPair::FromString(
              configurator()->fallback_origin())));
  EXPECT_TRUE(
      net::HostPortPair::FromString(params()->DefaultSSLOrigin())
          .Equals(net::HostPortPair::FromString(configurator()->ssl_origin())));

  config()->SetProxyConfigs(true, false, false, false);
  EXPECT_TRUE(configurator()->enabled());
  EXPECT_TRUE(
      net::HostPortPair::FromString(params()->DefaultOrigin())
          .Equals(net::HostPortPair::FromString(configurator()->origin())));
  EXPECT_TRUE(net::HostPortPair::FromString(params()->DefaultFallbackOrigin())
                  .Equals(net::HostPortPair::FromString(
                      configurator()->fallback_origin())));
  EXPECT_EQ("", configurator()->ssl_origin());

  config()->SetProxyConfigs(false, true, false, false);
  EXPECT_FALSE(configurator()->enabled());
  EXPECT_EQ("", configurator()->origin());
  EXPECT_EQ("", configurator()->fallback_origin());
  EXPECT_EQ("", configurator()->ssl_origin());

  config()->SetProxyConfigs(false, false, false, false);
  EXPECT_FALSE(configurator()->enabled());
  EXPECT_EQ("", configurator()->origin());
  EXPECT_EQ("", configurator()->fallback_origin());
  EXPECT_EQ("", configurator()->ssl_origin());
}

TEST_F(DataReductionProxyConfigTest, TestSetProxyConfigsHoldback) {
  ResetSettings(true, true, true, true, true);

  config()->SetProxyConfigs(true, true, false, false);
  EXPECT_FALSE(configurator()->enabled());
  EXPECT_EQ("", configurator()->origin());
  EXPECT_EQ("", configurator()->fallback_origin());
  EXPECT_EQ("", configurator()->ssl_origin());
}

TEST_F(DataReductionProxyConfigTest, TestOnIPAddressChanged) {
  base::MessageLoopForUI loop;
  // The proxy is enabled initially.
  config()->enabled_by_user_ = true;
  config()->restricted_by_carrier_ = false;
  config()->SetProxyConfigs(true, false, false, true);
  // IP address change triggers a probe that succeeds. Proxy remains
  // unrestricted.
  CheckProbeOnIPChange(kProbeURLWithOKResponse, "OK", true, false, false);
  // IP address change triggers a probe that fails. Proxy is restricted.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "Bad", true, true, false);
  // IP address change triggers a probe that fails. Proxy remains restricted.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "Bad", true, true, false);
  // IP address change triggers a probe that succeeds. Proxy is unrestricted.
  CheckProbeOnIPChange(kProbeURLWithOKResponse, "OK", true, false, false);
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
  base::MessageLoop::current()->RunUntilIdle();
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
  CheckProbeOnIPChange(kProbeURLWithOKResponse, "OK", true, false, false);
}

}  // namespace data_reduction_proxy
