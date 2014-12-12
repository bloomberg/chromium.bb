// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_url_collection_experiment.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_clock.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace urls_collection_experiment {

namespace {

int kParamActivePeriodInDaysValue = 7;
int kParamExperimentLengthInDaysValue = 365;
const char kParamBubbleStatusValue[] = "show_bubble";
const char kGroupShowBubbleWithClock[] = "ShowBubbleWithClock";
const char kGroupNeverShowBubbleWithClock[] = "NeverShowBubbleWithClock";

void SetupShowBubbleWithClockExperimentGroup() {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      kExperimentName, kGroupShowBubbleWithClock));
  std::map<std::string, std::string> params;
  params[kParamActivePeriodInDays] =
      base::IntToString(kParamActivePeriodInDaysValue);
  params[kParamExperimentLengthInDays] =
      base::IntToString(kParamExperimentLengthInDaysValue);
  params[kParamBubbleStatus] = kParamBubbleStatusValue;
  ASSERT_TRUE(variations::AssociateVariationParams(
      kExperimentName, kGroupShowBubbleWithClock, params));
}

void SetupNeverShowBubbleWithClockExperimentGroup() {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      kExperimentName, kGroupNeverShowBubbleWithClock));
}

}  // namespace

// Same as ShouldShowBubble(PrefService* prefs), but specifies a mock
// interface for clock functions for testing.
bool ShouldShowBubbleWithClock(PrefService* prefs, base::Clock* clock);

class PasswordManagerUrlsCollectionExperimentTest : public testing::Test {
 public:
  PasswordManagerUrlsCollectionExperimentTest()
      : field_trial_list_(new metrics::SHA1EntropyProvider("foo")) {}

  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAllowToCollectURLBubbleWasShown, false);
    pref_service_.registry()->RegisterDoublePref(
        prefs::kAllowToCollectURLBubbleActivePeriodStartFactor, -1);
  }
  void TearDown() override { variations::testing::ClearAllVariationParams(); }

  void SetTimeToBubbleActivePeriod(int experiment_length_in_days) {
    base::Time starting_time =
        DetermineStartOfActivityPeriod(prefs(), experiment_length_in_days);
    test_clock()->SetNow(starting_time);
  }

  void SetTimePastBubbleActivePeriod(int experiment_length_in_days) {
    base::Time starting_time =
        DetermineStartOfActivityPeriod(prefs(), experiment_length_in_days);
    test_clock()->SetNow(starting_time + base::TimeDelta::FromDays(
                                             kParamActivePeriodInDaysValue));
  }

  void PretendBubbleWasAlreadyShown() {
    prefs()->SetBoolean(prefs::kAllowToCollectURLBubbleWasShown, true);
  }

  PrefService* prefs() { return &pref_service_; }

  base::SimpleTestClock* test_clock() { return &test_clock_; }

 private:
  TestingPrefServiceSimple pref_service_;
  base::FieldTrialList field_trial_list_;
  base::SimpleTestClock test_clock_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerUrlsCollectionExperimentTest);
};

TEST_F(PasswordManagerUrlsCollectionExperimentTest,
       TestShowBubbleWithClockGroupTimeSpanWhenBubbleShouldAppear) {
  SetupShowBubbleWithClockExperimentGroup();
  SetTimeToBubbleActivePeriod(kParamExperimentLengthInDaysValue);
  EXPECT_TRUE(ShouldShowBubbleWithClock(prefs(), test_clock()));
}

TEST_F(PasswordManagerUrlsCollectionExperimentTest,
       TestShowBubbleWithClockGroupTimeSpanWhenBubbleShouldNotAppear) {
  SetupShowBubbleWithClockExperimentGroup();
  SetTimePastBubbleActivePeriod(kParamExperimentLengthInDaysValue);
  EXPECT_FALSE(ShouldShowBubbleWithClock(prefs(), test_clock()));
}

TEST_F(PasswordManagerUrlsCollectionExperimentTest,
       TestNeverShowBubbleWithClockGroup) {
  SetupNeverShowBubbleWithClockExperimentGroup();
  SetTimeToBubbleActivePeriod(kParamExperimentLengthInDaysValue);
  EXPECT_FALSE(ShouldShowBubbleWithClock(prefs(), test_clock()));
}

TEST_F(PasswordManagerUrlsCollectionExperimentTest, TestBubbleWasAlreadyShown) {
  SetupShowBubbleWithClockExperimentGroup();
  SetTimeToBubbleActivePeriod(kParamExperimentLengthInDaysValue);
  PretendBubbleWasAlreadyShown();
  EXPECT_FALSE(ShouldShowBubbleWithClock(prefs(), test_clock()));
}

}  //  namespace urls_collection_experiment
}  //  namespace password_manager
