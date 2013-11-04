// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_unittest.h"

#include "base/command_line.h"
#include "base/md5.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/pref_names.h"
#include "components/variations/entropy_provider.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

const char kDataReductionProxyOrigin[] = "https://foo.com:443/";
const char kDataReductionProxyFallback[] = "http://bar.com:80";
const char kDataReductionProxyAuth[] = "12345";

const char kProbeURLWithOKResponse[] = "http://ok.org/";
const char kProbeURLWithBadResponse[] = "http://bad.org/";
const char kProbeURLWithNoResponse[] = "http://no.org/";


DataReductionProxySettingsTestBase::DataReductionProxySettingsTestBase()
    : testing::Test() {
}

DataReductionProxySettingsTestBase::~DataReductionProxySettingsTestBase() {}

void DataReductionProxySettingsTestBase::AddProxyToCommandLine() {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSpdyProxyAuthOrigin, kDataReductionProxyOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSpdyProxyAuthFallback, kDataReductionProxyFallback);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSpdyProxyAuthValue, kDataReductionProxyAuth);
}

// testing::Test implementation:
void DataReductionProxySettingsTestBase::SetUp() {
  PrefRegistrySimple* registry = pref_service_.registry();
  registry->RegisterListPref(prefs::kDailyHttpOriginalContentLength);
  registry->RegisterListPref(prefs::kDailyHttpReceivedContentLength);
  registry->RegisterInt64Pref(
      prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
  registry->RegisterDictionaryPref(prefs::kProxy);
  registry->RegisterBooleanPref(prefs::kSpdyProxyAuthEnabled, false);
  registry->RegisterBooleanPref(prefs::kSpdyProxyAuthWasEnabledBefore, false);
  ResetSettings();

  ListPrefUpdate original_update(&pref_service_,
                                 prefs::kDailyHttpOriginalContentLength);
  ListPrefUpdate received_update(&pref_service_,
                                 prefs::kDailyHttpReceivedContentLength);
  for (int64 i = 0; i < spdyproxy::kNumDaysInHistory; i++) {
    original_update->Insert(0, new StringValue(base::Int64ToString(2 * i)));
    received_update->Insert(0, new StringValue(base::Int64ToString(i)));
  }
  last_update_time_ = base::Time::Now().LocalMidnight();
  pref_service_.SetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate,
      last_update_time_.ToInternalValue());
}

template <class C>
void DataReductionProxySettingsTestBase::ResetSettings() {
  MockDataReductionProxySettings<C>* settings =
      new MockDataReductionProxySettings<C>;
  EXPECT_CALL(*settings, GetOriginalProfilePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(&pref_service_));
  EXPECT_CALL(*settings, GetLocalStatePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(&pref_service_));
  EXPECT_CALL(*settings, GetURLFetcher()).Times(0);
  EXPECT_CALL(*settings, LogProxyState(_, _)).Times(0);
  settings_.reset(settings);
}

// Explicitly generate required instantiations.
template void
DataReductionProxySettingsTestBase::ResetSettings<DataReductionProxySettings>();
template void
DataReductionProxySettingsTestBase::ResetSettings<
    DataReductionProxySettingsAndroid>();

template <class C>
void DataReductionProxySettingsTestBase::SetProbeResult(
    const std::string& test_url,
    const std::string& response,
    bool success,
    int expected_calls)  {
  MockDataReductionProxySettings<C>* settings =
      static_cast<MockDataReductionProxySettings<C>*>(settings_.get());
  if (0 == expected_calls) {
    EXPECT_CALL(*settings, GetURLFetcher()).Times(0);
  } else {
    EXPECT_CALL(*settings, GetURLFetcher())
        .Times(expected_calls)
        .WillRepeatedly(Return(new net::FakeURLFetcher(
            GURL(test_url),
            settings,
            response,
            success ? net::HTTP_OK : net::HTTP_INTERNAL_SERVER_ERROR)));
  }
}

// Explicitly generate required instantiations.
template void
DataReductionProxySettingsTestBase::SetProbeResult<DataReductionProxySettings>(
    const std::string& test_url,
    const std::string& response,
    bool success,
    int expected_calls);
template void
DataReductionProxySettingsTestBase::SetProbeResult<
    DataReductionProxySettingsAndroid>(const std::string& test_url,
                                       const std::string& response,
                                       bool success,
                                       int expected_calls);

void DataReductionProxySettingsTestBase::CheckProxyPref(
    const std::string& expected_servers,
    const std::string& expected_mode) {
  const DictionaryValue* dict = pref_service_.GetDictionary(prefs::kProxy);
  std::string mode;
  std::string server;
  dict->GetString("mode", &mode);
  ASSERT_EQ(expected_mode, mode);
  dict->GetString("server", &server);
  ASSERT_EQ(expected_servers, server);
}

void DataReductionProxySettingsTestBase::CheckProxyConfigs(
    bool expected_enabled) {
  if (expected_enabled) {
    std::string main_proxy = kDataReductionProxyOrigin;
    std::string fallback_proxy = kDataReductionProxyFallback;
    std::string servers =
        "http=" + main_proxy + "," + fallback_proxy + ",direct://;";
    CheckProxyPref(servers,
                   ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS));
  } else {
    CheckProxyPref(std::string(), ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
  }
}

void DataReductionProxySettingsTestBase::CheckProbe(
    bool initially_enabled,
    const std::string& probe_url,
    const std::string& response,
    bool request_success,
    bool expected_enabled) {
  pref_service_.SetBoolean(prefs::kSpdyProxyAuthEnabled, initially_enabled);
  settings_->disabled_by_carrier_ = false;
  SetProbeResult(
      probe_url, response, request_success, initially_enabled ? 1 : 0);
  settings_->MaybeActivateDataReductionProxy(false);
  base::MessageLoop::current()->RunUntilIdle();
  CheckProxyConfigs(expected_enabled);
}

void DataReductionProxySettingsTestBase::CheckProbeOnIPChange(
    const std::string& probe_url,
    const std::string& response,
    bool request_success,
    bool expected_enabled) {
  SetProbeResult(probe_url, response, request_success, 1);
  settings_->OnIPAddressChanged();
  base::MessageLoop::current()->RunUntilIdle();
  CheckProxyConfigs(expected_enabled);
}

void DataReductionProxySettingsTestBase::CheckOnPrefChange(
    bool enabled,
    const std::string& probe_url,
    const std::string& response,
    bool request_success,
    bool expected_enabled) {
  SetProbeResult(
      probe_url, response, request_success, expected_enabled ? 1 : 0);
  pref_service_.SetBoolean(prefs::kSpdyProxyAuthEnabled, enabled);
  base::MessageLoop::current()->RunUntilIdle();
  CheckProxyConfigs(expected_enabled);
}

void DataReductionProxySettingsTestBase::CheckInitDataReductionProxy(
    bool enabled_at_startup) {
  AddProxyToCommandLine();
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  pref_service_.SetBoolean(prefs::kSpdyProxyAuthEnabled, enabled_at_startup);
  SetProbeResult(
      kProbeURLWithOKResponse, "OK", true, enabled_at_startup ? 1 : 0);
  settings_->InitDataReductionProxySettings();
  base::MessageLoop::current()->RunUntilIdle();
  if (enabled_at_startup) {
    CheckProxyConfigs(enabled_at_startup);
  } else {
    // This presumes the proxy preference hadn't been set up by Chrome.
    CheckProxyPref(std::string(), std::string());
  }
}


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
    net::HttpAuthCache::Entry* entry = cache.LookupByPath(*it, std::string());
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
  EXPECT_EQ(kDataReductionProxyOrigin, result);
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
      switches::kSpdyProxyAuthFallback, kDataReductionProxyFallback);
  proxies = DataReductionProxySettings::GetDataReductionProxies();

  // So: if there weren't any proxies before, there still won't be.
  // If there were one or two, there will be two now.
  expected_proxy_size = expected_proxy_size == 0u ? 0u : 2u;

  EXPECT_EQ(expected_proxy_size, proxies.size());

  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
    switches::kSpdyProxyAuthOrigin, kDataReductionProxyOrigin);
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
  std::string salted_key = salt + kDataReductionProxyAuth + salt;
  base::string16 expected_hash = UTF8ToUTF16(base::MD5String(salted_key));
  EXPECT_EQ(expected_hash,
            DataReductionProxySettings::AuthHashForSalt(8675309));
}

// Test that the auth key set by preprocessor directive is not used
// when an origin is set via a switch. This test only does anything useful in
// Chrome builds.
TEST_F(DataReductionProxySettingsTest,
       TestAuthHashGenerationWithOriginSetViaSwitch) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSpdyProxyAuthOrigin, kDataReductionProxyOrigin);
  EXPECT_EQ(base::string16(),
            DataReductionProxySettings::AuthHashForSalt(8675309));
}


TEST_F(DataReductionProxySettingsTest, TestIsProxyEnabledOrManaged) {
  settings_->InitPrefMembers();
  EXPECT_FALSE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  pref_service_.SetBoolean(prefs::kSpdyProxyAuthEnabled, true);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  pref_service_.SetManagedPref(prefs::kSpdyProxyAuthEnabled,
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
        net::HostPortPair::FromString(kDataReductionProxyOrigin);
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
  settings_->GetContentLengths(spdyproxy::kNumDaysInHistory,
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
  settings_->GetContentLengths(spdyproxy::kNumDaysInHistory,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  const unsigned int days = spdyproxy::kNumDaysInHistory;
  // Received content length history values are 0 to |kNumDaysInHistory - 1|.
  int64 expected_total_received_content_length = (days - 1L) * days / 2;
  // Original content length history values are 0 to
  // |2 * (kNumDaysInHistory - 1)|.
  long expected_total_original_content_length = (days - 1L) * days;
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);
  EXPECT_EQ(last_update_time_.ToInternalValue(), last_update_time);

  // Request |kNumDaysInHistory - 1| days.
  settings_->GetContentLengths(spdyproxy::kNumDaysInHistory - 1,
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
      prefs::kSpdyProxyAuthEnabled,
      settings_->GetOriginalProfilePrefs());

  // TODO(bengr): Test enabling/disabling while a probe is outstanding.
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  // The proxy is enabled initially.
  // Request succeeded but with bad response, expect proxy to be disabled.
  CheckProbe(true, kProbeURLWithBadResponse, "Bad", true, false);
  // Request succeeded with valid response, expect proxy to be enabled.
  CheckProbe(true, kProbeURLWithOKResponse, "OK", true, true);
  // Request failed, expect proxy to be disabled.
  CheckProbe(true, kProbeURLWithNoResponse, "", false, false);
  // The proxy is disabled initially. Probes should not be emitted to change
  // state.
  CheckProbe(false, kProbeURLWithOKResponse, "OK", true, false);
}

TEST_F(DataReductionProxySettingsTest, TestOnIPAddressChanged) {
  AddProxyToCommandLine();
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  // The proxy is enabled initially.
  settings_->enabled_by_user_ = true;
  settings_->SetProxyConfigs(true, true);
  // IP address change triggers a probe that succeeds. Proxy remains enabled.
  CheckProbeOnIPChange(kProbeURLWithOKResponse, "OK", true, true);
  // IP address change triggers a probe that fails. Proxy is disabled.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "Bad", true, false);
  // IP address change triggers a probe that fails. Proxy remains disabled.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "Bad", true, false);
  // IP address change triggers a probe that succeed. Proxy is enabled.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "OK", true, true);
}

TEST_F(DataReductionProxySettingsTest, TestOnProxyEnabledPrefChange) {
  AddProxyToCommandLine();
  settings_->InitPrefMembers();
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  // The proxy is enabled initially.
  settings_->enabled_by_user_ = true;
  settings_->SetProxyConfigs(true, true);
  // The pref is disabled, so correspondingly should be the proxy.
  CheckOnPrefChange(false, kProbeURLWithOKResponse, "OK", true, false);
  // The pref is enabled, so correspondingly should be the proxy.
  CheckOnPrefChange(true, kProbeURLWithOKResponse, "OK", true, true);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOn) {
  CheckInitDataReductionProxy(true);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOff) {
  // InitDataReductionProxySettings with the preference off will directly call
  // LogProxyState.
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, LogProxyState(false, true)).Times(1);
  CheckInitDataReductionProxy(false);
}

TEST_F(DataReductionProxySettingsTest, TestGetDailyContentLengths) {
  DataReductionProxySettings::ContentLengthList result =
      settings_->GetDailyContentLengths(prefs::kDailyHttpOriginalContentLength);

  ASSERT_FALSE(result.empty());
  ASSERT_EQ(spdyproxy::kNumDaysInHistory, result.size());

  for (size_t i = 0; i < spdyproxy::kNumDaysInHistory; ++i) {
    long expected_length =
        static_cast<long>((spdyproxy::kNumDaysInHistory - 1 - i) * 2);
    ASSERT_EQ(expected_length, result[i]);
  }
}

TEST_F(DataReductionProxySettingsTest, TestBypassList) {
  settings_->AddHostPatternToBypass("http://www.google.com");
  settings_->AddHostPatternToBypass("fefe:13::abc/33");
  settings_->AddURLPatternToBypass("foo.org/images/*");
  settings_->AddURLPatternToBypass("http://foo.com/*");
  settings_->AddURLPatternToBypass("http://baz.com:22/bar/*");
  settings_->AddURLPatternToBypass("http://*bat.com/bar/*");

  std::string expected[] = {
    "http://www.google.com",
    "fefe:13::abc/33",
    "foo.org",
    "http://foo.com",
    "http://baz.com:22",
    "http://*bat.com"
  };

  ASSERT_EQ(settings_->bypass_rules_.size(), 6u);
  int i = 0;
  for (std::vector<std::string>::iterator it = settings_->bypass_rules_.begin();
       it != settings_->bypass_rules_.end(); ++it) {
    EXPECT_EQ(expected[i++], *it);
  }
}
