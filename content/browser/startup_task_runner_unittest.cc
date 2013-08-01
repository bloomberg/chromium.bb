// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/startup_task_runner.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task_runner.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using base::Closure;
using testing::_;
using testing::Assign;
using testing::Invoke;
using testing::WithArg;

bool observer_called = false;
int observer_result;
base::Closure task;

// I couldn't get gMock's SaveArg to compile, hence had to save the argument
// this way
bool SaveTaskArg(const Closure& arg) {
  task = arg;
  return true;
}

void Observer(int result) {
  observer_called = true;
  observer_result = result;
}

class StartupTaskRunnerTest : public testing::Test {
 public:

  virtual void SetUp() {
    last_task_ = 0;
    observer_called = false;
  }

  int Task1() {
    last_task_ = 1;
    return 0;
  }

  int Task2() {
    last_task_ = 2;
    return 0;
  }

  int FailingTask() {
    // Task returning failure
    last_task_ = 3;
    return 1;
  }

  int GetLastTask() { return last_task_; }

 private:

  int last_task_;
};

// We can't use the real message loop, even if we want to, since doing so on
// Android requires a complex Java infrastructure. The test would have to built
// as a content_shell test; but content_shell startup invokes the class we are
// trying to test.
//
// The mocks are not directly in TaskRunnerProxy because reference counted
// objects seem to confuse the mocking framework

class MockTaskRunner {
 public:
  MOCK_METHOD3(
      PostDelayedTask,
      bool(const tracked_objects::Location&, const Closure&, base::TimeDelta));
  MOCK_METHOD3(
      PostNonNestableDelayedTask,
      bool(const tracked_objects::Location&, const Closure&, base::TimeDelta));
};

class TaskRunnerProxy : public base::SingleThreadTaskRunner {
 public:
  TaskRunnerProxy(MockTaskRunner* mock) : mock_(mock) {}
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE { return true; }
  virtual bool PostDelayedTask(const tracked_objects::Location& location,
                               const Closure& closure,
                               base::TimeDelta delta) OVERRIDE {
    return mock_->PostDelayedTask(location, closure, delta);
  }
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& location,
      const Closure& closure,
      base::TimeDelta delta) OVERRIDE {
    return mock_->PostNonNestableDelayedTask(location, closure, delta);
  }

 private:
  MockTaskRunner* mock_;
  virtual ~TaskRunnerProxy() {}
};

TEST_F(StartupTaskRunnerTest, SynchronousExecution) {
  MockTaskRunner mock_runner;
  scoped_refptr<TaskRunnerProxy> proxy = new TaskRunnerProxy(&mock_runner);

  EXPECT_CALL(mock_runner, PostDelayedTask(_, _, _)).Times(0);
  EXPECT_CALL(mock_runner, PostNonNestableDelayedTask(_, _, _)).Times(0);

  scoped_refptr<StartupTaskRunner> runner =
      new StartupTaskRunner(false, base::Bind(&Observer), proxy);

  StartupTask task1 =
      base::Bind(&StartupTaskRunnerTest::Task1, base::Unretained(this));
  runner->AddTask(task1);
  EXPECT_EQ(GetLastTask(), 0);
  StartupTask task2 =
      base::Bind(&StartupTaskRunnerTest::Task2, base::Unretained(this));
  runner->AddTask(task2);

  // Nothing should run until we tell them to.
  EXPECT_EQ(GetLastTask(), 0);
  runner->StartRunningTasks();

  // On an immediate StartupTaskRunner the tasks should now all have run.
  EXPECT_EQ(GetLastTask(), 2);

  EXPECT_TRUE(observer_called);
  EXPECT_EQ(observer_result, 0);
}

TEST_F(StartupTaskRunnerTest, NullObserver) {
  MockTaskRunner mock_runner;
  scoped_refptr<TaskRunnerProxy> proxy = new TaskRunnerProxy(&mock_runner);

  EXPECT_CALL(mock_runner, PostDelayedTask(_, _, _)).Times(0);
  EXPECT_CALL(mock_runner, PostNonNestableDelayedTask(_, _, _)).Times(0);

  scoped_refptr<StartupTaskRunner> runner =
      new StartupTaskRunner(false, base::Callback<void(int)>(), proxy);

  StartupTask task1 =
      base::Bind(&StartupTaskRunnerTest::Task1, base::Unretained(this));
  runner->AddTask(task1);
  EXPECT_EQ(GetLastTask(), 0);
  StartupTask task2 =
      base::Bind(&StartupTaskRunnerTest::Task2, base::Unretained(this));
  runner->AddTask(task2);

  // Nothing should run until we tell them to.
  EXPECT_EQ(GetLastTask(), 0);
  runner->StartRunningTasks();

  // On an immediate StartupTaskRunner the tasks should now all have run.
  EXPECT_EQ(GetLastTask(), 2);

  EXPECT_FALSE(observer_called);
}

TEST_F(StartupTaskRunnerTest, SynchronousExecutionFailedTask) {
  MockTaskRunner mock_runner;
  scoped_refptr<TaskRunnerProxy> proxy = new TaskRunnerProxy(&mock_runner);

  EXPECT_CALL(mock_runner, PostDelayedTask(_, _, _)).Times(0);
  EXPECT_CALL(mock_runner, PostNonNestableDelayedTask(_, _, _)).Times(0);

  scoped_refptr<StartupTaskRunner> runner =
      new StartupTaskRunner(false, base::Bind(&Observer), proxy);

  StartupTask task3 =
      base::Bind(&StartupTaskRunnerTest::FailingTask, base::Unretained(this));
  runner->AddTask(task3);
  EXPECT_EQ(GetLastTask(), 0);
  StartupTask task2 =
      base::Bind(&StartupTaskRunnerTest::Task2, base::Unretained(this));
  runner->AddTask(task2);

  // Nothing should run until we tell them to.
  EXPECT_EQ(GetLastTask(), 0);
  runner->StartRunningTasks();

  // Only the first task should have run, since it failed
  EXPECT_EQ(GetLastTask(), 3);

  EXPECT_TRUE(observer_called);
  EXPECT_EQ(observer_result, 1);
}

TEST_F(StartupTaskRunnerTest, AsynchronousExecution) {

  MockTaskRunner mock_runner;
  scoped_refptr<TaskRunnerProxy> proxy = new TaskRunnerProxy(&mock_runner);

  EXPECT_CALL(mock_runner, PostDelayedTask(_, _, _)).Times(0);
  EXPECT_CALL(
      mock_runner,
      PostNonNestableDelayedTask(_, _, base::TimeDelta::FromMilliseconds(0)))
      .Times(testing::Between(2, 3))
      .WillRepeatedly(WithArg<1>(Invoke(SaveTaskArg)));

  scoped_refptr<StartupTaskRunner> runner =
      new StartupTaskRunner(true, base::Bind(&Observer), proxy);

  StartupTask task1 =
      base::Bind(&StartupTaskRunnerTest::Task1, base::Unretained(this));
  runner->AddTask(task1);
  StartupTask task2 =
      base::Bind(&StartupTaskRunnerTest::Task2, base::Unretained(this));
  runner->AddTask(task2);

  // Nothing should run until we tell them to.
  EXPECT_EQ(GetLastTask(), 0);
  runner->StartRunningTasks();

  // No tasks should have run yet, since we the message loop hasn't run.
  EXPECT_EQ(GetLastTask(), 0);

  // Fake the actual message loop. Each time a task is run a new task should
  // be added to the queue, hence updating "task". The loop should actually run
  // at most 3 times (once for each task plus possibly once for the observer),
  // the "4" is a backstop.
  for (int i = 0; i < 4 && !observer_called; i++) {
    task.Run();
    EXPECT_EQ(i + 1, GetLastTask());
  }
  EXPECT_TRUE(observer_called);
  EXPECT_EQ(observer_result, 0);
}

TEST_F(StartupTaskRunnerTest, AsynchronousExecutionFailedTask) {

  MockTaskRunner mock_runner;
  scoped_refptr<TaskRunnerProxy> proxy = new TaskRunnerProxy(&mock_runner);

  EXPECT_CALL(mock_runner, PostDelayedTask(_, _, _)).Times(0);
  EXPECT_CALL(
      mock_runner,
      PostNonNestableDelayedTask(_, _, base::TimeDelta::FromMilliseconds(0)))
      .Times(testing::Between(1, 2))
      .WillRepeatedly(WithArg<1>(Invoke(SaveTaskArg)));

  scoped_refptr<StartupTaskRunner> runner =
      new StartupTaskRunner(true, base::Bind(&Observer), proxy);

  StartupTask task3 =
      base::Bind(&StartupTaskRunnerTest::FailingTask, base::Unretained(this));
  runner->AddTask(task3);
  StartupTask task2 =
      base::Bind(&StartupTaskRunnerTest::Task2, base::Unretained(this));
  runner->AddTask(task2);

  // Nothing should run until we tell them to.
  EXPECT_EQ(GetLastTask(), 0);
  runner->StartRunningTasks();

  // No tasks should have run yet, since we the message loop hasn't run.
  EXPECT_EQ(GetLastTask(), 0);

  // Fake the actual message loop. Each time a task is run a new task should
  // be added to the queue, hence updating "task". The loop should actually run
  // at most twice (once for the failed task plus possibly once for the
  // observer), the "4" is a backstop.
  for (int i = 0; i < 4 && !observer_called; i++) {
    task.Run();
  }
  EXPECT_EQ(GetLastTask(), 3);

  EXPECT_TRUE(observer_called);
  EXPECT_EQ(observer_result, 1);
}
}  // namespace
}  // namespace content
