// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"

#include "base/command_line.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kProbeURLWithOKResponse[] = "http://ok.org/";
const char kProbeURLWithBadResponse[] = "http://bad.org/";
const char kProbeURLWithNoResponse[] = "http://no.org/";
const char kWarmupURLWithNoContentResponse[] = "http://warm.org/";

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxySettingsTest
    : public ConcreteDataReductionProxySettingsTest<
          DataReductionProxySettings> {
};

TEST_F(DataReductionProxySettingsTest, TestGetDataReductionProxyOrigin) {
  // SetUp() adds the origin to the command line, which should be returned here.
  std::string result =
      settings_->params()->origin().spec();
  EXPECT_EQ(GURL(expected_params_->DefaultOrigin()), GURL(result));
}

TEST_F(DataReductionProxySettingsTest, TestGetDataReductionProxyDevOrigin) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyDev, expected_params_->DefaultDevOrigin());
  ResetSettings(true, true, false, true, false);
  std::string result =
      settings_->params()->origin().spec();
  EXPECT_EQ(GURL(expected_params_->DefaultDevOrigin()), GURL(result));
}


TEST_F(DataReductionProxySettingsTest, TestGetDataReductionProxies) {
  DataReductionProxyParams::DataReductionProxyList proxies =
      expected_params_->GetAllowedProxies();

  unsigned int expected_proxy_size = 2u;
  EXPECT_EQ(expected_proxy_size, proxies.size());

  net::HostPortPair expected_origin =
      net::HostPortPair::FromURL(GURL(expected_params_->DefaultOrigin()));
  net::HostPortPair expected_fallback_origin =
      net::HostPortPair::FromURL(
          GURL(expected_params_->DefaultFallbackOrigin()));
  EXPECT_EQ(expected_origin.host(), proxies[0].host());
  EXPECT_EQ(expected_origin.port() ,proxies[0].EffectiveIntPort());
  EXPECT_EQ(expected_fallback_origin.host(), proxies[1].host());
  EXPECT_EQ(expected_fallback_origin.port(), proxies[1].EffectiveIntPort());
}

TEST_F(DataReductionProxySettingsTest, TestSetProxyConfigs) {
  TestDataReductionProxyParams drp_params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING &
      ~TestDataReductionProxyParams::HAS_DEV_ORIGIN);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAlt, drp_params.DefaultAltOrigin());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAltFallback,
      drp_params.DefaultAltFallbackOrigin());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionSSLProxy, drp_params.DefaultSSLOrigin());
  ResetSettings(true, true, true, true, false);
  TestDataReductionProxyConfig* config =
      static_cast<TestDataReductionProxyConfig*>(
          settings_->configurator());

  settings_->SetProxyConfigs(true, true, false, false);
  EXPECT_TRUE(config->enabled_);
  EXPECT_TRUE(net::HostPortPair::FromString(
      expected_params_->DefaultAltOrigin()).Equals(
          net::HostPortPair::FromString(config->origin_)));
  EXPECT_TRUE(net::HostPortPair::FromString(
      expected_params_->DefaultAltFallbackOrigin()).Equals(
          net::HostPortPair::FromString(config->fallback_origin_)));
  EXPECT_TRUE(net::HostPortPair::FromString(
      expected_params_->DefaultSSLOrigin()).Equals(
          net::HostPortPair::FromString(config->ssl_origin_)));

  settings_->SetProxyConfigs(true, false, false, false);
  EXPECT_TRUE(config->enabled_);
  EXPECT_TRUE(net::HostPortPair::FromString(drp_params.DefaultOrigin()).Equals(
      net::HostPortPair::FromString(config->origin_)));
  EXPECT_TRUE(net::HostPortPair::FromString(
      drp_params.DefaultFallbackOrigin()).Equals(
          net::HostPortPair::FromString(config->fallback_origin_)));
  EXPECT_EQ("", config->ssl_origin_);

  settings_->SetProxyConfigs(false, true, false, false);
  EXPECT_FALSE(config->enabled_);
  EXPECT_EQ("", config->origin_);
  EXPECT_EQ("", config->fallback_origin_);
  EXPECT_EQ("", config->ssl_origin_);

  settings_->SetProxyConfigs(false, false, false, false);
  EXPECT_FALSE(config->enabled_);
  EXPECT_EQ("", config->origin_);
  EXPECT_EQ("", config->fallback_origin_);
  EXPECT_EQ("", config->ssl_origin_);
}

TEST_F(DataReductionProxySettingsTest, TestSetProxyConfigsHoldback) {
  ResetSettings(true, true, true, true, true);
  TestDataReductionProxyConfig* config =
      static_cast<TestDataReductionProxyConfig*>(
          settings_->configurator());

   // Holdback.
  settings_->SetProxyConfigs(true, true, false, false);
  EXPECT_FALSE(config->enabled_);
  EXPECT_EQ("", config->origin_);
  EXPECT_EQ("", config->fallback_origin_);
  EXPECT_EQ("", config->ssl_origin_);
}

TEST_F(DataReductionProxySettingsTest, TestIsProxyEnabledOrManaged) {
  settings_->InitPrefMembers();
  base::MessageLoopForUI loop;
  // The proxy is disabled initially.
  settings_->enabled_by_user_ = false;
  settings_->SetProxyConfigs(false, false, false, false);

  EXPECT_FALSE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  CheckOnPrefChange(true, true, false);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  CheckOnPrefChange(true, true, true);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_TRUE(settings_->IsDataReductionProxyManaged());

  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(DataReductionProxySettingsTest, TestResetDataReductionStatistics) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_time;
  settings_->ResetDataReductionStatistics();
  settings_->GetContentLengths(kNumDaysInHistory,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  EXPECT_EQ(0L, original_content_length);
  EXPECT_EQ(0L, received_content_length);
  EXPECT_EQ(last_update_time_.ToInternalValue(), last_update_time);
}

TEST_F(DataReductionProxySettingsTest, TestContentLengths) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_time;

  // Request |kNumDaysInHistory| days.
  settings_->GetContentLengths(kNumDaysInHistory,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  const unsigned int days = kNumDaysInHistory;
  // Received content length history values are 0 to |kNumDaysInHistory - 1|.
  int64 expected_total_received_content_length = (days - 1L) * days / 2;
  // Original content length history values are 0 to
  // |2 * (kNumDaysInHistory - 1)|.
  long expected_total_original_content_length = (days - 1L) * days;
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);
  EXPECT_EQ(last_update_time_.ToInternalValue(), last_update_time);

  // Request |kNumDaysInHistory - 1| days.
  settings_->GetContentLengths(kNumDaysInHistory - 1,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  expected_total_received_content_length -= (days - 1);
  expected_total_original_content_length -= 2 * (days - 1);
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);

  // Request 0 days.
  settings_->GetContentLengths(0,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  expected_total_received_content_length = 0;
  expected_total_original_content_length = 0;
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);

  // Request 1 day. First day had 0 bytes so should be same as 0 days.
  settings_->GetContentLengths(1,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);
}

// TODO(marq): Add a test to verify that MaybeActivateDataReductionProxy
// is called when the pref in |settings_| is enabled.
TEST_F(DataReductionProxySettingsTest, TestMaybeActivateDataReductionProxy) {
  // Initialize the pref member in |settings_| without the usual callback
  // so it won't trigger MaybeActivateDataReductionProxy when the pref value
  // is set.
  settings_->spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      settings_->GetOriginalProfilePrefs());
  settings_->data_reduction_proxy_alternative_enabled_.Init(
      prefs::kDataReductionProxyAltEnabled,
      settings_->GetOriginalProfilePrefs());

  // TODO(bengr): Test enabling/disabling while a probe is outstanding.
  base::MessageLoopForUI loop;
  // The proxy is enabled and unrestructed initially.
  // Request succeeded but with bad response, expect proxy to be restricted.
  CheckProbe(true,
             kProbeURLWithBadResponse,
             kWarmupURLWithNoContentResponse,
             "Bad",
             true,
             true,
             true,
             false);
  // Request succeeded with valid response, expect proxy to be unrestricted.
  CheckProbe(true,
             kProbeURLWithOKResponse,
             kWarmupURLWithNoContentResponse,
             "OK",
             true,
             true,
             false,
             false);
  // Request failed, expect proxy to be enabled but restricted.
  CheckProbe(true,
             kProbeURLWithNoResponse,
             kWarmupURLWithNoContentResponse,
             "",
             false,
             true,
             true,
             false);
  // The proxy is disabled initially. Probes should not be emitted to change
  // state.
  CheckProbe(false,
             kProbeURLWithOKResponse,
             kWarmupURLWithNoContentResponse,
             "OK",
             true,
             false,
             false,
             false);
}

TEST_F(DataReductionProxySettingsTest, TestOnIPAddressChanged) {
  base::MessageLoopForUI loop;
  // The proxy is enabled initially.
  pref_service_.SetBoolean(prefs::kDataReductionProxyEnabled, true);
  settings_->spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      settings_->GetOriginalProfilePrefs());
  settings_->data_reduction_proxy_alternative_enabled_.Init(
      prefs::kDataReductionProxyAltEnabled,
      settings_->GetOriginalProfilePrefs());
  settings_->enabled_by_user_ = true;
  settings_->restricted_by_carrier_ = false;
  settings_->SetProxyConfigs(true, false, false, true);
  // IP address change triggers a probe that succeeds. Proxy remains
  // unrestricted.
  CheckProbeOnIPChange(kProbeURLWithOKResponse,
                       kWarmupURLWithNoContentResponse,
                       "OK",
                       true,
                       false,
                       false);
  // IP address change triggers a probe that fails. Proxy is restricted.
  CheckProbeOnIPChange(kProbeURLWithBadResponse,
                       kWarmupURLWithNoContentResponse,
                       "Bad",
                       true,
                       true,
                       false);
  // IP address change triggers a probe that fails. Proxy remains restricted.
  CheckProbeOnIPChange(kProbeURLWithBadResponse,
                       kWarmupURLWithNoContentResponse,
                       "Bad",
                       true,
                       true,
                       false);
  // IP address change triggers a probe that succeeds. Proxy is unrestricted.
  CheckProbeOnIPChange(kProbeURLWithOKResponse,
                       kWarmupURLWithNoContentResponse,
                       "OK",
                       true,
                       false,
                       false);
  // Simulate a VPN connection. The proxy should be disabled.
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  settings->network_interfaces_.reset(new net::NetworkInterfaceList());
  settings->network_interfaces_->push_back(
      net::NetworkInterface("tun0",  /* network interface name */
                            "tun0",  /* network interface friendly name */
                            0,  /* interface index */
                            net::NetworkChangeNotifier::CONNECTION_WIFI,
                            net::IPAddressNumber(), /* IP address */
                            0  /* network prefix */
                            ));
  settings_->OnIPAddressChanged();
  base::MessageLoop::current()->RunUntilIdle();
  CheckProxyConfigs(false, false, false);

  // Check that the proxy is re-enabled if a non-VPN connection is later used.
  settings->network_interfaces_.reset(new net::NetworkInterfaceList());
  settings->network_interfaces_->push_back(
      net::NetworkInterface("eth0",  /* network interface name */
                            "eth0",  /* network interface friendly name */
                            0,  /* interface index */
                            net::NetworkChangeNotifier::CONNECTION_WIFI,
                            net::IPAddressNumber(),
                            0  /* network prefix */
                            ));
  CheckProbeOnIPChange(kProbeURLWithOKResponse,
                       kWarmupURLWithNoContentResponse,
                       "OK",
                       true,
                       false,
                       false);
}

TEST_F(DataReductionProxySettingsTest, TestOnProxyEnabledPrefChange) {
  settings_->InitPrefMembers();
  base::MessageLoopForUI loop;
  // The proxy is enabled initially.
  settings_->enabled_by_user_ = true;
  settings_->SetProxyConfigs(true, false, false, true);
  // The pref is disabled, so correspondingly should be the proxy.
  CheckOnPrefChange(false, false, false);
  // The pref is enabled, so correspondingly should be the proxy.
  CheckOnPrefChange(true, true, false);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOn) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));

  pref_service_.SetBoolean(prefs::kDataReductionProxyEnabled, true);
  CheckInitDataReductionProxy(true);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOff) {
  // InitDataReductionProxySettings with the preference off will directly call
  // LogProxyState.
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_DISABLED));

  pref_service_.SetBoolean(prefs::kDataReductionProxyEnabled, false);
  CheckInitDataReductionProxy(false);
}

TEST_F(DataReductionProxySettingsTest, TestEnableProxyFromCommandLine) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDataReductionProxy);
  CheckInitDataReductionProxy(true);
}

TEST_F(DataReductionProxySettingsTest, TestGetDailyContentLengths) {
  DataReductionProxySettings::ContentLengthList result =
      settings_->GetDailyContentLengths(prefs::kDailyHttpOriginalContentLength);

  ASSERT_FALSE(result.empty());
  ASSERT_EQ(kNumDaysInHistory, result.size());

  for (size_t i = 0; i < kNumDaysInHistory; ++i) {
    long expected_length =
        static_cast<long>((kNumDaysInHistory - 1 - i) * 2);
    ASSERT_EQ(expected_length, result[i]);
  }
}

TEST_F(DataReductionProxySettingsTest, CheckInitMetricsWhenNotAllowed) {
  // No call to |AddProxyToCommandLine()| was made, so the proxy feature
  // should be unavailable.
  base::MessageLoopForUI loop;
  // Clear the command line. Setting flags can force the proxy to be allowed.
  CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);

  ResetSettings(false, false, false, false, false);
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_FALSE(settings->params()->allowed());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_NOT_AVAILABLE));

  scoped_ptr<DataReductionProxyConfigurator> configurator(
      new TestDataReductionProxyConfig());
  settings_->SetProxyConfigurator(configurator.get());
  scoped_refptr<net::TestURLRequestContextGetter> request_context =
      new net::TestURLRequestContextGetter(base::MessageLoopProxy::current());
  settings_->InitDataReductionProxySettings(
      &pref_service_,
      &pref_service_,
      request_context.get());
  settings_->SetOnDataReductionEnabledCallback(
      base::Bind(&DataReductionProxySettingsTestBase::
                 RegisterSyntheticFieldTrialCallback,
                 base::Unretained(this)));

  base::MessageLoop::current()->RunUntilIdle();
}

}  // namespace data_reduction_proxy
