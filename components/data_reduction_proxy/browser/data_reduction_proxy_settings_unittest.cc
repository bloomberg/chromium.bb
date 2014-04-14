// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"

#include "base/command_line.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kDataReductionProxy[] = "https://foo.com:443/";
const char kDataReductionProxyDev[] = "http://foo-dev.com:80";
const char kDataReductionProxyFallback[] = "http://bar.com:80";
const char kDataReductionProxyKey[] = "12345";

const char kProbeURLWithOKResponse[] = "http://ok.org/";
const char kProbeURLWithBadResponse[] = "http://bad.org/";
const char kProbeURLWithNoResponse[] = "http://no.org/";

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxySettingsTest
    : public ConcreteDataReductionProxySettingsTest<
          DataReductionProxySettings> {
};


TEST_F(DataReductionProxySettingsTest, TestAuthenticationInit) {
  AddProxyToCommandLine();
  net::HttpAuthCache cache;
  DataReductionProxySettings::InitDataReductionAuthentication(&cache);
  DataReductionProxySettings::DataReductionProxyList proxies =
      DataReductionProxySettings::GetDataReductionProxies();
  for (DataReductionProxySettings::DataReductionProxyList::iterator it =
           proxies.begin();  it != proxies.end(); ++it) {
    net::HttpAuthCache::Entry* entry = cache.LookupByPath(*it,
                                                          std::string("/"));
    EXPECT_TRUE(entry != NULL);
    EXPECT_EQ(net::HttpAuth::AUTH_SCHEME_SPDYPROXY, entry->scheme());
    EXPECT_EQ("SpdyProxy", entry->auth_challenge().substr(0,9));
  }
  GURL bad_server = GURL("https://bad.proxy.com/");
  net::HttpAuthCache::Entry* entry =
      cache.LookupByPath(bad_server, std::string());
  EXPECT_TRUE(entry == NULL);
}

TEST_F(DataReductionProxySettingsTest, TestGetDataReductionProxyOrigin) {
  AddProxyToCommandLine();
  // SetUp() adds the origin to the command line, which should be returned here.
  std::string result =
      DataReductionProxySettings::GetDataReductionProxyOrigin();
  EXPECT_EQ(kDataReductionProxy, result);
}

TEST_F(DataReductionProxySettingsTest, TestGetDataReductionProxyDevOrigin) {
  AddProxyToCommandLine();
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyDev, kDataReductionProxyDev);
  std::string result =
      DataReductionProxySettings::GetDataReductionProxyOrigin();
  EXPECT_EQ(kDataReductionProxyDev, result);
}

TEST_F(DataReductionProxySettingsTest, TestGetDataReductionProxies) {
  DataReductionProxySettings::DataReductionProxyList proxies =
      DataReductionProxySettings::GetDataReductionProxies();

  unsigned int expected_proxy_size = 0u;
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  ++expected_proxy_size;
#endif
#if defined(DATA_REDUCTION_FALLBACK_HOST)
  ++expected_proxy_size;
#endif

  EXPECT_EQ(expected_proxy_size, proxies.size());

  // Adding just the fallback on the command line shouldn't add a proxy unless
  // there was already one compiled in.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyFallback, kDataReductionProxyFallback);
  proxies = DataReductionProxySettings::GetDataReductionProxies();

  // So: if there weren't any proxies before, there still won't be.
  // If there were one or two, there will be two now.
  expected_proxy_size = expected_proxy_size == 0u ? 0u : 2u;

  EXPECT_EQ(expected_proxy_size, proxies.size());

  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
    switches::kDataReductionProxy, kDataReductionProxy);
  proxies = DataReductionProxySettings::GetDataReductionProxies();
  EXPECT_EQ(2u, proxies.size());

  // Command line proxies have precedence, so even if there were other values
  // compiled in, these should be the ones in the list.
  EXPECT_EQ("foo.com", proxies[0].host());
  EXPECT_EQ(443 ,proxies[0].EffectiveIntPort());
  EXPECT_EQ("bar.com", proxies[1].host());
  EXPECT_EQ(80, proxies[1].EffectiveIntPort());
}

TEST_F(DataReductionProxySettingsTest, TestAuthHashGeneration) {
  AddProxyToCommandLine();
  std::string salt = "8675309";  // Jenny's number to test the hash generator.
  std::string salted_key = salt + kDataReductionProxyKey + salt;
  base::string16 expected_hash = base::UTF8ToUTF16(base::MD5String(salted_key));
  EXPECT_EQ(expected_hash,
            DataReductionProxySettings::AuthHashForSalt(8675309));
}

// Test that the auth key set by preprocessor directive is not used
// when an origin is set via a switch. This test only does anything useful in
// Chrome builds.
TEST_F(DataReductionProxySettingsTest,
       TestAuthHashGenerationWithOriginSetViaSwitch) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxy, kDataReductionProxy);
  EXPECT_EQ(base::string16(),
            DataReductionProxySettings::AuthHashForSalt(8675309));
}

TEST_F(DataReductionProxySettingsTest, TestIsProxyEnabledOrManaged) {
  settings_->InitPrefMembers();
  EXPECT_FALSE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  pref_service_.SetBoolean(prefs::kDataReductionProxyEnabled, true);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  pref_service_.SetManagedPref(prefs::kDataReductionProxyEnabled,
                               base::Value::CreateBooleanValue(true));
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_TRUE(settings_->IsDataReductionProxyManaged());
}

TEST_F(DataReductionProxySettingsTest, TestAcceptableChallenges) {
  AddProxyToCommandLine();
  typedef struct {
    std::string host;
    std::string realm;
    bool expected_to_succeed;
  } challenge_test;

  challenge_test tests[] = {
    {"foo.com:443", "", false},                 // 0. No realm.
    {"foo.com:443", "xxx", false},              // 1. Wrong realm.
    {"foo.com:443", "spdyproxy", false},        // 2. Case matters.
    {"foo.com:443", "SpdyProxy", true},         // 3. OK.
    {"foo.com:443", "SpdyProxy1234567", true},  // 4. OK
    {"bar.com:80", "SpdyProxy1234567", true},   // 5. OK.
    {"foo.com:443", "SpdyProxyxxx", true},      // 6. OK
    {"", "SpdyProxy1234567", false},            // 7. No challenger.
    {"xxx.net:443", "SpdyProxy1234567", false}, // 8. Wrong host.
    {"foo.com", "SpdyProxy1234567", false},     // 9. No port.
    {"foo.com:80", "SpdyProxy1234567", false},  // 10.Wrong port.
    {"bar.com:81", "SpdyProxy1234567", false},  // 11.Wrong port.
  };

  for (int i = 0; i <= 11; ++i) {
    scoped_refptr<net::AuthChallengeInfo> auth_info(new net::AuthChallengeInfo);
    auth_info->challenger = net::HostPortPair::FromString(tests[i].host);
    auth_info->realm = tests[i].realm;
    EXPECT_EQ(tests[i].expected_to_succeed,
              settings_->IsAcceptableAuthChallenge(auth_info.get()));
  }
}

TEST_F(DataReductionProxySettingsTest, TestChallengeTokens) {
  AddProxyToCommandLine();
  typedef struct {
    std::string realm;
    bool expected_empty_token;
  } token_test;

  token_test tests[] = {
    {"", true},                  // 0. No realm.
    {"xxx", true},               // 1. realm too short.
    {"spdyproxy", true},         // 2. no salt.
    {"SpdyProxyxxx", true},      // 3. Salt not an int.
    {"SpdyProxy1234567", false}, // 4. OK
  };

  for (int i = 0; i <= 4; ++i) {
    scoped_refptr<net::AuthChallengeInfo> auth_info(new net::AuthChallengeInfo);
    auth_info->challenger =
        net::HostPortPair::FromString(kDataReductionProxy);
    auth_info->realm = tests[i].realm;
    base::string16 token = settings_->GetTokenForAuthChallenge(auth_info.get());
    EXPECT_EQ(tests[i].expected_empty_token, token.empty());
  }
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
  AddProxyToCommandLine();

  // Initialize the pref member in |settings_| without the usual callback
  // so it won't trigger MaybeActivateDataReductionProxy when the pref value
  // is set.
  settings_->spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      settings_->GetOriginalProfilePrefs());

  // TODO(bengr): Test enabling/disabling while a probe is outstanding.
  base::MessageLoopForUI loop;
  // The proxy is enabled and unrestructed initially.
  // Request succeeded but with bad response, expect proxy to be restricted.
  CheckProbe(true, kProbeURLWithBadResponse, "Bad", true, true, true);
  // Request succeeded with valid response, expect proxy to be unrestricted.
  CheckProbe(true, kProbeURLWithOKResponse, "OK", true, true, false);
  // Request failed, expect proxy to be enabled but restricted.
  CheckProbe(true, kProbeURLWithNoResponse, "", false, true, true);
  // The proxy is disabled initially. Probes should not be emitted to change
  // state.
  CheckProbe(false, kProbeURLWithOKResponse, "OK", true, false, false);
}

TEST_F(DataReductionProxySettingsTest, TestOnIPAddressChanged) {
  AddProxyToCommandLine();
  base::MessageLoopForUI loop;
  // The proxy is enabled initially.
  pref_service_.SetBoolean(prefs::kDataReductionProxyEnabled, true);
  settings_->spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      settings_->GetOriginalProfilePrefs());
  settings_->enabled_by_user_ = true;
  settings_->restricted_by_carrier_ = false;
  settings_->SetProxyConfigs(true, false, true);
  // IP address change triggers a probe that succeeds. Proxy remains
  // unrestricted.
  CheckProbeOnIPChange(kProbeURLWithOKResponse, "OK", true, false);
  // IP address change triggers a probe that fails. Proxy is restricted.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "Bad", true, true);
  // IP address change triggers a probe that fails. Proxy remains restricted.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "Bad", true, true);
  // IP address change triggers a probe that succeed. Proxy is unrestricted.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "OK", true, false);
}

TEST_F(DataReductionProxySettingsTest, TestOnProxyEnabledPrefChange) {
  AddProxyToCommandLine();
  settings_->InitPrefMembers();
  base::MessageLoopForUI loop;
  // The proxy is enabled initially.
  settings_->enabled_by_user_ = true;
  settings_->SetProxyConfigs(true, false, true);
  // The pref is disabled, so correspondingly should be the proxy.
  CheckOnPrefChange(false, false);
  // The pref is enabled, so correspondingly should be the proxy.
  CheckOnPrefChange(true, true);
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

TEST_F(DataReductionProxySettingsTest, TestSetProxyFromCommandLine) {
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
  EXPECT_FALSE(DataReductionProxySettings::IsDataReductionProxyAllowed());
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_NOT_AVAILABLE));

  scoped_ptr<DataReductionProxyConfigurator> configurator(
      new TestDataReductionProxyConfig());
  scoped_refptr<net::TestURLRequestContextGetter> request_context =
      new net::TestURLRequestContextGetter(base::MessageLoopProxy::current());
  settings_->InitDataReductionProxySettings(
      &pref_service_, &pref_service_, request_context.get(),
      configurator.Pass());

  base::MessageLoop::current()->RunUntilIdle();
}

}  // namespace data_reduction_proxy
