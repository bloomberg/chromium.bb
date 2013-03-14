// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/rate_limiter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class RateLimiterTest : public testing::Test {
 public:
  RateLimiterTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        clock_(new base::SimpleTestTickClock()),
        callbacks_(0),
        max_requests_(5),
        duration_(base::TimeDelta::FromHours(1)),
        small_delta_(base::TimeDelta::FromMinutes(1)),
        limiter_(max_requests_,
                 duration_,
                 base::Bind(&RateLimiterTest::Callback, base::Unretained(this)),
                 task_runner_,
                 scoped_ptr<base::TickClock>(clock_).Pass()) {}
  virtual ~RateLimiterTest() {}

 protected:
  void Callback() {
    callbacks_++;
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SimpleTestTickClock* clock_;
  size_t callbacks_;
  const size_t max_requests_;
  const base::TimeDelta duration_;
  const base::TimeDelta small_delta_;
  RateLimiter limiter_;
};

TEST_F(RateLimiterTest, LimitRequests) {
  size_t count = 0;
  for (size_t i = 0; i < max_requests_; ++i) {
    EXPECT_EQ(count, callbacks_);
    limiter_.PostRequest();
    ++count;
    EXPECT_EQ(count, callbacks_);
    EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
    clock_->Advance(small_delta_);
  }

  for (size_t i = 0; i < 10; ++i) {
    limiter_.PostRequest();
    EXPECT_EQ(max_requests_, callbacks_);
    clock_->Advance(small_delta_);
    EXPECT_FALSE(task_runner_->GetPendingTasks().empty());
  }

  // Now advance the clock beyond the duration. The callback is invoked once.
  callbacks_ = 0;
  clock_->Advance(duration_);
  task_runner_->RunPendingTasks();
  EXPECT_EQ(1u, callbacks_);
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(RateLimiterTest, Steady) {
  const base::TimeDelta delta = duration_ / 2;
  size_t count = 0;
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(count, callbacks_);
    limiter_.PostRequest();
    ++count;
    EXPECT_EQ(count, callbacks_);
    EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
    clock_->Advance(delta);
  }
}

TEST_F(RateLimiterTest, RetryAfterDelay) {
  size_t count = 0;
  base::TimeDelta total_delta;
  // Fill the queue.
  for (size_t i = 0; i < max_requests_; ++i) {
    EXPECT_EQ(count, callbacks_);
    limiter_.PostRequest();
    ++count;
    EXPECT_EQ(count, callbacks_);
    EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
    clock_->Advance(small_delta_);
    total_delta += small_delta_;
  }

  // Now post a request that will be delayed.
  EXPECT_EQ(max_requests_, callbacks_);
  limiter_.PostRequest();
  EXPECT_EQ(max_requests_, callbacks_);
  EXPECT_FALSE(task_runner_->GetPendingTasks().empty());

  while (total_delta < duration_) {
    task_runner_->RunPendingTasks();
    // The queue is still full, so another task is immediately posted.
    EXPECT_FALSE(task_runner_->GetPendingTasks().empty());
    clock_->Advance(small_delta_);
    total_delta += small_delta_;
  }

  // Now advance time beyond the initial duration. It will immediately execute
  // the callback.
  EXPECT_EQ(max_requests_, callbacks_);
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
  EXPECT_EQ(max_requests_ + 1, callbacks_);
}

}  // namespace policy
