// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_throttler.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "components/safe_browsing/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace safe_browsing {

class TriggerThrottlerTest : public ::testing::Test {
 public:
  void SetQuotaForTriggerType(TriggerType trigger_type, size_t max_quota) {
    trigger_throttler_.trigger_type_and_quota_list_.push_back(
        std::make_pair(trigger_type, max_quota));
  }

  TriggerThrottler* throttler() { return &trigger_throttler_; }

  void SetTestClock(base::Clock* clock) {
    trigger_throttler_.SetClockForTesting(base::WrapUnique(clock));
  }

  std::vector<time_t> GetEventTimestampsForTriggerType(
      TriggerType trigger_type) {
    return trigger_throttler_.trigger_events_[trigger_type];
  }

 private:
  TriggerThrottler trigger_throttler_;
};

TEST_F(TriggerThrottlerTest, SecurityInterstitialsHaveUnlimitedQuota) {
  // Make sure that security interstitials never run out of quota.
  for (int i = 0; i < 1000; ++i) {
    throttler()->TriggerFired(TriggerType::SECURITY_INTERSTITIAL);
    EXPECT_TRUE(
        throttler()->TriggerCanFire(TriggerType::SECURITY_INTERSTITIAL));
  }
}

TEST_F(TriggerThrottlerTest, SecurityInterstitialQuotaCanNotBeOverwritten) {
  // Make sure that security interstitials never run out of quota, even if we
  // try to configure quota for this trigger type.
  SetQuotaForTriggerType(TriggerType::SECURITY_INTERSTITIAL, 3);
  for (int i = 0; i < 1000; ++i) {
    throttler()->TriggerFired(TriggerType::SECURITY_INTERSTITIAL);
    EXPECT_TRUE(
        throttler()->TriggerCanFire(TriggerType::SECURITY_INTERSTITIAL));
  }
}

TEST_F(TriggerThrottlerTest, TriggerQuotaSetToOne) {
  // This is a corner case where we can exceed array bounds for triggers that
  // have quota set to 1 report per day. This can happen when quota is 1 and
  // exactly one event has fired. When deciding whether another event can fire,
  // we look at the Nth-from-last event to check if it was recent or not - in
  // this scenario, Nth-from-last is 1st-from-last (because quota is 1). An
  // off-by-one error in this calculation can cause us to look at position 1
  // instead of position 0 in the even list.
  SetQuotaForTriggerType(TriggerType::AD_SAMPLE, 1);

  // Fire the trigger, first event will be allowed.
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);

  // Ensure that checking whether this trigger can fire again does not cause
  // an error and also returns the expected result.
  EXPECT_FALSE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
}

TEST_F(TriggerThrottlerTest, TriggerExceedsQuota) {
  // Ensure that a trigger can't fire more than its quota allows.
  SetQuotaForTriggerType(TriggerType::AD_SAMPLE, 2);

  // First two triggers should work
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);

  // Third attempt will fail since we're out of quota.
  EXPECT_FALSE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
}

TEST_F(TriggerThrottlerTest, TriggerQuotaResetsAfterOneDay) {
  // Ensure that trigger events older than a day are cleaned up and triggers can
  // resume firing.

  // We initialize the test clock to several days ago and fire some events to
  // use up quota. We then advance the clock by a day and ensure quota is
  // available again.
  base::SimpleTestClock* test_clock(new base::SimpleTestClock);
  test_clock->SetNow(base::Time::Now() - base::TimeDelta::FromDays(10));
  time_t base_ts = test_clock->Now().ToTimeT();

  SetTestClock(test_clock);
  SetQuotaForTriggerType(TriggerType::AD_SAMPLE, 2);

  // First two triggers should work
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);

  // Third attempt will fail since we're out of quota.
  EXPECT_FALSE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));

  // Also confirm that the throttler contains two event timestamps for the above
  // two events - since we use a test clock, it doesn't move unless we tell it
  // to.
  EXPECT_THAT(GetEventTimestampsForTriggerType(TriggerType::AD_SAMPLE),
              ElementsAre(base_ts, base_ts));

  // Move the clock forward by 1 day (and a bit) and try the trigger again,
  // quota should be available now.
  test_clock->Advance(base::TimeDelta::FromDays(1) +
                      base::TimeDelta::FromSeconds(1));
  time_t advanced_ts = test_clock->Now().ToTimeT();
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));

  // The previous time stamps should remain in the throttler.
  EXPECT_THAT(GetEventTimestampsForTriggerType(TriggerType::AD_SAMPLE),
              ElementsAre(base_ts, base_ts));

  // Firing the trigger will clean up the expired timestamps and insert the new
  // timestamp.
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);
  EXPECT_THAT(GetEventTimestampsForTriggerType(TriggerType::AD_SAMPLE),
              ElementsAre(advanced_ts));
}

TEST(TriggerThrottlerTestFinch, ConfigureQuotaViaFinch) {
  // Make sure that setting the quota param via Finch params works as expected.
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
      safe_browsing::kTriggerThrottlerDailyQuotaFeature.name, "Group");
  std::map<std::string, std::string> feature_params;
  feature_params[std::string(safe_browsing::kTriggerTypeAndQuotaParam)] =
      base::StringPrintf("%d,%d", TriggerType::AD_SAMPLE, 3);
  base::AssociateFieldTrialParams(
      safe_browsing::kTriggerThrottlerDailyQuotaFeature.name, "Group",
      feature_params);
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      safe_browsing::kTriggerThrottlerDailyQuotaFeature.name, std::string());
  feature_list->AssociateReportingFieldTrial(
      safe_browsing::kTriggerThrottlerDailyQuotaFeature.name,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // The throttler has been configured (above) to allow ad samples to fire three
  // times per day.
  TriggerThrottler throttler;

  // First three triggers should work
  EXPECT_TRUE(throttler.TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler.TriggerFired(TriggerType::AD_SAMPLE);
  EXPECT_TRUE(throttler.TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler.TriggerFired(TriggerType::AD_SAMPLE);
  EXPECT_TRUE(throttler.TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler.TriggerFired(TriggerType::AD_SAMPLE);

  // Fourth attempt will fail since we're out of quota.
  EXPECT_FALSE(throttler.TriggerCanFire(TriggerType::AD_SAMPLE));
}
}  // namespace safe_browsing
