// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_experiments_stats.h"

#include <stdint.h>
#include <map>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_retrieval_params.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test value for how long a Data Reduction Proxy configuration should be valid.
const int64_t kConfigExpirationSeconds = 60 * 60 * 24;

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxyExperimentsStatsTest : public testing::Test {
 public:
  void SetPrefValue(const std::string& path, int64_t value) {}

 protected:
  void SetUp() override {
    test_context_ = DataReductionProxyTestContext::Builder().Build();
  }

  void SetConfigExperimentValues(
      bool enabled,
      const base::TimeDelta& retrieve_offset_from_now,
      int64_t roundtrip_milliseconds_base,
      double roundtrip_multiplier,
      int64_t roundtrip_milliseconds_increment,
      int64_t expiration_seconds,
      bool always_stale) {
    variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> variation_params;
    variation_params[kConfigRoundtripMillisecondsBaseParam] =
        base::Int64ToString(roundtrip_milliseconds_base);
    variation_params[kConfigRoundtripMultiplierParam] =
        base::DoubleToString(roundtrip_multiplier);
    variation_params[kConfigRoundtripMillisecondsIncrementParam] =
        base::Int64ToString(roundtrip_milliseconds_increment);
    variation_params[kConfigExpirationSecondsParam] =
        base::Int64ToString(expiration_seconds);
    variation_params[kConfigAlwaysStaleParam] = always_stale ? "1" : "0:";
    if (enabled) {
      ASSERT_TRUE(variations::AssociateVariationParams(
          kConfigFetchTrialGroup, "Enabled", variation_params));
    }
    pref_service()->SetInt64(
        prefs::kSimulatedConfigRetrieveTime,
        (base::Time::Now() + retrieve_offset_from_now).ToInternalValue());
  }

  PrefService* pref_service() { return test_context_->pref_service(); }

 private:
  base::MessageLoopForIO message_loop_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyExperimentsStatsTest, CheckHistogramsNoExperiments) {
  scoped_ptr<DataReductionProxyExperimentsStats> experiments_stats(
      new DataReductionProxyExperimentsStats(
          base::Bind(&DataReductionProxyExperimentsStatsTest::SetPrefValue,
                     base::Unretained(this))));
  experiments_stats->InitializeOnUIThread(
      scoped_ptr<DataReductionProxyConfigRetrievalParams>());
  base::HistogramTester histogram_tester;
  experiments_stats->RecordBytes(base::Time::Now(), VIA_DATA_REDUCTION_PROXY,
                                 500, 1000);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigFetchLostBytesOCL_0", 0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigFetchLostBytesCL_0", 0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigFetchLostBytesDiff_0", 0);
}

TEST_F(DataReductionProxyExperimentsStatsTest, CheckConfigHistograms) {
  struct {
    DataReductionProxyRequestType request_type;
    base::Time request_time;
    int64_t received_content_length;
    int64_t original_content_length;
    int64_t expected_received_content_length;
    int64_t expected_original_content_length;
    int64_t expected_diff;
  } test_cases[] = {
      {
       VIA_DATA_REDUCTION_PROXY,
       base::Time::Now() + base::TimeDelta::FromMinutes(5),
       500,
       1000,
       -1,
       -1,
       -1,
      },
      {
       VIA_DATA_REDUCTION_PROXY, base::Time::Now(), 500, 1000, 500, 1000, 500,
      },
      {
       HTTPS, base::Time::Now(), 500, 1000, -1, -1, -1,
      },
  };

  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(kConfigFetchTrialGroup, "Enabled");
  SetConfigExperimentValues(true, base::TimeDelta::FromHours(-2), 100, 2.0,
                            1000, kConfigExpirationSeconds, true);
  scoped_ptr<DataReductionProxyConfigRetrievalParams> config_params =
      DataReductionProxyConfigRetrievalParams::Create(pref_service());
  scoped_ptr<DataReductionProxyExperimentsStats> experiments_stats(
      new DataReductionProxyExperimentsStats(
          base::Bind(&DataReductionProxyExperimentsStatsTest::SetPrefValue,
                     base::Unretained(this))));
  experiments_stats->InitializeOnUIThread(std::move(config_params));
  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    experiments_stats->RecordBytes(
        test_case.request_time, test_case.request_type,
        test_case.received_content_length, test_case.original_content_length);
    if (test_case.expected_received_content_length == -1) {
      histogram_tester.ExpectTotalCount(
          "DataReductionProxy.ConfigFetchLostBytesCL_0", 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          "DataReductionProxy.ConfigFetchLostBytesCL_0",
          test_case.expected_received_content_length, 1);
    }

    if (test_case.expected_original_content_length == -1) {
      histogram_tester.ExpectTotalCount(
          "DataReductionProxy.ConfigFetchLostBytesOCL_0", 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          "DataReductionProxy.ConfigFetchLostBytesOCL_0",
          test_case.expected_original_content_length, 1);
    }

    if (test_case.expected_diff == -1) {
      histogram_tester.ExpectTotalCount(
          "DataReductionProxy.ConfigFetchLostBytesDiff_0", 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          "DataReductionProxy.ConfigFetchLostBytesDiff_0",
          test_case.expected_diff, 1);
    }
  }
}

}  // namespace data_reduction_proxy
