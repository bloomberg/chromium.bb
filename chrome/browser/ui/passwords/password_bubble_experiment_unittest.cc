// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_bubble_experiment.h"

#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kTimeSpanDays = 2;
const int kTimeSpanThreshold = 3;
const int kProbabilityFakeSaves = 0;
const int kProbabilityHistory = 10;

void SetupTimeSpanExperiment() {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      password_bubble_experiment::kExperimentName,
      password_bubble_experiment::kGroupTimeSpanBased));
  std::map<std::string, std::string> params;
  params[password_bubble_experiment::kParamTimeSpan] =
      base::IntToString(kTimeSpanDays);
  params[password_bubble_experiment::kParamTimeSpanNopeThreshold] =
      base::IntToString(kTimeSpanThreshold);
  ASSERT_TRUE(variations::AssociateVariationParams(
      password_bubble_experiment::kExperimentName,
      password_bubble_experiment::kGroupTimeSpanBased,
      params));
}

void SetupProbabilityExperiment() {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      password_bubble_experiment::kExperimentName,
      password_bubble_experiment::kGroupProbabilityBased));
  std::map<std::string, std::string> params;
  params[password_bubble_experiment::kParamProbabilityFakeSaves] =
      base::IntToString(kProbabilityFakeSaves);
  params[password_bubble_experiment::kParamProbabilityInteractionsCount] =
      base::IntToString(kProbabilityHistory);
  ASSERT_TRUE(variations::AssociateVariationParams(
      password_bubble_experiment::kExperimentName,
      password_bubble_experiment::kGroupProbabilityBased,
      params));
}

} // namespace

class PasswordBubbleExperimentTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_.reset(new TestingProfile(temp_dir_.path()));

    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("foo")));
    variations::testing::ClearAllVariationParams();
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

 private:
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(PasswordBubbleExperimentTest, TimeSpan) {
  SetupTimeSpanExperiment();

  EXPECT_TRUE(password_bubble_experiment::ShouldShowBubble(prefs()));
  // Don't save password enough times.
  for (int i = 0; i < kTimeSpanThreshold; ++i) {
    password_manager::metrics_util::UIDismissalReason reason = i % 2 ?
        password_manager::metrics_util::NO_DIRECT_INTERACTION :
        password_manager::metrics_util::CLICKED_NOPE;
    password_bubble_experiment::RecordBubbleClosed(prefs(), reason);
  }
  EXPECT_FALSE(password_bubble_experiment::ShouldShowBubble(prefs()));

  // Save password many times. It doesn't bring the bubble back while the time
  // span isn't over.
  for (int i = 0; i < 2*kTimeSpanThreshold; ++i) {
    password_bubble_experiment::RecordBubbleClosed(
        prefs(),
        password_manager::metrics_util::CLICKED_SAVE);
  }
  EXPECT_FALSE(password_bubble_experiment::ShouldShowBubble(prefs()));
}

TEST_F(PasswordBubbleExperimentTest, TimeSpanOver) {
  SetupTimeSpanExperiment();

  base::Time past_interval =
      base::Time::Now() - base::TimeDelta::FromDays(kTimeSpanDays + 1);
  prefs()->SetInt64(prefs::kPasswordBubbleTimeStamp,
                    past_interval.ToInternalValue());
  prefs()->SetInteger(prefs::kPasswordBubbleNopesCount, kTimeSpanThreshold);
  // The time span is over. The bubble should be shown.
  EXPECT_TRUE(password_bubble_experiment::ShouldShowBubble(prefs()));
  EXPECT_EQ(0, prefs()->GetInteger(prefs::kPasswordBubbleNopesCount));

  // Set the old time span again and record "Nope". The counter restarts from 0.
  prefs()->SetInt64(prefs::kPasswordBubbleTimeStamp,
                    past_interval.ToInternalValue());
  password_bubble_experiment::RecordBubbleClosed(
      prefs(), password_manager::metrics_util::CLICKED_NOPE);
  EXPECT_TRUE(password_bubble_experiment::ShouldShowBubble(prefs()));
  EXPECT_EQ(1, prefs()->GetInteger(prefs::kPasswordBubbleNopesCount));
}

TEST_F(PasswordBubbleExperimentTest, Probability) {
  SetupProbabilityExperiment();

  EXPECT_TRUE(password_bubble_experiment::ShouldShowBubble(prefs()));
  // Don't save password enough times.
  for (int i = 0; i < kProbabilityHistory; ++i) {
    password_manager::metrics_util::UIDismissalReason reason = i % 2 ?
        password_manager::metrics_util::NO_DIRECT_INTERACTION :
        password_manager::metrics_util::CLICKED_NOPE;
    password_bubble_experiment::RecordBubbleClosed(prefs(), reason);
  }
  EXPECT_FALSE(password_bubble_experiment::ShouldShowBubble(prefs()));

  // Save password enough times.
  for (int i = 0; i < kProbabilityHistory; ++i) {
    password_bubble_experiment::RecordBubbleClosed(
        prefs(),
        password_manager::metrics_util::CLICKED_SAVE);
  }
  EXPECT_TRUE(password_bubble_experiment::ShouldShowBubble(prefs()));
}
