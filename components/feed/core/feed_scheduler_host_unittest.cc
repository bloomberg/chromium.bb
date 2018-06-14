// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_scheduler_host.h"

#include "base/test/simple_test_clock.h"
#include "components/feed/core/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

// Fixed "now" to make tests more deterministic.
char kNowString[] = "2018-06-11 15:41";

using base::Time;

class FeedSchedulerHostTest : public ::testing::Test {
 public:
 protected:
  FeedSchedulerHostTest() : scheduler_(&pref_service_, &test_clock_) {
    base::Time now;
    CHECK(base::Time::FromUTCString(kNowString, &now));
    test_clock_.SetNow(now);

    FeedSchedulerHost::RegisterProfilePrefs(pref_service_.registry());
  }

  PrefService* pref_service() { return &pref_service_; }
  base::SimpleTestClock* test_clock() { return &test_clock_; }
  FeedSchedulerHost* scheduler() { return &scheduler_; }

 private:
  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock test_clock_;
  FeedSchedulerHost scheduler_;
};

TEST_F(FeedSchedulerHostTest, OnReceiveNewContent) {
  EXPECT_EQ(Time(), pref_service()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnReceiveNewContent(Time());
  EXPECT_EQ(test_clock()->Now(),
            pref_service()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, OnRequestError) {
  EXPECT_EQ(Time(), pref_service()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnRequestError(0);
  EXPECT_EQ(test_clock()->Now(),
            pref_service()->GetTime(prefs::kLastFetchAttemptTime));
}

}  // namespace

}  // namespace feed
