// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_c.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "components/cronet/native/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ExecutorsTest : public ::testing::Test {
 public:
  ExecutorsTest() = default;
  ~ExecutorsTest() override = default;

 protected:
  static void TestRunnable_Run(Cronet_RunnablePtr self);
  bool runnable_called() const { return runnable_called_; }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  void set_runnable_called(bool value) { runnable_called_ = value; }

  bool runnable_called_ = false;
  DISALLOW_COPY_AND_ASSIGN(ExecutorsTest);
};

// App implementation of Cronet_Executor methods.
void TestExecutor_Execute(Cronet_ExecutorPtr self, Cronet_RunnablePtr command) {
  CHECK(self);
  Cronet_Runnable_Run(command);
  Cronet_Runnable_Destroy(command);
}

// Implementation of TestRunnable methods.
// static
void ExecutorsTest::TestRunnable_Run(Cronet_RunnablePtr self) {
  CHECK(self);
  Cronet_RunnableContext context = Cronet_Runnable_GetContext(self);
  ExecutorsTest* test = static_cast<ExecutorsTest*>(context);
  CHECK(test);
  test->set_runnable_called(true);
}

// Test that custom Executor defined by the app runs the runnable.
TEST_F(ExecutorsTest, TestCustom) {
  ASSERT_FALSE(runnable_called());
  Cronet_RunnablePtr runnable =
      Cronet_Runnable_CreateStub(ExecutorsTest::TestRunnable_Run);
  Cronet_Runnable_SetContext(runnable, this);
  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateStub(TestExecutor_Execute);
  Cronet_Executor_Execute(executor, runnable);
  Cronet_Executor_Destroy(executor);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(runnable_called());
}

// Test that cronet::test::TestExecutor runs the runnable.
TEST_F(ExecutorsTest, TestTestExecutor) {
  ASSERT_FALSE(runnable_called());
  Cronet_RunnablePtr runnable = Cronet_Runnable_CreateStub(TestRunnable_Run);
  Cronet_Runnable_SetContext(runnable, this);
  Cronet_ExecutorPtr executor = cronet::test::CreateTestExecutor();
  Cronet_Executor_Execute(executor, runnable);
  Cronet_Executor_Destroy(executor);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(runnable_called());
}

}  // namespace
