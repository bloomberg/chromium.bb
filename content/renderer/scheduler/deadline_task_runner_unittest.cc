// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/deadline_task_runner.h"

#include "cc/test/ordered_simple_task_runner.h"
#include "cc/test/test_now_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class DeadlineTaskRunnerTest : public testing::Test {
 public:
  DeadlineTaskRunnerTest() {}
  ~DeadlineTaskRunnerTest() override {}

  void SetUp() override {
    clock_ = cc::TestNowSource::Create(5000);
    mock_task_runner_ = new cc::OrderedSimpleTaskRunner(clock_, true);
    deadline_task_runner_.reset(new DeadlineTaskRunner(
        base::Bind(&DeadlineTaskRunnerTest::TestTask, base::Unretained(this)),
        mock_task_runner_));
    run_times_.clear();
  }

  bool RunUntilIdle() { return mock_task_runner_->RunUntilIdle(); }

  void TestTask() { run_times_.push_back(clock_->Now()); }

  scoped_refptr<cc::TestNowSource> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_ptr<DeadlineTaskRunner> deadline_task_runner_;
  std::vector<base::TimeTicks> run_times_;
};

TEST_F(DeadlineTaskRunnerTest, RunOnce) {
  base::TimeTicks start_time = clock_->Now();
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  deadline_task_runner_->SetDeadline(FROM_HERE, delay, clock_->Now());
  RunUntilIdle();

  EXPECT_THAT(run_times_, testing::ElementsAre(start_time + delay));
};

TEST_F(DeadlineTaskRunnerTest, RunTwice) {
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks deadline1 = clock_->Now() + delay1;
  deadline_task_runner_->SetDeadline(FROM_HERE, delay1, clock_->Now());
  RunUntilIdle();

  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks deadline2 = clock_->Now() + delay2;
  deadline_task_runner_->SetDeadline(FROM_HERE, delay2, clock_->Now());
  RunUntilIdle();

  EXPECT_THAT(run_times_, testing::ElementsAre(deadline1, deadline2));
};

TEST_F(DeadlineTaskRunnerTest, EarlierDeadlinesTakePrecidence) {
  base::TimeTicks start_time = clock_->Now();
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta delay10 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay100 = base::TimeDelta::FromMilliseconds(100);
  deadline_task_runner_->SetDeadline(FROM_HERE, delay100, clock_->Now());
  deadline_task_runner_->SetDeadline(FROM_HERE, delay10, clock_->Now());
  deadline_task_runner_->SetDeadline(FROM_HERE, delay1, clock_->Now());

  RunUntilIdle();

  EXPECT_THAT(run_times_, testing::ElementsAre(start_time + delay1));
};

TEST_F(DeadlineTaskRunnerTest, LaterDeadlinesIgnored) {
  base::TimeTicks start_time = clock_->Now();
  base::TimeDelta delay100 = base::TimeDelta::FromMilliseconds(100);
  base::TimeDelta delay10000 = base::TimeDelta::FromMilliseconds(10000);
  deadline_task_runner_->SetDeadline(FROM_HERE, delay100, clock_->Now());
  deadline_task_runner_->SetDeadline(FROM_HERE, delay10000, clock_->Now());

  RunUntilIdle();

  EXPECT_THAT(run_times_, testing::ElementsAre(start_time + delay100));
};

TEST_F(DeadlineTaskRunnerTest, DeleteDeadlineTaskRunnerAfterPosting) {
  deadline_task_runner_->SetDeadline(
      FROM_HERE, base::TimeDelta::FromMilliseconds(10), clock_->Now());

  // Deleting the pending task should cancel it.
  deadline_task_runner_.reset(nullptr);
  RunUntilIdle();

  EXPECT_TRUE(run_times_.empty());
};

}  // namespace content
