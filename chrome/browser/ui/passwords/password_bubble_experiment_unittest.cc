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

const char kGroupName[] = "SomeGroupName";
const int kNopeThreshold = 10;

void SetupExperiment() {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      password_bubble_experiment::kExperimentName,
      kGroupName));
  std::map<std::string, std::string> params;
  params[password_bubble_experiment::kParamNopeThreshold] =
      base::IntToString(kNopeThreshold);
  ASSERT_TRUE(variations::AssociateVariationParams(
      password_bubble_experiment::kExperimentName,
      kGroupName,
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
  }

  void TearDown() override {
    variations::testing::ClearAllVariationParams();
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

 private:
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(PasswordBubbleExperimentTest, NoExperiment) {
  EXPECT_FALSE(
      password_bubble_experiment::ShouldShowNeverForThisSiteDefault(prefs()));
  for (int i = 0; i <= kNopeThreshold; ++i) {
    password_bubble_experiment::RecordBubbleClosed(
        prefs(), password_manager::metrics_util::CLICKED_NOPE);
    EXPECT_FALSE(
        password_bubble_experiment::ShouldShowNeverForThisSiteDefault(prefs()));
  }
}

TEST_F(PasswordBubbleExperimentTest, WithExperiment) {
  SetupExperiment();

  // Repeatedly click "Never". It shouldn't affect the state.
  for (int i = 0; i < kNopeThreshold; ++i) {
    password_manager::metrics_util::UIDismissalReason reason =
        password_manager::metrics_util::CLICKED_NEVER;
    password_bubble_experiment::RecordBubbleClosed(prefs(), reason);
  }
  EXPECT_FALSE(
      password_bubble_experiment::ShouldShowNeverForThisSiteDefault(prefs()));
  // Repeatedly refuse to save password, for |kNopeThreshold|-1 times.
  for (int i = 0; i < kNopeThreshold - 1; ++i) {
    password_manager::metrics_util::UIDismissalReason reason =
        password_manager::metrics_util::CLICKED_NOPE;
    password_bubble_experiment::RecordBubbleClosed(prefs(), reason);
  }
  EXPECT_FALSE(
      password_bubble_experiment::ShouldShowNeverForThisSiteDefault(prefs()));

  // Refuse to save once more to make Never the default button.
  password_bubble_experiment::RecordBubbleClosed(
      prefs(), password_manager::metrics_util::CLICKED_NOPE);
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowNeverForThisSiteDefault(prefs()));
  password_bubble_experiment::RecordBubbleClosed(
      prefs(), password_manager::metrics_util::CLICKED_SAVE);
  EXPECT_FALSE(
      password_bubble_experiment::ShouldShowNeverForThisSiteDefault(prefs()));

  // Repeatedly refuse to save password, for |kNopeThreshold| times.
  for (int i = 0; i < kNopeThreshold; ++i) {
    password_manager::metrics_util::UIDismissalReason reason =
        password_manager::metrics_util::CLICKED_NOPE;
    password_bubble_experiment::RecordBubbleClosed(prefs(), reason);
  }
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowNeverForThisSiteDefault(prefs()));
  // Repeatedly click "Never". It shouldn't affect the state.
  for (int i = 0; i < kNopeThreshold; ++i) {
    password_manager::metrics_util::UIDismissalReason reason =
        password_manager::metrics_util::CLICKED_NEVER;
    password_bubble_experiment::RecordBubbleClosed(prefs(), reason);
  }
  EXPECT_TRUE(
      password_bubble_experiment::ShouldShowNeverForThisSiteDefault(prefs()));
}
