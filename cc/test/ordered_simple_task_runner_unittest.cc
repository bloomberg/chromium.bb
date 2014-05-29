// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_pending_task.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class OrderedSimpleTaskRunnerTest : public testing::Test {
 public:
  OrderedSimpleTaskRunnerTest() {
    task_runner_ = new OrderedSimpleTaskRunner;
  }
  virtual ~OrderedSimpleTaskRunnerTest() {}

 protected:
  void CreateAndPostTask(int task_num, base::TimeDelta delay) {
    base::Closure test_task = base::Bind(&OrderedSimpleTaskRunnerTest::Task,
                                         base::Unretained(this),
                                         task_num);
    task_runner_->PostDelayedTask(FROM_HERE, test_task, delay);
  }

  void RunAndCheckResult(const std::string expected_result) {
    task_runner_->RunPendingTasks();
    EXPECT_EQ(expected_result, executed_tasks_);
  }

 private:
  std::string executed_tasks_;
  scoped_refptr<OrderedSimpleTaskRunner> task_runner_;

  void Task(int task_num) {
    if (!executed_tasks_.empty())
      executed_tasks_ += " ";
    executed_tasks_ += base::StringPrintf("%d", task_num);
  }

  DISALLOW_COPY_AND_ASSIGN(OrderedSimpleTaskRunnerTest);
};

TEST_F(OrderedSimpleTaskRunnerTest, BasicOrderingTest) {
  CreateAndPostTask(1, base::TimeDelta());
  CreateAndPostTask(2, base::TimeDelta());
  CreateAndPostTask(3, base::TimeDelta());

  RunAndCheckResult("1 2 3");
}

TEST_F(OrderedSimpleTaskRunnerTest, OrderingTestWithDelayedTasks) {
  CreateAndPostTask(1, base::TimeDelta());
  CreateAndPostTask(2, base::TimeDelta::FromMilliseconds(15));
  CreateAndPostTask(3, base::TimeDelta());
  CreateAndPostTask(4, base::TimeDelta::FromMilliseconds(8));

  RunAndCheckResult("1 3 4 2");
}

}  // namespace cc
