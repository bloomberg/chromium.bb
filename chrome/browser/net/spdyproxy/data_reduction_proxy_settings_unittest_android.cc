// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

const char kDataReductionProxyDev[] = "http://foo-dev.com:80";

using data_reduction_proxy::DataReductionProxySettings;

// Used for testing the DataReductionProxySettingsAndroid class.
class TestDataReductionProxySettingsAndroid
    : public DataReductionProxySettingsAndroid {
 public:
  // Constructs an Android settings object for test that wraps the provided
  // settings object.
  explicit TestDataReductionProxySettingsAndroid(
      DataReductionProxySettings* settings)
      : DataReductionProxySettingsAndroid(),
        settings_(settings) {}

  // Returns the provided setting object. Used by wrapping methods.
  virtual DataReductionProxySettings* Settings() OVERRIDE {
    return settings_;
  }

  // The wrapped settings object.
  DataReductionProxySettings* settings_;
};

template <class C>
void data_reduction_proxy::DataReductionProxySettingsTestBase::ResetSettings(
    bool allowed,
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
  MockDataReductionProxySettings<C>* settings =
      new MockDataReductionProxySettings<C>(flags);
  EXPECT_CALL(*settings, GetOriginalProfilePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(&pref_service_));
  EXPECT_CALL(*settings, GetLocalStatePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(&pref_service_));
  EXPECT_CALL(*settings, GetURLFetcherForAvailabilityCheck()).Times(0);
  EXPECT_CALL(*settings, LogProxyState(_, _, _)).Times(0);
  settings_.reset(settings);
  settings_->SetDataReductionProxyStatisticsPrefs(statistics_prefs_.get());
}

template <class C>
void data_reduction_proxy::DataReductionProxySettingsTestBase::SetProbeResult(
    const std::string& test_url,
    const std::string& response,
    ProbeURLFetchResult result,
    bool success,
    int expected_calls)  {
  MockDataReductionProxySettings<C>* settings =
      static_cast<MockDataReductionProxySettings<C>*>(settings_.get());
  if (0 == expected_calls) {
    EXPECT_CALL(*settings, GetURLFetcherForAvailabilityCheck()).Times(0);
    EXPECT_CALL(*settings, RecordProbeURLFetchResult(_)).Times(0);
  } else {
    EXPECT_CALL(*settings, RecordProbeURLFetchResult(result)).Times(1);
    EXPECT_CALL(*settings, GetURLFetcherForAvailabilityCheck())
        .Times(expected_calls)
        .WillRepeatedly(Return(new net::FakeURLFetcher(
            GURL(test_url),
            settings,
            response,
            success ? net::HTTP_OK : net::HTTP_INTERNAL_SERVER_ERROR,
            success ? net::URLRequestStatus::SUCCESS :
                      net::URLRequestStatus::FAILED)));
  }
}

template void
data_reduction_proxy::DataReductionProxySettingsTestBase::ResetSettings<
    DataReductionProxyChromeSettings>(bool allowed,
                                       bool fallback_allowed,
                                       bool alt_allowed,
                                       bool promo_allowed,
                                       bool holdback);

template void
data_reduction_proxy::DataReductionProxySettingsTestBase::SetProbeResult<
    DataReductionProxyChromeSettings>(const std::string& test_url,
                                       const std::string& response,
                                       ProbeURLFetchResult result,
                                       bool success,
                                       int expected_calls);

class DataReductionProxySettingsAndroidTest
    : public data_reduction_proxy::ConcreteDataReductionProxySettingsTest<
          DataReductionProxyChromeSettings> {
 public:
  // DataReductionProxySettingsTest implementation:
  virtual void SetUp() OVERRIDE {
    env_ = base::android::AttachCurrentThread();
    DataReductionProxySettingsAndroid::Register(env_);
    DataReductionProxySettingsTestBase::SetUp();
    ResetSettingsAndroid();
  }

  void ResetSettingsAndroid() {
    settings_android_.reset(new TestDataReductionProxySettingsAndroid(
        settings_.get()));
  }

  DataReductionProxySettings* Settings() {
    return settings_.get();
  }

  DataReductionProxySettingsAndroid* SettingsAndroid() {
    return settings_android_.get();
  }

  scoped_ptr<DataReductionProxySettingsAndroid> settings_android_;
  JNIEnv* env_;
};

TEST_F(DataReductionProxySettingsAndroidTest, TestGetDataReductionProxyOrigin) {
  // SetUp() adds the origin to the command line, which should be returned here.
  ScopedJavaLocalRef<jstring> result =
      SettingsAndroid()->GetDataReductionProxyOrigin(env_, NULL);
  ASSERT_TRUE(result.obj());
  const base::android::JavaRef<jstring>& str_ref = result;
  EXPECT_EQ(GURL(expected_params_->DefaultOrigin()),
            GURL(ConvertJavaStringToUTF8(str_ref)));
}

TEST_F(DataReductionProxySettingsAndroidTest,
       TestGetDataReductionProxyDevOrigin) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyDev,
      kDataReductionProxyDev);
  ResetSettings(true, true, false, true, false);
  ResetSettingsAndroid();
  ScopedJavaLocalRef<jstring> result =
      SettingsAndroid()->GetDataReductionProxyOrigin(env_, NULL);
  ASSERT_TRUE(result.obj());
  const base::android::JavaRef<jstring>& str_ref = result;
  EXPECT_EQ(GURL(kDataReductionProxyDev),
            GURL(ConvertJavaStringToUTF8(str_ref)));
}

TEST_F(DataReductionProxySettingsAndroidTest, TestGetDailyContentLengths) {
  ScopedJavaLocalRef<jlongArray> result =
      SettingsAndroid()->GetDailyContentLengths(
          env_, data_reduction_proxy::prefs::kDailyHttpOriginalContentLength);
  ASSERT_TRUE(result.obj());

  jsize java_array_len = env_->GetArrayLength(result.obj());
  ASSERT_EQ(static_cast<jsize>(data_reduction_proxy::kNumDaysInHistory),
            java_array_len);

  jlong value;
  for (size_t i = 0; i < data_reduction_proxy::kNumDaysInHistory; ++i) {
    env_->GetLongArrayRegion(result.obj(), i, 1, &value);
    ASSERT_EQ(
        static_cast<long>(
            (data_reduction_proxy::kNumDaysInHistory - 1 - i) * 2), value);
  }
}

