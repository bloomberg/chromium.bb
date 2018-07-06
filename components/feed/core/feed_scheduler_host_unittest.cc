// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_scheduler_host.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/simple_test_clock.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/core/time_serialization.h"
#include "components/feed/core/user_classifier.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

// Fixed "now" to make tests more deterministic.
char kNowString[] = "2018-06-11 15:41";

using base::Time;

class FeedSchedulerHostTest : public ::testing::Test {
 public:
  void FixedTimerCompletion() { fixed_timer_completion_count_++; }

 protected:
  FeedSchedulerHostTest() : weak_factory_(this) {
    Time now;
    EXPECT_TRUE(Time::FromUTCString(kNowString, &now));
    test_clock_.SetNow(now);

    FeedSchedulerHost::RegisterProfilePrefs(pref_service_.registry());
    UserClassifier::RegisterProfilePrefs(pref_service_.registry());

    scheduler_ =
        std::make_unique<FeedSchedulerHost>(&pref_service_, &test_clock_);
    InitializeScheduler(scheduler());
  }

  void InitializeScheduler(FeedSchedulerHost* scheduler) {
    scheduler->Initialize(
        base::BindRepeating(&FeedSchedulerHostTest::TriggerRefresh,
                            base::Unretained(this)),
        base::BindRepeating(&FeedSchedulerHostTest::ScheduleWakeUp,
                            base::Unretained(this)));
  }

  // Note: Time will be advanced.
  void ClassifyAsRareNtpUser() {
    // By moving time forward from initial seed events, the user will be moved
    // into RARE_NTP_USER classification.
    test_clock()->Advance(base::TimeDelta::FromDays(7));
  }

  // Note: Time will be advanced.
  void ClassifyAsActiveSuggestionsConsumer() {
    // Click on some articles to move the user into ACTIVE_SUGGESTIONS_CONSUMER
    // classification. Separate by at least 30 minutes for different sessions.
    scheduler()->OnSuggestionConsumed();
    test_clock()->Advance(base::TimeDelta::FromMinutes(31));
    scheduler()->OnSuggestionConsumed();
  }

  // This helper method sets prefs::kLastFetchAttemptTime to the same value
  // that's about to be passed into ShouldSessionRequestData(). This is what the
  // scheduler will typically experience when refreshes are successful.
  NativeRequestBehavior ShouldSessionRequestData(
      bool has_content,
      base::Time content_creation_date_time,
      bool has_outstanding_request) {
    pref_service()->SetTime(prefs::kLastFetchAttemptTime,
                            content_creation_date_time);
    return scheduler()->ShouldSessionRequestData(
        has_content, content_creation_date_time, has_outstanding_request);
  }

  PrefService* pref_service() { return &pref_service_; }
  base::SimpleTestClock* test_clock() { return &test_clock_; }
  FeedSchedulerHost* scheduler() { return scheduler_.get(); }
  int refresh_call_count() { return refresh_call_count_; }
  const std::vector<base::TimeDelta>& schedule_wake_up_times() {
    return schedule_wake_up_times_;
  }
  int cancel_wake_up_call_count() { return cancel_wake_up_call_count_; }
  int fixed_timer_completion_count() { return fixed_timer_completion_count_; }

 private:
  void TriggerRefresh() { refresh_call_count_++; }

  void ScheduleWakeUp(base::TimeDelta threshold_ms) {
    schedule_wake_up_times_.push_back(threshold_ms);
  }

  void CancelWakeUp() { cancel_wake_up_call_count_++; }

  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<FeedSchedulerHost> scheduler_;
  int refresh_call_count_ = 0;
  std::vector<base::TimeDelta> schedule_wake_up_times_;
  int cancel_wake_up_call_count_ = 0;
  int fixed_timer_completion_count_ = 0;
  base::WeakPtrFactory<FeedSchedulerHostTest> weak_factory_;
};

TEST_F(FeedSchedulerHostTest, GetTriggerThreshold) {
  // Make sure that there is no missing configuration in the cartesian product
  // of states between TriggerType and UserClass.
  std::vector<FeedSchedulerHost::TriggerType> triggers = {
      FeedSchedulerHost::TriggerType::NTP_SHOWN,
      FeedSchedulerHost::TriggerType::FOREGROUNDED,
      FeedSchedulerHost::TriggerType::FIXED_TIMER};

  // Classification starts out as an active NTP user.
  for (FeedSchedulerHost::TriggerType trigger : triggers) {
    EXPECT_FALSE(scheduler()->GetTriggerThreshold(trigger).is_zero());
  }

  ClassifyAsRareNtpUser();
  for (FeedSchedulerHost::TriggerType trigger : triggers) {
    EXPECT_FALSE(scheduler()->GetTriggerThreshold(trigger).is_zero());
  }

  ClassifyAsActiveSuggestionsConsumer();
  for (FeedSchedulerHost::TriggerType trigger : triggers) {
    EXPECT_FALSE(scheduler()->GetTriggerThreshold(trigger).is_zero());
  }
}

TEST_F(FeedSchedulerHostTest, ShouldSessionRequestDataSimple) {
  // For an ACTIVE_NTP_USER, refreshes on NTP_OPEN should be triggered after 4
  // hours, and staleness should be at 24 hours. Each case tests a range of
  // values.
  base::Time no_refresh_large =
      test_clock()->Now() - base::TimeDelta::FromHours(3);
  base::Time refresh_only_small =
      test_clock()->Now() - base::TimeDelta::FromHours(5);
  base::Time refresh_only_large =
      test_clock()->Now() - base::TimeDelta::FromHours(23);
  base::Time stale_small = test_clock()->Now() - base::TimeDelta::FromHours(25);

  EXPECT_EQ(REQUEST_WITH_WAIT,
            ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));

  EXPECT_EQ(REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ refresh_only_small,
                /*has_outstanding_request*/ false));
  EXPECT_EQ(REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ refresh_only_large,
                /*has_outstanding_request*/ false));

  EXPECT_EQ(
      REQUEST_WITH_TIMEOUT,
      ShouldSessionRequestData(
          /*has_content*/ true, /*content_creation_date_time*/ stale_small,
          /*has_outstanding_request*/ false));
  EXPECT_EQ(
      REQUEST_WITH_TIMEOUT,
      ShouldSessionRequestData(
          /*has_content*/ true, /*content_creation_date_time*/ base::Time(),
          /*has_outstanding_request*/ false));

  // |content_creation_date_time| should be ignored when |has_content| is false.
  EXPECT_EQ(NO_REQUEST_WITH_WAIT,
            ShouldSessionRequestData(
                /*has_content*/ false,
                /*content_creation_date_time*/ test_clock()->Now(),
                /*has_outstanding_request*/ true));
  EXPECT_EQ(NO_REQUEST_WITH_WAIT,
            ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ true));

  EXPECT_EQ(NO_REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now(),
                /*has_outstanding_request*/ false));
  EXPECT_EQ(NO_REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ no_refresh_large,
                /*has_outstanding_request*/ false));
  EXPECT_EQ(NO_REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now(),
                /*has_outstanding_request*/ true));
  EXPECT_EQ(NO_REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ refresh_only_large,
                /*has_outstanding_request*/ true));

  EXPECT_EQ(
      NO_REQUEST_WITH_TIMEOUT,
      ShouldSessionRequestData(
          /*has_content*/ true, /*content_creation_date_time*/ stale_small,
          /*has_outstanding_request*/ true));
  EXPECT_EQ(
      NO_REQUEST_WITH_TIMEOUT,
      ShouldSessionRequestData(
          /*has_content*/ true, /*content_creation_date_time*/ base::Time(),
          /*has_outstanding_request*/ true));
}

TEST_F(FeedSchedulerHostTest, ShouldSessionRequestDataDivergentTimes) {
  // If a request fails, then the |content_creation_date_time| and the value of
  // prefs::kLastFetchAttemptTime may diverge. This is okay, and will typically
  // mean that refreshes are not taken. Staleness should continue to track
  // |content_creation_date_time|, but because staleness uses a bigger threshold
  // than NTP_OPEN, this will not affect much.

  // Like above case, the user is an ACTIVE_NTP_USER, staleness at 24 hours and
  // refresh at 4.
  base::Time refresh = test_clock()->Now() - base::TimeDelta::FromHours(5);
  base::Time no_refresh = test_clock()->Now() - base::TimeDelta::FromHours(3);
  base::Time stale = test_clock()->Now() - base::TimeDelta::FromHours(25);
  base::Time not_stale = test_clock()->Now() - base::TimeDelta::FromHours(23);

  pref_service()->SetTime(prefs::kLastFetchAttemptTime, no_refresh);
  EXPECT_EQ(NO_REQUEST_WITH_CONTENT, scheduler()->ShouldSessionRequestData(
                                         /*has_content*/ true,
                                         /*content_creation_date_time*/ stale,
                                         /*has_outstanding_request*/ false));

  pref_service()->SetTime(prefs::kLastFetchAttemptTime, refresh);

  EXPECT_EQ(REQUEST_WITH_CONTENT, scheduler()->ShouldSessionRequestData(
                                      /*has_content*/ true,
                                      /*content_creation_date_time*/ not_stale,
                                      /*has_outstanding_request*/ false));

  pref_service()->SetTime(prefs::kLastFetchAttemptTime, refresh);
  EXPECT_EQ(REQUEST_WITH_TIMEOUT, scheduler()->ShouldSessionRequestData(
                                      /*has_content*/ true,
                                      /*content_creation_date_time*/ stale,
                                      /*has_outstanding_request*/ false));

  // This shouldn't be possible, since last attempt is farther back than
  // |content_creation_date_time| which updates on success, but verify scheduler
  // handles it reasonably.
  pref_service()->SetTime(prefs::kLastFetchAttemptTime, base::Time());
  EXPECT_EQ(REQUEST_WITH_CONTENT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now(),
                /*has_outstanding_request*/ false));

  // By changing the foregrounded threshold, staleness calculation changes.
  variations::testing::VariationParamsManager variation_params(
      kInterestFeedContentSuggestions.name,
      {{"foregrounded_hours_active_ntp_user", "7.5"}},
      {kInterestFeedContentSuggestions.name});

  EXPECT_EQ(REQUEST_WITH_CONTENT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    base::TimeDelta::FromHours(7),
                /*has_outstanding_request*/ false));
  EXPECT_EQ(REQUEST_WITH_TIMEOUT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    base::TimeDelta::FromHours(8),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, NtpShownActiveNtpUser) {
  variations::testing::VariationParamsManager variation_params(
      kInterestFeedContentSuggestions.name,
      {{"ntp_shown_hours_active_ntp_user", "2.5"}},
      {kInterestFeedContentSuggestions.name});

  EXPECT_EQ(NO_REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    base::TimeDelta::FromHours(2),
                /*has_outstanding_request*/ false));

  EXPECT_EQ(REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    base::TimeDelta::FromHours(3),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, NtpShownRareNtpUser) {
  variations::testing::VariationParamsManager variation_params(
      kInterestFeedContentSuggestions.name,
      {{"ntp_shown_hours_rare_ntp_user", "1.5"}},
      {kInterestFeedContentSuggestions.name});

  ClassifyAsRareNtpUser();

  EXPECT_EQ(NO_REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    base::TimeDelta::FromHours(1),
                /*has_outstanding_request*/ false));

  // ShouldSessionRequestData() has the side effect of adding NTP_SHOWN event to
  // the classifier, so push the timer out to keep classification.
  ClassifyAsRareNtpUser();

  EXPECT_EQ(REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    base::TimeDelta::FromHours(2),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, NtpShownActiveSuggestionsConsumer) {
  ClassifyAsActiveSuggestionsConsumer();

  EXPECT_EQ(NO_REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    base::TimeDelta::FromMinutes(59),
                /*has_outstanding_request*/ false));

  EXPECT_EQ(REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    base::TimeDelta::FromMinutes(61),
                /*has_outstanding_request*/ false));

  variations::testing::VariationParamsManager variation_params(
      kInterestFeedContentSuggestions.name,
      {{"ntp_shown_hours_active_suggestions_consumer", "7.5"}},
      {kInterestFeedContentSuggestions.name});

  EXPECT_EQ(NO_REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    base::TimeDelta::FromHours(7),
                /*has_outstanding_request*/ false));

  EXPECT_EQ(REQUEST_WITH_CONTENT,
            ShouldSessionRequestData(
                /*has_content*/ true,
                /*content_creation_date_time*/ test_clock()->Now() -
                    base::TimeDelta::FromHours(8),
                /*has_outstanding_request*/ false));
}

TEST_F(FeedSchedulerHostTest, OnReceiveNewContentVerifyPref) {
  EXPECT_EQ(Time(), pref_service()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnReceiveNewContent(Time());
  EXPECT_EQ(Time(), pref_service()->GetTime(prefs::kLastFetchAttemptTime));
  // Scheduler should prefer to use specified time over clock time.
  EXPECT_NE(test_clock()->Now(),
            pref_service()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, OnRequestErrorVerifyPref) {
  EXPECT_EQ(Time(), pref_service()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnRequestError(0);
  EXPECT_EQ(test_clock()->Now(),
            pref_service()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, OnForegroundedActiveNtpUser) {
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 24 hours.
  test_clock()->Advance(base::TimeDelta::FromHours(23));
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(2));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  variations::testing::VariationParamsManager variation_params(
      kInterestFeedContentSuggestions.name,
      {{"foregrounded_hours_active_ntp_user", "7.5"}},
      {kInterestFeedContentSuggestions.name});

  test_clock()->Advance(base::TimeDelta::FromHours(7));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(1));
  scheduler()->OnForegrounded();
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnForegroundedRareNtpUser) {
  ClassifyAsRareNtpUser();

  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 24 hours.
  test_clock()->Advance(base::TimeDelta::FromHours(23));
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(2));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  variations::testing::VariationParamsManager variation_params(
      kInterestFeedContentSuggestions.name,
      {{"foregrounded_hours_rare_ntp_user", "7.5"}},
      {kInterestFeedContentSuggestions.name});

  test_clock()->Advance(base::TimeDelta::FromHours(7));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(1));
  scheduler()->OnForegrounded();
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnForegroundedActiveSuggestionsConsumer) {
  ClassifyAsActiveSuggestionsConsumer();

  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 12 hours.
  test_clock()->Advance(base::TimeDelta::FromHours(11));
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(2));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  variations::testing::VariationParamsManager variation_params(
      kInterestFeedContentSuggestions.name,
      {{"foregrounded_hours_active_suggestions_consumer", "7.5"}},
      {kInterestFeedContentSuggestions.name});

  test_clock()->Advance(base::TimeDelta::FromHours(7));
  scheduler()->OnForegrounded();
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(1));
  scheduler()->OnForegrounded();
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerNullCallback) {
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerCompletionRunOnSuccess) {
  scheduler()->OnFixedTimer(base::BindOnce(
      &FeedSchedulerHostTest::FixedTimerCompletion, base::Unretained(this)));
  EXPECT_EQ(1, refresh_call_count());

  scheduler()->OnReceiveNewContent(Time());
  EXPECT_EQ(1, fixed_timer_completion_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerCompletionRunOnFailure) {
  scheduler()->OnFixedTimer(base::BindOnce(
      &FeedSchedulerHostTest::FixedTimerCompletion, base::Unretained(this)));
  EXPECT_EQ(1, refresh_call_count());

  scheduler()->OnRequestError(0);
  EXPECT_EQ(1, fixed_timer_completion_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerActiveNtpUser) {
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 48 hours.
  test_clock()->Advance(base::TimeDelta::FromHours(47));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(2));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  variations::testing::VariationParamsManager variation_params(
      kInterestFeedContentSuggestions.name,
      {{"fixed_timer_hours_active_ntp_user", "7.5"}},
      {kInterestFeedContentSuggestions.name});

  test_clock()->Advance(base::TimeDelta::FromHours(7));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(1));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerActiveRareNtpUser) {
  ClassifyAsRareNtpUser();

  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 96 hours.
  test_clock()->Advance(base::TimeDelta::FromHours(95));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(2));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  variations::testing::VariationParamsManager variation_params(
      kInterestFeedContentSuggestions.name,
      {{"fixed_timer_hours_rare_ntp_user", "7.5"}},
      {kInterestFeedContentSuggestions.name});

  test_clock()->Advance(base::TimeDelta::FromHours(7));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(1));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerActiveSuggestionsConsumer) {
  ClassifyAsActiveSuggestionsConsumer();

  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  // Default is 24 hours.
  test_clock()->Advance(base::TimeDelta::FromHours(23));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(1, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(2));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());
  scheduler()->OnReceiveNewContent(test_clock()->Now());

  variations::testing::VariationParamsManager variation_params(
      kInterestFeedContentSuggestions.name,
      {{"fixed_timer_hours_active_suggestions_consumer", "7.5"}},
      {kInterestFeedContentSuggestions.name});

  test_clock()->Advance(base::TimeDelta::FromHours(7));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(2, refresh_call_count());

  test_clock()->Advance(base::TimeDelta::FromHours(1));
  scheduler()->OnFixedTimer(base::OnceClosure());
  EXPECT_EQ(3, refresh_call_count());
}

TEST_F(FeedSchedulerHostTest, ScheduleFixedTimerWakeUpOnSuccess) {
  // First wake up scheduled during Initialize().
  EXPECT_EQ(1U, schedule_wake_up_times().size());
  scheduler()->OnReceiveNewContent(Time());
  EXPECT_EQ(2U, schedule_wake_up_times().size());

  // Make another scheduler to initialize, make sure it doesn't schedule a
  // wake up.
  FeedSchedulerHost second_scheduler(pref_service(), test_clock());
  InitializeScheduler(&second_scheduler);
  EXPECT_EQ(2U, schedule_wake_up_times().size());
}

TEST_F(FeedSchedulerHostTest, OnTaskReschedule) {
  EXPECT_EQ(1U, schedule_wake_up_times().size());
  scheduler()->OnTaskReschedule();
  EXPECT_EQ(2U, schedule_wake_up_times().size());
}

}  // namespace feed
