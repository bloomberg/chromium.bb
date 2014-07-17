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
}

template <class C>
void data_reduction_proxy::DataReductionProxySettingsTestBase::SetProbeResult(
    const std::string& test_url,
    const std::string& warmup_test_url,
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
    DataReductionProxySettingsAndroid>(bool allowed,
                                       bool fallback_allowed,
                                       bool alt_allowed,
                                       bool promo_allowed,
                                       bool holdback);

template void
data_reduction_proxy::DataReductionProxySettingsTestBase::SetProbeResult<
    DataReductionProxySettingsAndroid>(const std::string& test_url,
                                       const std::string& warmup_test_url,
                                       const std::string& response,
                                       ProbeURLFetchResult result,
                                       bool success,
                                       int expected_calls);

class DataReductionProxySettingsAndroidTest
    : public data_reduction_proxy::ConcreteDataReductionProxySettingsTest<
        DataReductionProxySettingsAndroid> {
 public:
  // DataReductionProxySettingsTest implementation:
  virtual void SetUp() OVERRIDE {
    env_ = base::android::AttachCurrentThread();
    DataReductionProxySettingsAndroid::Register(env_);
    DataReductionProxySettingsTestBase::SetUp();
  }

  DataReductionProxySettingsAndroid* Settings() {
    return static_cast<DataReductionProxySettingsAndroid*>(settings_.get());
  }

  JNIEnv* env_;
};

TEST_F(DataReductionProxySettingsAndroidTest, TestGetDataReductionProxyOrigin) {
  // SetUp() adds the origin to the command line, which should be returned here.
  ScopedJavaLocalRef<jstring> result =
      Settings()->GetDataReductionProxyOrigin(env_, NULL);
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
  ScopedJavaLocalRef<jstring> result =
      Settings()->GetDataReductionProxyOrigin(env_, NULL);
  ASSERT_TRUE(result.obj());
  const base::android::JavaRef<jstring>& str_ref = result;
  EXPECT_EQ(GURL(kDataReductionProxyDev),
            GURL(ConvertJavaStringToUTF8(str_ref)));
}

TEST_F(DataReductionProxySettingsAndroidTest, TestGetDailyContentLengths) {
  ScopedJavaLocalRef<jlongArray> result = Settings()->GetDailyContentLengths(
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
            (data_reduction_proxy::kNumDaysInHistory - 1 - i) * 2),
        value);
  }
}

