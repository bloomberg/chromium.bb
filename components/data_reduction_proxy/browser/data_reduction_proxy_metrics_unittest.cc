// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const size_t kNumDaysInHistory = 60;

int64 GetListPrefInt64Value(
    const base::ListValue& list_update, size_t index) {
  std::string string_value;
  EXPECT_TRUE(list_update.GetString(index, &string_value));

  int64 value = 0;
  EXPECT_TRUE(base::StringToInt64(string_value, &value));
  return value;
}

}  // namespace

namespace data_reduction_proxy {

// Test UpdateContentLengthPrefs.
class ChromeNetworkDataSavingMetricsTest : public testing::Test {
 protected:
  ChromeNetworkDataSavingMetricsTest() {}

  virtual void SetUp() OVERRIDE {
    PrefRegistrySimple* registry = pref_service_.registry();
    registry->RegisterInt64Pref(
        data_reduction_proxy::prefs::kHttpReceivedContentLength, 0);
    registry->RegisterInt64Pref(
        data_reduction_proxy::prefs::kHttpOriginalContentLength, 0);

    registry->RegisterListPref(data_reduction_proxy::prefs::
                                   kDailyHttpOriginalContentLength);
    registry->RegisterListPref(data_reduction_proxy::prefs::
                                   kDailyHttpReceivedContentLength);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthHttpsWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthShortBypassWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthLongBypassWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthUnknownWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxy);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthViaDataReductionProxy);
    registry->RegisterInt64Pref(
        data_reduction_proxy::prefs::
            kDailyHttpContentLengthLastUpdateDate, 0L);
  }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(ChromeNetworkDataSavingMetricsTest, TotalLengths) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;

  UpdateContentLengthPrefs(
      kReceivedLength, kOriginalLength,
      false, UNKNOWN_TYPE, &pref_service_);
  EXPECT_EQ(kReceivedLength,
            pref_service_.GetInt64(
                data_reduction_proxy::prefs::kHttpReceivedContentLength));
  EXPECT_EQ(kOriginalLength,
            pref_service_.GetInt64(
                data_reduction_proxy::prefs::kHttpOriginalContentLength));

  // Record the same numbers again, and total lengths should be dobuled.
  UpdateContentLengthPrefs(
      kReceivedLength, kOriginalLength,
      false, UNKNOWN_TYPE, &pref_service_);
  EXPECT_EQ(kReceivedLength * 2,
            pref_service_.GetInt64(
                data_reduction_proxy::prefs::kHttpReceivedContentLength));
  EXPECT_EQ(kOriginalLength * 2,
            pref_service_.GetInt64(
                data_reduction_proxy::prefs::kHttpOriginalContentLength));
}

// The initial last update time used in test. There is no leap second a few
// days around this time used in the test.
// Note: No time zone is specified. Local time will be assumed by
// base::Time::FromString below.
const char kLastUpdateTime[] = "Wed, 18 Sep 2013 03:45:26";

class ChromeNetworkDailyDataSavingMetricsTest
    : public ChromeNetworkDataSavingMetricsTest {
 protected:
  ChromeNetworkDailyDataSavingMetricsTest() {
    base::Time::FromString(kLastUpdateTime, &now_);
  }

  virtual void SetUp() OVERRIDE {
    ChromeNetworkDataSavingMetricsTest::SetUp();

    // Only create two lists in Setup to test that adding new lists is fine.
    CreatePrefList(
        data_reduction_proxy::prefs::kDailyHttpOriginalContentLength);
    CreatePrefList(
        data_reduction_proxy::prefs::kDailyHttpReceivedContentLength);
  }

  base::Time FakeNow() const {
    return now_ + now_delta_;
  }

  void SetFakeTimeDeltaInHours(int hours) {
    now_delta_ = base::TimeDelta::FromHours(hours);
  }

  void AddFakeTimeDeltaInHours(int hours) {
    now_delta_ += base::TimeDelta::FromHours(hours);
  }

  // Create daily pref list of |kNumDaysInHistory| zero values.
  void CreatePrefList(const char* pref) {
    ListPrefUpdate update(&pref_service_, pref);
    update->Clear();
    for (size_t i = 0; i < kNumDaysInHistory; ++i) {
      update->Insert(0, new base::StringValue(base::Int64ToString(0)));
    }
  }

  // Verify the pref list values are equal to the given values.
  // If the count of values is less than kNumDaysInHistory, zeros are assumed
  // at the beginning.
  void VerifyPrefList(const char* pref, const int64* values, size_t count) {
    ASSERT_GE(kNumDaysInHistory, count);
    ListPrefUpdate update(&pref_service_, pref);
    ASSERT_EQ(kNumDaysInHistory, update->GetSize()) << "Pref: " << pref;

    for (size_t i = 0; i < count; ++i) {
      EXPECT_EQ(
          values[i],
          GetListPrefInt64Value(*update, kNumDaysInHistory - count + i))
          << "index=" << (kNumDaysInHistory - count + i);
    }
    for (size_t i = 0; i < kNumDaysInHistory - count; ++i) {
      EXPECT_EQ(0, GetListPrefInt64Value(*update, i)) << "index=" << i;
    }
  }

  // Verify all daily data saving pref list values.
  void VerifyDailyDataSavingContentLengthPrefLists(
      const int64* original_values, size_t original_count,
      const int64* received_values, size_t received_count,
      const int64* original_with_data_reduction_proxy_enabled_values,
      size_t original_with_data_reduction_proxy_enabled_count,
      const int64* received_with_data_reduction_proxy_enabled_values,
      size_t received_with_data_reduction_proxy_count,
      const int64* original_via_data_reduction_proxy_values,
      size_t original_via_data_reduction_proxy_count,
      const int64* received_via_data_reduction_proxy_values,
      size_t received_via_data_reduction_proxy_count) {
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
                   original_values, original_count);
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpReceivedContentLength,
                   received_values, received_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabled,
        original_with_data_reduction_proxy_enabled_values,
        original_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabled,
        received_with_data_reduction_proxy_enabled_values,
        received_with_data_reduction_proxy_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxy,
        original_via_data_reduction_proxy_values,
        original_via_data_reduction_proxy_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthViaDataReductionProxy,
        received_via_data_reduction_proxy_values,
        received_via_data_reduction_proxy_count);
  }

  // Verify daily data saving pref for request types.
  void VerifyDailyRequestTypeContentLengthPrefLists(
      const int64* original_values, size_t original_count,
      const int64* received_values, size_t received_count,
      const int64* original_with_data_reduction_proxy_enabled_values,
      size_t original_with_data_reduction_proxy_enabled_count,
      const int64* received_with_data_reduction_proxy_enabled_values,
      size_t received_with_data_reduction_proxy_count,
      const int64* https_with_data_reduction_proxy_enabled_values,
      size_t https_with_data_reduction_proxy_enabled_count,
      const int64* short_bypass_with_data_reduction_proxy_enabled_values,
      size_t short_bypass_with_data_reduction_proxy_enabled_count,
      const int64* long_bypass_with_data_reduction_proxy_enabled_values,
      size_t long_bypass_with_data_reduction_proxy_enabled_count,
      const int64* unknown_with_data_reduction_proxy_enabled_values,
      size_t unknown_with_data_reduction_proxy_enabled_count) {
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
                   original_values, original_count);
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpReceivedContentLength,
                   received_values, received_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabled,
        original_with_data_reduction_proxy_enabled_values,
        original_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabled,
        received_with_data_reduction_proxy_enabled_values,
        received_with_data_reduction_proxy_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthHttpsWithDataReductionProxyEnabled,
        https_with_data_reduction_proxy_enabled_values,
        https_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthShortBypassWithDataReductionProxyEnabled,
        short_bypass_with_data_reduction_proxy_enabled_values,
        short_bypass_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthLongBypassWithDataReductionProxyEnabled,
        long_bypass_with_data_reduction_proxy_enabled_values,
        long_bypass_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthUnknownWithDataReductionProxyEnabled,
        unknown_with_data_reduction_proxy_enabled_values,
        unknown_with_data_reduction_proxy_enabled_count);
  }

 private:
  base::Time now_;
  base::TimeDelta now_delta_;
};

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, OneResponse) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, MultipleResponses) {
  const int64 kOriginalLength = 150;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      false, UNKNOWN_TYPE,
      FakeNow(), &pref_service_);
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      NULL, 0, NULL, 0, NULL, 0, NULL, 0);

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, UNKNOWN_TYPE,
      FakeNow(), &pref_service_);
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  int64 original_proxy_enabled[] = {kOriginalLength};
  int64 received_proxy_enabled[] = {kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      NULL, 0, NULL, 0);

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  original_proxy_enabled[0] += kOriginalLength;
  received_proxy_enabled[0] += kReceivedLength;
  int64 original_via_proxy[] = {kOriginalLength};
  int64 received_via_proxy[] = {kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, UNKNOWN_TYPE, FakeNow(), &pref_service_);
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  original_proxy_enabled[0] += kOriginalLength;
  received_proxy_enabled[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      false, UNKNOWN_TYPE, FakeNow(), &pref_service_);
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, RequestType) {
  const int64 kContentLength = 200;
  int64 received[] = {0};
  int64 https_received[] = {0};
  int64 total_received[] = {0};
  int64 proxy_enabled_received[] = {0};

  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      true, HTTPS,
      FakeNow(), &pref_service_);
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  https_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 0,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  // Data reduction proxy is not enabled.
  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      false, HTTPS,
      FakeNow(), &pref_service_);
  total_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 0,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      true, HTTPS,
      FakeNow(), &pref_service_);
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  https_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 0,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      true, SHORT_BYPASS,
      FakeNow(), &pref_service_);
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 1,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      true, LONG_BYPASS,
      FakeNow(), &pref_service_);
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,  // total
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 1,  // short bypass
      received, 1,  // long bypass
      received, 0);  // unknown

  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      true, UNKNOWN_TYPE,
      FakeNow(), &pref_service_);
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 1,  // short bypass
      received, 1,  // long bypass
      received, 1);  // unknown
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, ForwardOneDay) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);

  // Forward one day.
  SetFakeTimeDeltaInHours(24);

  // Proxy not enabled. Not via proxy.
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      false, UNKNOWN_TYPE, FakeNow(), &pref_service_);

  int64 original[] = {kOriginalLength, kOriginalLength};
  int64 received[] = {kReceivedLength, kReceivedLength};
  int64 original_with_data_reduction_proxy_enabled[] = {kOriginalLength, 0};
  int64 received_with_data_reduction_proxy_enabled[] = {kReceivedLength, 0};
  int64 original_via_data_reduction_proxy[] = {kOriginalLength, 0};
  int64 received_via_data_reduction_proxy[] = {kReceivedLength, 0};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2,
      received, 2,
      original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2,
      received_via_data_reduction_proxy, 2);

  // Proxy enabled. Not via proxy.
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, UNKNOWN_TYPE, FakeNow(), &pref_service_);
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2,
      received, 2,
      original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2,
      received_via_data_reduction_proxy, 2);

  // Proxy enabled and via proxy.
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kReceivedLength;
  original_via_data_reduction_proxy[1] += kOriginalLength;
  received_via_data_reduction_proxy[1] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2,
      received, 2,
      original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2,
      received_via_data_reduction_proxy, 2);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, PartialDayTimeChange) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {0, kOriginalLength};
  int64 received[] = {0, kReceivedLength};

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2,
      original, 2, received, 2,
      original, 2, received, 2);

  // Forward 10 hours, stay in the same day.
  // See kLastUpdateTime: "Now" in test is 03:45am.
  SetFakeTimeDeltaInHours(10);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2,
      original, 2, received, 2,
      original, 2, received, 2);

  // Forward 11 more hours, comes to tomorrow.
  AddFakeTimeDeltaInHours(11);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  int64 original2[] = {kOriginalLength * 2, kOriginalLength};
  int64 received2[] = {kReceivedLength * 2, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original2, 2, received2, 2,
      original2, 2, received2, 2,
      original2, 2, received2, 2);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, ForwardMultipleDays) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);

  // Forward three days.
  SetFakeTimeDeltaInHours(3 * 24);

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);

  int64 original[] = {kOriginalLength, 0, 0, kOriginalLength};
  int64 received[] = {kReceivedLength, 0, 0, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 4, received, 4,
      original, 4, received, 4,
      original, 4, received, 4);

  // Forward four more days.
  AddFakeTimeDeltaInHours(4 * 24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  int64 original2[] = {
    kOriginalLength, 0, 0, kOriginalLength, 0, 0, 0, kOriginalLength,
  };
  int64 received2[] = {
    kReceivedLength, 0, 0, kReceivedLength, 0, 0, 0, kReceivedLength,
  };
  VerifyDailyDataSavingContentLengthPrefLists(
      original2, 8, received2, 8,
      original2, 8, received2, 8,
      original2, 8, received2, 8);

  // Forward |kNumDaysInHistory| more days.
  AddFakeTimeDeltaInHours(kNumDaysInHistory * 24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  int64 original3[] = {kOriginalLength};
  int64 received3[] = {kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original3, 1, received3, 1,
      original3, 1, received3, 1,
      original3, 1, received3, 1);

  // Forward |kNumDaysInHistory| + 1 more days.
  AddFakeTimeDeltaInHours((kNumDaysInHistory + 1)* 24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  VerifyDailyDataSavingContentLengthPrefLists(
      original3, 1, received3, 1,
      original3, 1, received3, 1,
      original3, 1, received3, 1);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, BackwardAndForwardOneDay) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);

  // Backward one day.
  SetFakeTimeDeltaInHours(-24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);

  // Then, Forward one day
  AddFakeTimeDeltaInHours(24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  int64 original2[] = {kOriginalLength * 2, kOriginalLength};
  int64 received2[] = {kReceivedLength * 2, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original2, 2, received2, 2,
      original2, 2, received2, 2,
      original2, 2, received2, 2);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, BackwardTwoDays) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  // Backward two days.
  SetFakeTimeDeltaInHours(-2 * 24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), &pref_service_);
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);
}

}  // namespace data_reduction_proxy
