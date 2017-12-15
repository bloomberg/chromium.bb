// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator.h"

#include <stddef.h>
#include <memory>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/version.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

// Constants used to create the test seed.
const char kTestSeedStudyName[] = "test";
const char kTestSeedExperimentName[] = "abc";
const int kTestSeedExperimentProbability = 100;
const char kTestSeedSerialNumber[] = "123";

// Populates |seed| with simple test data. The resulting seed will contain one
// study called "test", which contains one experiment called "abc" with
// probability weight 100. |seed|'s study field will be cleared before adding
// the new study.
VariationsSeed CreateTestSeed() {
  VariationsSeed seed;
  Study* study = seed.add_study();
  study->set_name(kTestSeedStudyName);
  study->set_default_experiment_name(kTestSeedExperimentName);
  Study_Experiment* experiment = study->add_experiment();
  experiment->set_name(kTestSeedExperimentName);
  experiment->set_probability_weight(kTestSeedExperimentProbability);
  seed.set_serial_number(kTestSeedSerialNumber);
  return seed;
}

class TestPlatformFieldTrials : public PlatformFieldTrials {
 public:
  TestPlatformFieldTrials() = default;
  ~TestPlatformFieldTrials() override = default;

  // PlatformFieldTrials:
  void SetupFieldTrials() override {}
  void SetupFeatureControllingFieldTrials(
      bool has_seed,
      base::FeatureList* feature_list) override {}
};

class TestVariationsServiceClient : public VariationsServiceClient {
 public:
  TestVariationsServiceClient() = default;
  ~TestVariationsServiceClient() override = default;

  // VariationsServiceClient:
  std::string GetApplicationLocale() override { return std::string(); }
  base::Callback<base::Version(void)> GetVersionForSimulationCallback()
      override {
    return base::Callback<base::Version(void)>();
  }
  net::URLRequestContextGetter* GetURLRequestContext() override {
    return nullptr;
  }
  network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  version_info::Channel GetChannel() override {
    return version_info::Channel::UNKNOWN;
  }
  bool OverridesRestrictParameter(std::string* parameter) override {
    if (restrict_parameter_.empty())
      return false;
    *parameter = restrict_parameter_;
    return true;
  }

  void set_restrict_parameter(const std::string& value) {
    restrict_parameter_ = value;
  }

 private:
  std::string restrict_parameter_;

  DISALLOW_COPY_AND_ASSIGN(TestVariationsServiceClient);
};

class TestVariationsFieldTrialCreator : public VariationsFieldTrialCreator {
 public:
  TestVariationsFieldTrialCreator(PrefService* local_state,
                                  TestVariationsServiceClient* client)
      : VariationsFieldTrialCreator(local_state, client, UIStringOverrider()) {}

  ~TestVariationsFieldTrialCreator() override = default;

  // A convenience wrapper around SetupFieldTrials() which passes default values
  // for uninteresting params.
  bool SetupFieldTrials() {
    std::vector<std::string> variation_ids;
    TestPlatformFieldTrials platform_field_trials;
    return VariationsFieldTrialCreator::SetupFieldTrials(
        "", "", "", std::set<std::string>(), nullptr,
        std::make_unique<base::FeatureList>(), &variation_ids,
        &platform_field_trials);
  }

 private:
  bool LoadSeed(VariationsSeed* seed) override {
    *seed = CreateTestSeed();
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(TestVariationsFieldTrialCreator);
};

}  // namespace

class FieldTrialCreatorTest : public ::testing::Test {
 protected:
  FieldTrialCreatorTest() {
    VariationsService::RegisterPrefs(prefs_.registry());
    global_feature_list_ = base::FeatureList::ClearInstanceForTesting();
  }

  ~FieldTrialCreatorTest() override {
    // Clear out any features created by tests in this suite, and restore the
    // global feature list.
    base::FeatureList::ClearInstanceForTesting();
    base::FeatureList::RestoreInstanceForTesting(
        std::move(global_feature_list_));
  }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  // The global feature list, which is ignored by tests in this suite.
  std::unique_ptr<base::FeatureList> global_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialCreatorTest);
};

TEST_F(FieldTrialCreatorTest, SetupFieldTrials_Basic) {
  // This local FieldTrialList holds any field trials created in this test.
  base::FieldTrialList field_trial_list(nullptr);
  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client);

  // Simulate a seed having been stored recently.
  prefs_.SetInt64(prefs::kVariationsLastFetchTime,
                  base::Time::Now().ToInternalValue());

  // Check that field trials are created from the seed. Since the test study has
  // only 1 experiment with 100% probability weight, we must be part of it.
  EXPECT_TRUE(field_trial_creator.SetupFieldTrials());
  EXPECT_EQ(kTestSeedExperimentName,
            base::FieldTrialList::FindFullName(kTestSeedStudyName));
}

TEST_F(FieldTrialCreatorTest, SetupFieldTrials_NoLastFetchTime) {
  // This local FieldTrialList holds any field trials created in this test.
  base::FieldTrialList field_trial_list(nullptr);
  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client);

  // Simulate a first run by leaving |prefs::kVariationsLastFetchTime| empty.
  EXPECT_EQ(0, prefs_.GetInt64(prefs::kVariationsLastFetchTime));

  // Check that field trials are created from the seed. Since the test study has
  // only 1 experiment with 100% probability weight, we must be part of it.
  EXPECT_TRUE(field_trial_creator.SetupFieldTrials());
  EXPECT_EQ(base::FieldTrialList::FindFullName(kTestSeedStudyName),
            kTestSeedExperimentName);
}

TEST_F(FieldTrialCreatorTest, SetupFieldTrials_ExpiredSeed) {
  // This local FieldTrialList holds any field trials created in this test.
  base::FieldTrialList field_trial_list(nullptr);
  TestVariationsServiceClient variations_service_client;
  TestVariationsFieldTrialCreator field_trial_creator(
      &prefs_, &variations_service_client);

  // Simulate an expired seed.
  const base::Time seed_date =
      base::Time::Now() - base::TimeDelta::FromDays(31);
  prefs_.SetInt64(prefs::kVariationsLastFetchTime, seed_date.ToInternalValue());

  // Check that field trials are not created from the expired seed.
  EXPECT_FALSE(field_trial_creator.SetupFieldTrials());
  EXPECT_TRUE(base::FieldTrialList::FindFullName(kTestSeedStudyName).empty());
}

}  // namespace variations
