// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_retrieval_params.h"

#include <stdint.h>

#include <map>

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test value for how long a Data Reduction Proxy configuration should be valid.
const int64_t kConfigExpirationSeconds = 60 * 60 * 24;

// Checks that |actual| falls in the range of |min| + |delta| and
// |max| + |delta|.
void ExpectInTimeRange(const base::Time& min,
                       const base::Time& max,
                       const base::TimeDelta& delta,
                       const base::Time& actual) {
  EXPECT_GE(actual, min + delta);
  EXPECT_LE(actual, max + delta);
}

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxyConfigRetrievalParamsTest : public testing::Test {
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

TEST_F(DataReductionProxyConfigRetrievalParamsTest, ExpectedVariations) {
  struct {
    bool experiment_enabled;
    base::TimeDelta retrieve_offset_from_now;
    int64_t expiration_seconds;
    int64_t roundtrip_milliseconds_base;
    double roundtrip_multiplier;
    int64_t roundtrip_milliseconds_increment;
    bool always_stale;
    bool expected_has_variations;
    base::TimeDelta expected_config_expiration_from_now;
    base::TimeDelta expected_delta_0;
    base::TimeDelta expected_delta_2;
    base::TimeDelta expected_delta_5;
  } test_cases[] = {
      // Experiment is disabled.
      {false,
       base::TimeDelta::FromHours(-2),
       kConfigExpirationSeconds,
       100,
       2.0,
       100,
       false,
       false,
       base::TimeDelta(),
       base::TimeDelta(),
       base::TimeDelta(),
       base::TimeDelta()},
      // Experiment is enabled, but not marked always stale and the last
      // retrieval is such that the configuration is still valid.
      {true,
       base::TimeDelta::FromHours(-2),
       kConfigExpirationSeconds,
       100,
       2.0,
       100,
       false,
       false,
       base::TimeDelta::FromHours(22),
       base::TimeDelta(),
       base::TimeDelta(),
       base::TimeDelta()},
      // Experiment is enabled, but not marked always stale with a retrieval
      // time sufficiently in the past such that the configuration is expired.
      {
       true,
       base::TimeDelta::FromHours(-26),
       kConfigExpirationSeconds,
       100,
       2.0,
       100,
       true,
       true,
       base::TimeDelta::FromSeconds(kConfigExpirationSeconds),
       base::TimeDelta::FromMilliseconds(100),
       base::TimeDelta::FromMilliseconds(700),
       base::TimeDelta::FromMilliseconds(6300),
      },
      // Experiment is enabled, marked always stale and the retrieval time is
      // recent enough that a configuration would be still valid.
      {
       true,
       base::TimeDelta::FromHours(-2),
       kConfigExpirationSeconds,
       100,
       2.0,
       100,
       true,
       true,
       base::TimeDelta::FromSeconds(kConfigExpirationSeconds),
       base::TimeDelta::FromMilliseconds(100),
       base::TimeDelta::FromMilliseconds(700),
       base::TimeDelta::FromMilliseconds(6300),
      },
      // Experiment is enabled but values are invalid.
      {
       true,
       base::TimeDelta::FromHours(-2),
       -400,
       -500,
       0.5,
       -300,
       true,
       true,
       base::TimeDelta::FromSeconds(kConfigExpirationSeconds),
       base::TimeDelta::FromMilliseconds(100),
       base::TimeDelta::FromMilliseconds(300),
       base::TimeDelta::FromMilliseconds(600),
      },
      // Experiment is enabled, but the last config retrieval time is in the
      // future and thus ignored.
      {
       true,
       base::TimeDelta::FromHours(26),
       kConfigExpirationSeconds,
       100,
       2.0,
       100,
       true,
       true,
       base::TimeDelta::FromSeconds(kConfigExpirationSeconds),
       base::TimeDelta::FromMilliseconds(100),
       base::TimeDelta::FromMilliseconds(700),
       base::TimeDelta::FromMilliseconds(6300),
      },
  };

  for (const auto& test_case : test_cases) {
    base::FieldTrialList field_trial_list(nullptr);
    base::FieldTrialList::CreateFieldTrial(
        kConfigFetchTrialGroup,
        test_case.experiment_enabled ? "Enabled" : "Disabled");
    base::Time min_now = base::Time::Now();
    SetConfigExperimentValues(
        test_case.experiment_enabled, test_case.retrieve_offset_from_now,
        test_case.roundtrip_milliseconds_base, test_case.roundtrip_multiplier,
        test_case.roundtrip_milliseconds_increment, kConfigExpirationSeconds,
        test_case.always_stale);
    scoped_ptr<DataReductionProxyConfigRetrievalParams> config_params =
        DataReductionProxyConfigRetrievalParams::Create(pref_service());
    base::Time max_now = base::Time::Now();
    if (!test_case.experiment_enabled) {
      EXPECT_EQ(nullptr, config_params.get());
      continue;
    }

    EXPECT_NE(nullptr, config_params.get());
    ExpectInTimeRange(min_now, max_now,
                      test_case.expected_config_expiration_from_now,
                      config_params->config_expiration());
    EXPECT_EQ(config_params->refresh_interval(),
              base::TimeDelta::FromSeconds(kConfigExpirationSeconds) -
                  base::TimeDelta::FromSeconds(kConfigFetchBufferSeconds));
    if (test_case.expected_has_variations) {
      EXPECT_EQ(kConfigFetchGroups, (int)config_params->variations().size());
      ExpectInTimeRange(
          min_now, max_now, test_case.expected_delta_0,
          config_params->variations()[0].simulated_config_retrieved());
      ExpectInTimeRange(
          min_now, max_now, test_case.expected_delta_2,
          config_params->variations()[2].simulated_config_retrieved());
      ExpectInTimeRange(
          min_now, max_now, test_case.expected_delta_5,
          config_params->variations()[5].simulated_config_retrieved());
    } else {
      EXPECT_EQ(0u, config_params->variations().size());
    }
  }
}

}  // namespace data_reduction_proxy
