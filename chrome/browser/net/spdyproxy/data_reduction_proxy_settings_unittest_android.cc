// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/pref_names.h"
#include "components/variations/entropy_provider.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

const char kDataReductionProxyOrigin[] = "https://foo:443/";
const char kDataReductionProxyOriginHostPort[] = "foo:443";
const char kDataReductionProxyAuth[] = "12345";

const char kProbeURLWithOKResponse[] = "http://ok.org/";
const char kProbeURLWithBadResponse[] = "http://bad.org/";
const char kProbeURLWithNoResponse[] = "http://no.org/";

class TestDataReductionProxySettingsAndroid
    : public DataReductionProxySettingsAndroid {
 public:
  TestDataReductionProxySettingsAndroid(JNIEnv* env, jobject obj,
                                        PrefService* profile_prefs,
                                        PrefService* local_state_prefs)
    : DataReductionProxySettingsAndroid(env, obj),
      success_(false),
      fake_fetcher_request_count_(0),
      profile_prefs_(profile_prefs),
      local_state_prefs_(local_state_prefs) {
  }

  // DataReductionProxySettingsAndroid implementation:
  virtual net::URLFetcher* GetURLFetcher() OVERRIDE {
    if (test_url_.empty())
      return NULL;
    net::URLFetcher* fetcher = new net::FakeURLFetcher(GURL(test_url_), this,
                                                       response_, success_);
    fake_fetcher_request_count_++;
    return fetcher;
  }

  virtual PrefService* GetOriginalProfilePrefs() OVERRIDE {
    return profile_prefs_;
  }
  virtual PrefService* GetLocalStatePrefs() OVERRIDE {
    return local_state_prefs_;
  }

  void set_probe_result(const std::string& test_url,
                        const std::string& response,
                        bool success) {
    test_url_ = test_url;
    response_ = response;
    success_ = success;
  }

  const int fake_fetcher_request_count() {
    return fake_fetcher_request_count_;
  }

 private:
  std::string test_url_;
  std::string response_;
  bool success_;
  int fake_fetcher_request_count_;
  PrefService* profile_prefs_;
  PrefService* local_state_prefs_;
};

class DataReductionProxySettingsAndroidTest : public testing::Test {
 protected:
  void AddProxyToCommandLine() {
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSpdyProxyAuthOrigin, kDataReductionProxyOrigin);
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSpdyProxyAuthValue, kDataReductionProxyAuth);
  }

  // testing::Test implementation:
  virtual void SetUp() OVERRIDE {
    env_ = base::android::AttachCurrentThread();
    DataReductionProxySettingsAndroid::Register(env_);
    PrefRegistrySimple* registry = pref_service_.registry();
    registry->RegisterListPref(prefs::kDailyHttpOriginalContentLength);
    registry->RegisterListPref(prefs::kDailyHttpReceivedContentLength);
    registry->RegisterInt64Pref(
        prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
    registry->RegisterDictionaryPref(prefs::kProxy);
    registry->RegisterBooleanPref(prefs::kSpdyProxyAuthEnabled, false);
    registry->RegisterBooleanPref(prefs::kSpdyProxyAuthWasEnabledBefore, false);
    settings_.reset(new TestDataReductionProxySettingsAndroid(NULL, NULL,
                                                              &pref_service_,
                                                              &pref_service_));
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

  void CheckProxyPref(const std::string& expected_pac_url,
                      const std::string& expected_mode) {
    const DictionaryValue* dict = pref_service_.GetDictionary(prefs::kProxy);
    std::string mode;
    std::string pac_url;
    dict->GetString("mode", &mode);
    ASSERT_EQ(expected_mode, mode);
    dict->GetString("pac_url", &pac_url);
    ASSERT_EQ(expected_pac_url, pac_url);
  }

  void CheckProxyPac(bool expected_enabled) {
    if (expected_enabled) {
      std::string pac;
      base::Base64Encode(settings_->GetProxyPacScript(), &pac);
      std::string expected_pac_url =
          "data:application/x-ns-proxy-autoconfig;base64," + pac;
      CheckProxyPref(expected_pac_url,
                     ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));
    } else {
      CheckProxyPref(std::string(), ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
    }
  }

  void CheckProbe(bool initially_enabled, const std::string& probe_url,
                  const std::string& response, bool request_success,
                  bool expected_enabled) {
    pref_service_.SetBoolean(prefs::kSpdyProxyAuthEnabled, initially_enabled);
    settings_->set_probe_result(probe_url, response, request_success);
    settings_->MaybeActivateDataReductionProxy(false);
    base::MessageLoop::current()->RunUntilIdle();
    CheckProxyPac(expected_enabled);
  }

  void CheckProbeOnIPChange(const std::string& probe_url,
                            const std::string& response,
                            bool request_success,
                            bool expected_enabled) {
    settings_->set_probe_result(probe_url, response, request_success);
    settings_->OnIPAddressChanged();
    base::MessageLoop::current()->RunUntilIdle();
    CheckProxyPac(expected_enabled);
  }

  void CheckOnPrefChange(bool enabled, const std::string& probe_url,
                         const std::string& response, bool request_success,
                         bool expected_enabled) {
    settings_->set_probe_result(probe_url, response, request_success);
    pref_service_.SetBoolean(prefs::kSpdyProxyAuthEnabled, enabled);
    base::MessageLoop::current()->RunUntilIdle();
    CheckProxyPac(expected_enabled);
  }

  void CheckInitDataReductionProxy(bool enabled_at_startup) {
    AddProxyToCommandLine();
    base::MessageLoop loop(base::MessageLoop::TYPE_UI);
    pref_service_.SetBoolean(prefs::kSpdyProxyAuthEnabled, enabled_at_startup);
    settings_->set_probe_result(kProbeURLWithOKResponse, "OK", true);
    settings_->InitDataReductionProxySettings(NULL, NULL);
    base::MessageLoop::current()->RunUntilIdle();
    if (enabled_at_startup) {
      CheckProxyPac(enabled_at_startup);
    } else {
      // This presumes the proxy pref hadn't been set up by Chrome.
      CheckProxyPref(std::string(), std::string());
    }
  }

  TestingPrefServiceSimple pref_service_;
  scoped_ptr<TestDataReductionProxySettingsAndroid> settings_;
  base::Time last_update_time_;
  // This is a singleton that will clear all set field trials on destruction.
  scoped_ptr<base::FieldTrialList> field_trial_list_;
  JNIEnv* env_;
};

TEST_F(DataReductionProxySettingsAndroidTest, TestGetDataReductionProxyOrigin) {
  AddProxyToCommandLine();
  // SetUp() adds the origin to the command line, which should be returned here.
  ScopedJavaLocalRef<jstring> result =
      settings_->GetDataReductionProxyOrigin(env_, NULL);
  ASSERT_TRUE(result.obj());
  const base::android::JavaRef<jstring>& str_ref = result;
  EXPECT_EQ(kDataReductionProxyOrigin, ConvertJavaStringToUTF8(str_ref));
}

TEST_F(DataReductionProxySettingsAndroidTest, TestGetDataReductionProxyAuth) {
  AddProxyToCommandLine();
  // SetUp() adds the auth string to the command line, which should be returned
  // here.
  ScopedJavaLocalRef<jstring> result =
      settings_->GetDataReductionProxyAuth(env_, NULL);
  ASSERT_TRUE(result.obj());
  const base::android::JavaRef<jstring>& str_ref = result;
  EXPECT_EQ(kDataReductionProxyAuth, ConvertJavaStringToUTF8(str_ref));
}

// Test that the auth value set by preprocessor directive is not returned
// when an origin is set via a switch. This test only does anything useful in
// Chrome builds.
TEST_F(DataReductionProxySettingsAndroidTest,
       TestGetDataReductionProxyAuthWithOriginSetViaSwitch) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSpdyProxyAuthOrigin, kDataReductionProxyOrigin);
  // SetUp() adds the auth string to the command line, which should be returned
  // here.
  ScopedJavaLocalRef<jstring> result =
      settings_->GetDataReductionProxyAuth(env_, NULL);
  ASSERT_TRUE(result.obj());
  const base::android::JavaRef<jstring>& str_ref = result;
  EXPECT_EQ(std::string(), ConvertJavaStringToUTF8(str_ref));
}

// Confirm that the bypass rule functions generate the intended JavaScript
// code for the Proxy PAC.
TEST_F(DataReductionProxySettingsAndroidTest, TestBypassRules) {
  settings_->AddURLPatternToBypass("http://foo.com/*");
  settings_->AddHostPatternToBypass("bar.com");
  settings_->AddHostToBypass("127.0.0.1");

  std::string expected[] = {
      "shExpMatch(url, 'http://foo.com/*')",
      "shExpMatch(host, 'bar.com')",
      "host == '127.0.0.1'"
  };

  int i = 0;
  for (std::vector<std::string>::iterator it = settings_->bypass_rules_.begin();
       it != settings_->bypass_rules_.end(); ++it) {
    EXPECT_EQ(expected[i++], *it);
  }
}

TEST_F(DataReductionProxySettingsAndroidTest, TestIsProxyEnabledOrManaged) {
  settings_->InitPrefMembers();
  EXPECT_FALSE(settings_->IsDataReductionProxyEnabled(NULL, NULL));
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged(NULL, NULL));

  pref_service_.SetBoolean(prefs::kSpdyProxyAuthEnabled, true);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled(NULL, NULL));
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged(NULL, NULL));

  pref_service_.SetManagedPref(prefs::kSpdyProxyAuthEnabled,
                               base::Value::CreateBooleanValue(true));
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled(NULL, NULL));
  EXPECT_TRUE(settings_->IsDataReductionProxyManaged(NULL, NULL));
}

TEST_F(DataReductionProxySettingsAndroidTest, TestSetProxyPac) {
  settings_->AddDefaultProxyBypassRules();
  std::string pac;
  base::Base64Encode(settings_->GetProxyPacScript(), &pac);
  std::string expected_pac_url =
      "data:application/x-ns-proxy-autoconfig;base64," + pac;
  // Test setting the PAC, without generating histograms.
  settings_->has_turned_on_ = true;
  settings_->SetProxyPac(true, false);
  CheckProxyPref(expected_pac_url,
                 ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));

  // Test disabling the PAC, without generating histograms.
  settings_->has_turned_off_ = true;
  settings_->SetProxyPac(false, false);
  CheckProxyPref(std::string(), ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
}

TEST_F(DataReductionProxySettingsAndroidTest, TestGetDailyContentLengths) {
  ScopedJavaLocalRef<jlongArray> result =
      settings_->GetDailyContentLengths(env_,
                                        prefs::kDailyHttpOriginalContentLength);
  ASSERT_TRUE(result.obj());

  jsize java_array_len = env_->GetArrayLength(result.obj());
  ASSERT_EQ(static_cast<jsize>(spdyproxy::kNumDaysInHistory), java_array_len);

  jlong value;
  for (size_t i = 0; i < spdyproxy::kNumDaysInHistory; ++i) {
    env_->GetLongArrayRegion(result.obj(), i, 1, &value);
    ASSERT_EQ(
        static_cast<long>((spdyproxy::kNumDaysInHistory - 1 - i) * 2), value);
  }
}

TEST_F(DataReductionProxySettingsAndroidTest,
       TestResetDataReductionStatistics) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_time;
  settings_->ResetDataReductionStatistics();
  settings_->GetContentLengthsInternal(spdyproxy::kNumDaysInHistory,
                                       &original_content_length,
                                       &received_content_length,
                                       &last_update_time);
  EXPECT_EQ(0L, original_content_length);
  EXPECT_EQ(0L, received_content_length);
  EXPECT_EQ(last_update_time_.ToInternalValue(), last_update_time);
}

TEST_F(DataReductionProxySettingsAndroidTest, TestContentLengthsInternal) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_time;

  // Request |kNumDaysInHistory| days.
  settings_->GetContentLengthsInternal(spdyproxy::kNumDaysInHistory,
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
  settings_->GetContentLengthsInternal(spdyproxy::kNumDaysInHistory - 1,
                                       &original_content_length,
                                       &received_content_length,
                                       &last_update_time);
  expected_total_received_content_length -= (days - 1);
  expected_total_original_content_length -= 2 * (days - 1);
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);

  // Request 0 days.
  settings_->GetContentLengthsInternal(0,
                                       &original_content_length,
                                       &received_content_length,
                                       &last_update_time);
  expected_total_received_content_length = 0;
  expected_total_original_content_length = 0;
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);

  // Request 1 day. First day had 0 bytes so should be same as 0 days.
  settings_->GetContentLengthsInternal(1,
                                       &original_content_length,
                                       &received_content_length,
                                       &last_update_time);
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);
}

TEST_F(DataReductionProxySettingsAndroidTest,
       TestMaybeActivateDataReductionProxy) {
  AddProxyToCommandLine();
  settings_->InitPrefMembers();
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
  EXPECT_EQ(3, settings_->fake_fetcher_request_count());
  CheckProbe(false, kProbeURLWithOKResponse, "OK", true, false);
  EXPECT_EQ(3, settings_->fake_fetcher_request_count());
}

TEST_F(DataReductionProxySettingsAndroidTest,
       TestOnIPAddressChanged) {
  AddProxyToCommandLine();
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  // The proxy is enabled initially.
  settings_->enabled_by_user_ = true;
  settings_->SetProxyPac(true, true);
  // IP address change triggers a probe that succeeds. Proxy remains enabled.
  CheckProbeOnIPChange(kProbeURLWithOKResponse, "OK", true, true);
  // IP address change triggers a probe that fails. Proxy is disabled.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "Bad", true, false);
  // IP address change triggers a probe that fails. Proxy remains disabled.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "Bad", true, false);
  // IP address change triggers a probe that succeed. Proxy is enabled.
  CheckProbeOnIPChange(kProbeURLWithBadResponse, "OK", true, true);
  EXPECT_EQ(4, settings_->fake_fetcher_request_count());
}

TEST_F(DataReductionProxySettingsAndroidTest,
       TestOnProxyEnabledPrefChange) {
  AddProxyToCommandLine();
  settings_->InitPrefMembers();
  base::MessageLoop loop(base::MessageLoop::TYPE_UI);
  LOG(WARNING) << "Before init pref members";
  // The proxy is enabled initially.
  settings_->enabled_by_user_ = true;
  settings_->SetProxyPac(true, true);
  LOG(WARNING) << "after set proxy pac";
  // The pref is disabled, so correspondingly should be the proxy.
  CheckOnPrefChange(false, kProbeURLWithOKResponse, "OK", true, false);
  // The pref is enabled, so correspondingly should be the proxy.
  CheckOnPrefChange(true, kProbeURLWithOKResponse, "OK", true, true);
  EXPECT_EQ(1, settings_->fake_fetcher_request_count());
}

TEST_F(DataReductionProxySettingsAndroidTest,
       TestInitDataReductionProxyOn) {
  CheckInitDataReductionProxy(true);
}

TEST_F(DataReductionProxySettingsAndroidTest,
       TestInitDataReductionProxyOff) {
  CheckInitDataReductionProxy(false);
}

