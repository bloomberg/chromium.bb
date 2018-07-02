// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_scheduler_host.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/simple_test_clock.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/core/time_serialization.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

// Fixed "now" to make tests more deterministic.
char kNowString[] = "2018-06-11 15:41";

using base::Time;

class FeedSchedulerHostTest : public ::testing::Test {
 public:
  void FixedTimerCompletion() { fixed_timer_completion_count_++; }

 protected:
  FeedSchedulerHostTest()
      : scheduler_(&pref_service_, &test_clock_), weak_factory_(this) {
    Time now;
    CHECK(Time::FromUTCString(kNowString, &now));
    test_clock_.SetNow(now);
    FeedSchedulerHost::RegisterProfilePrefs(pref_service_.registry());
    InitializeScheduler(scheduler());
  }

  void InitializeScheduler(FeedSchedulerHost* scheduler) {
    scheduler->Initialize(
        base::BindRepeating(&FeedSchedulerHostTest::TriggerRefresh,
                            base::Unretained(this)),
        base::BindRepeating(&FeedSchedulerHostTest::ScheduleWakeUp,
                            base::Unretained(this)));
  }

  PrefService* pref_service() { return &pref_service_; }
  base::SimpleTestClock* test_clock() { return &test_clock_; }
  FeedSchedulerHost* scheduler() { return &scheduler_; }
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
  FeedSchedulerHost scheduler_;
  int refresh_call_count_ = 0;
  std::vector<base::TimeDelta> schedule_wake_up_times_;
  int cancel_wake_up_call_count_ = 0;
  int fixed_timer_completion_count_ = 0;
  base::WeakPtrFactory<FeedSchedulerHostTest> weak_factory_;
};

TEST_F(FeedSchedulerHostTest, ShouldSessionRequestDataSimple) {
  EXPECT_EQ(REQUEST_WITH_WAIT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));
  // TODO(skym): REQUEST_WITH_TIMEOUT.
  EXPECT_EQ(REQUEST_WITH_CONTENT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));
  EXPECT_EQ(NO_REQUEST_WITH_WAIT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ true));
  // TODO(skym): NO_REQUEST_WITH_TIMEOUT.
  EXPECT_EQ(NO_REQUEST_WITH_CONTENT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ true));
}

TEST_F(FeedSchedulerHostTest, OnReceiveNewContentVerifyPref) {
  EXPECT_EQ(Time(), pref_service()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnReceiveNewContent(Time());
  EXPECT_EQ(test_clock()->Now(),
            pref_service()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, OnRequestErrorVerifyPref) {
  EXPECT_EQ(Time(), pref_service()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnRequestError(0);
  EXPECT_EQ(test_clock()->Now(),
            pref_service()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, OnForegroundedTriggersRefresh) {
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, refresh_call_count());
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

}  // namespace

}  // namespace feed
