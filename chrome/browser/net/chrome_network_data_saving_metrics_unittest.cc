// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/net/chrome_network_data_saving_metrics.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(OS_ANDROID) || defined(OS_IOS)
const size_t kNumDaysInHistory = 60;

int64 GetListPrefInt64Value(
    const base::ListValue& list_update, size_t index) {
  std::string string_value;
  EXPECT_TRUE(list_update.GetString(index, &string_value));

  int64 value = 0;
  EXPECT_TRUE(base::StringToInt64(string_value, &value));
  return value;
}

#endif  // defined(OS_ANDROID) || defined(OS_IOS)

// Test UpdateContentLengthPrefs.
class ChromeNetworkDataSavingMetricsTest : public testing::Test {
 protected:
  ChromeNetworkDataSavingMetricsTest() {}

  virtual void SetUp() OVERRIDE {
    PrefRegistrySimple* registry = pref_service_.registry();
    registry->RegisterInt64Pref(prefs::kHttpReceivedContentLength, 0);
    registry->RegisterInt64Pref(prefs::kHttpOriginalContentLength, 0);

#if defined(OS_ANDROID) || defined(OS_IOS)
    registry->RegisterListPref(prefs::kDailyHttpOriginalContentLength);
    registry->RegisterListPref(prefs::kDailyHttpReceivedContentLength);
    registry->RegisterListPref(
        prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        prefs::kDailyContentLengthWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        prefs::kDailyOriginalContentLengthViaDataReductionProxy);
    registry->RegisterListPref(
        prefs::kDailyContentLengthViaDataReductionProxy);
    registry->RegisterInt64Pref(
        prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
  }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(ChromeNetworkDataSavingMetricsTest, TotalLengths) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;

  chrome_browser_net::UpdateContentLengthPrefs(
      kReceivedLength, kOriginalLength,
      false, false, &pref_service_);
  EXPECT_EQ(kReceivedLength,
            pref_service_.GetInt64(prefs::kHttpReceivedContentLength));
  EXPECT_EQ(kOriginalLength,
            pref_service_.GetInt64(prefs::kHttpOriginalContentLength));

  // Record the same numbers again, and total lengths should be dobuled.
  chrome_browser_net::UpdateContentLengthPrefs(
      kReceivedLength, kOriginalLength,
      false, false, &pref_service_);
  EXPECT_EQ(kReceivedLength * 2,
            pref_service_.GetInt64(prefs::kHttpReceivedContentLength));
  EXPECT_EQ(kOriginalLength * 2,
            pref_service_.GetInt64(prefs::kHttpOriginalContentLength));
}

#if defined(OS_ANDROID) || defined(OS_IOS)

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
    CreatePrefList(prefs::kDailyHttpOriginalContentLength);
    CreatePrefList(prefs::kDailyHttpReceivedContentLength);
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
      update->Insert(0, new StringValue(base::Int64ToString(0)));
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

  // Verify all daily pref list values.
  void VerifyDailyContentLengthPrefLists(
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
    VerifyPrefList(prefs::kDailyHttpOriginalContentLength,
                   original_values, original_count);
    VerifyPrefList(prefs::kDailyHttpReceivedContentLength,
                   received_values, received_count);
    VerifyPrefList(
        prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled,
        original_with_data_reduction_proxy_enabled_values,
        original_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        prefs::kDailyContentLengthWithDataReductionProxyEnabled,
        received_with_data_reduction_proxy_enabled_values,
        received_with_data_reduction_proxy_count);
    VerifyPrefList(
        prefs::kDailyOriginalContentLengthViaDataReductionProxy,
        original_via_data_reduction_proxy_values,
        original_via_data_reduction_proxy_count);
    VerifyPrefList(
        prefs::kDailyContentLengthViaDataReductionProxy,
        received_via_data_reduction_proxy_values,
        received_via_data_reduction_proxy_count);
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

  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  VerifyDailyContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, MultipleResponses) {
  const int64 kOriginalLength = 150;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      false, false, FakeNow(), &pref_service_);
  VerifyDailyContentLengthPrefLists(
      original, 1, received, 1,
      NULL, 0, NULL, 0, NULL, 0, NULL, 0);

  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, false, FakeNow(), &pref_service_);
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  int64 original_proxy_enabled[] = {kOriginalLength};
  int64 received_proxy_enabled[] = {kReceivedLength};
  VerifyDailyContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      NULL, 0, NULL, 0);

  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  original_proxy_enabled[0] += kOriginalLength;
  received_proxy_enabled[0] += kReceivedLength;
  int64 original_via_proxy[] = {kOriginalLength};
  int64 received_via_proxy[] = {kReceivedLength};
  VerifyDailyContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);

  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, false, FakeNow(), &pref_service_);
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  original_proxy_enabled[0] += kOriginalLength;
  received_proxy_enabled[0] += kReceivedLength;
  VerifyDailyContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);

  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      false, false, FakeNow(), &pref_service_);
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  VerifyDailyContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, ForwardOneDay) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;

  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);

  // Forward one day.
  SetFakeTimeDeltaInHours(24);

  // Proxy not enabled. Not via proxy.
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      false, false, FakeNow(), &pref_service_);

  int64 original[] = {kOriginalLength, kOriginalLength};
  int64 received[] = {kReceivedLength, kReceivedLength};
  int64 original_with_data_reduction_proxy_enabled[] = {kOriginalLength, 0};
  int64 received_with_data_reduction_proxy_enabled[] = {kReceivedLength, 0};
  int64 original_via_data_reduction_proxy[] = {kOriginalLength, 0};
  int64 received_via_data_reduction_proxy[] = {kReceivedLength, 0};
  VerifyDailyContentLengthPrefLists(
      original, 2,
      received, 2,
      original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2,
      received_via_data_reduction_proxy, 2);

  // Proxy enabled. Not via proxy.
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, false, FakeNow(), &pref_service_);
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kReceivedLength;
  VerifyDailyContentLengthPrefLists(
      original, 2,
      received, 2,
      original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2,
      received_via_data_reduction_proxy, 2);

  // Proxy enabled and via proxy.
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kReceivedLength;
  original_via_data_reduction_proxy[1] += kOriginalLength;
  received_via_data_reduction_proxy[1] += kReceivedLength;
  VerifyDailyContentLengthPrefLists(
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

  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  VerifyDailyContentLengthPrefLists(
      original, 2, received, 2,
      original, 2, received, 2,
      original, 2, received, 2);

  // Forward 10 hours, stay in the same day.
  // See kLastUpdateTime: "Now" in test is 03:45am.
  SetFakeTimeDeltaInHours(10);
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  VerifyDailyContentLengthPrefLists(
      original, 2, received, 2,
      original, 2, received, 2,
      original, 2, received, 2);

  // Forward 11 more hours, comes to tomorrow.
  AddFakeTimeDeltaInHours(11);
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  int64 original2[] = {kOriginalLength * 2, kOriginalLength};
  int64 received2[] = {kReceivedLength * 2, kReceivedLength};
  VerifyDailyContentLengthPrefLists(
      original2, 2, received2, 2,
      original2, 2, received2, 2,
      original2, 2, received2, 2);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, ForwardMultipleDays) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);

  // Forward three days.
  SetFakeTimeDeltaInHours(3 * 24);

  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);

  int64 original[] = {kOriginalLength, 0, 0, kOriginalLength};
  int64 received[] = {kReceivedLength, 0, 0, kReceivedLength};
  VerifyDailyContentLengthPrefLists(
      original, 4, received, 4,
      original, 4, received, 4,
      original, 4, received, 4);

  // Forward four more days.
  AddFakeTimeDeltaInHours(4 * 24);
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  int64 original2[] = {
    kOriginalLength, 0, 0, kOriginalLength, 0, 0, 0, kOriginalLength,
  };
  int64 received2[] = {
    kReceivedLength, 0, 0, kReceivedLength, 0, 0, 0, kReceivedLength,
  };
  VerifyDailyContentLengthPrefLists(
      original2, 8, received2, 8,
      original2, 8, received2, 8,
      original2, 8, received2, 8);

  // Forward |kNumDaysInHistory| more days.
  AddFakeTimeDeltaInHours(kNumDaysInHistory * 24);
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  int64 original3[] = {kOriginalLength};
  int64 received3[] = {kReceivedLength};
  VerifyDailyContentLengthPrefLists(
      original3, 1, received3, 1,
      original3, 1, received3, 1,
      original3, 1, received3, 1);

  // Forward |kNumDaysInHistory| + 1 more days.
  AddFakeTimeDeltaInHours((kNumDaysInHistory + 1)* 24);
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  VerifyDailyContentLengthPrefLists(
      original3, 1, received3, 1,
      original3, 1, received3, 1,
      original3, 1, received3, 1);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, BackwardAndForwardOneDay) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);

  // Backward one day.
  SetFakeTimeDeltaInHours(-24);
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  VerifyDailyContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);

  // Then, Forward one day
  AddFakeTimeDeltaInHours(24);
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  int64 original2[] = {kOriginalLength * 2, kOriginalLength};
  int64 received2[] = {kReceivedLength * 2, kReceivedLength};
  VerifyDailyContentLengthPrefLists(
      original2, 2, received2, 2,
      original2, 2, received2, 2,
      original2, 2, received2, 2);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, BackwardTwoDays) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  // Backward two days.
  SetFakeTimeDeltaInHours(-2 * 24);
  chrome_browser_net::UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, true, FakeNow(), &pref_service_);
  VerifyDailyContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

}  // namespace
