// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_unittest.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/pref_names.h"
#include "components/variations/entropy_provider.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

const char kDataReductionProxyOrigin[] = "https://foo.com:443/";
const char kDataReductionProxyOriginPAC[] = "HTTPS foo.com:443;";
const char kDataReductionProxyFallbackPAC[] = "HTTP bar.com:80;";

class DataReductionProxySettingsAndroidTest
    : public ConcreteDataReductionProxySettingsTest<
        DataReductionProxySettingsAndroid> {
 public:
  // DataReductionProxySettingsTest implementation:
  virtual void SetUp() OVERRIDE {
    env_ = base::android::AttachCurrentThread();
    DataReductionProxySettingsAndroid::Register(env_);
    DataReductionProxySettingsTestBase::SetUp();
  }

  void CheckProxyPacPref(const std::string& expected_pac_url,
                         const std::string& expected_mode) {
    const DictionaryValue* dict = pref_service_.GetDictionary(prefs::kProxy);
    std::string mode;
    std::string pac_url;
    dict->GetString("mode", &mode);
    ASSERT_EQ(expected_mode, mode);
    dict->GetString("pac_url", &pac_url);
    ASSERT_EQ(expected_pac_url, pac_url);
  }

  DataReductionProxySettingsAndroid* Settings() {
    return static_cast<DataReductionProxySettingsAndroid*>(settings_.get());
  }

  JNIEnv* env_;
};

TEST_F(DataReductionProxySettingsAndroidTest, TestGetDataReductionProxyOrigin) {
  AddProxyToCommandLine();
  // SetUp() adds the origin to the command line, which should be returned here.
  ScopedJavaLocalRef<jstring> result =
      Settings()->GetDataReductionProxyOrigin(env_, NULL);
  ASSERT_TRUE(result.obj());
  const base::android::JavaRef<jstring>& str_ref = result;
  EXPECT_EQ(kDataReductionProxyOrigin, ConvertJavaStringToUTF8(str_ref));
}

// Confirm that the bypass rule functions generate the intended JavaScript
// code for the Proxy PAC.
TEST_F(DataReductionProxySettingsAndroidTest, TestBypassPACRules) {
  Settings()->AddURLPatternToBypass("http://foo.com/*");
  Settings()->AddHostPatternToBypass("bar.com");

  EXPECT_EQ(Settings()->pac_bypass_rules_.size(), 1u);
  EXPECT_EQ("shExpMatch(url, 'http://foo.com/*')",
            Settings()->pac_bypass_rules_[0]);

  EXPECT_EQ(Settings()->BypassRules().size(), 1u);
  EXPECT_EQ("bar.com", Settings()->BypassRules()[0]);
}

TEST_F(DataReductionProxySettingsAndroidTest, TestSetProxyPac) {
  AddProxyToCommandLine();
  Settings()->AddDefaultProxyBypassRules();
  std::string raw_pac = Settings()->GetProxyPacScript();
  EXPECT_NE(raw_pac.find(kDataReductionProxyOriginPAC), std::string::npos);
  EXPECT_NE(raw_pac.find(kDataReductionProxyFallbackPAC), std::string::npos);;
  std::string pac;
  base::Base64Encode(raw_pac, &pac);
  std::string expected_pac_url =
      "data:application/x-ns-proxy-autoconfig;base64," + pac;
  // Test setting the PAC, without generating histograms.
  Settings()->SetHasTurnedOn();
  Settings()->SetProxyConfigs(true, false);
  CheckProxyPacPref(expected_pac_url,
                    ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));

  // Test disabling the PAC, without generating histograms.
  Settings()->SetHasTurnedOff();
  Settings()->SetProxyConfigs(false, false);
  CheckProxyPacPref(std::string(), ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
}

TEST_F(DataReductionProxySettingsAndroidTest, TestGetDailyContentLengths) {
  ScopedJavaLocalRef<jlongArray> result = Settings()->GetDailyContentLengths(
        env_, prefs::kDailyHttpOriginalContentLength);
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

