// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/delayed_callback_runner.h"

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A class of objects that invoke a callback upon destruction. This is used as
// an owned argument on callbacks given to a DelayedCallbackRunner under test.
class CallbackArgument {
 public:
  explicit CallbackArgument(const base::Closure& on_delete)
      : on_delete_(on_delete) {}
  ~CallbackArgument() { on_delete_.Run(); }

 private:
  base::Closure on_delete_;

  DISALLOW_COPY_AND_ASSIGN(CallbackArgument);
};

}  // namespace

// A test fixture that prepares a DelayedCallbackRunner instance for use and
// tracks the lifecycle of callbacks sent to it.
class DelayedCallbackRunnerTest : public testing::Test {
 public:
  // Registers a callback that will record its running and destruction to the
  // test fixture under the given name.
  void RegisterTestCallback(const std::string& name) {
    callbacks_[name] = CallbackState();
    instance_->RegisterCallback(MakeCallback(name));
  }

 protected:
  DelayedCallbackRunnerTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        thread_task_runner_handle_(task_runner_) {}

  virtual void SetUp() OVERRIDE {
    instance_.reset(new safe_browsing::DelayedCallbackRunner(
        base::TimeDelta::FromMilliseconds(1),  // ignored by simple runner.
        task_runner_));
  }

  virtual void TearDown() OVERRIDE { instance_.reset(); }

  void OnRun(const std::string& name, CallbackArgument* arg) {
    EXPECT_FALSE(callbacks_[name].run);
    callbacks_[name].run = true;
  }

  void OnDelete(const std::string& name) {
    EXPECT_FALSE(callbacks_[name].deleted);
    callbacks_[name].deleted = true;
  }

  // Returns a callback argument that calls the test fixture's OnDelete method
  // on behalf of the given callback name.
  scoped_ptr<CallbackArgument> MakeCallbackArgument(const std::string& name) {
    return make_scoped_ptr(new CallbackArgument(base::Bind(
        &DelayedCallbackRunnerTest::OnDelete, base::Unretained(this), name)));
  }

  // Returns a closure that calls |OnRun| when run and |OnDelete| when deleted
  // on behalf of the given callback name.
  base::Closure MakeCallback(const std::string& name) {
    return base::Bind(&DelayedCallbackRunnerTest::OnRun,
                      base::Unretained(this),
                      name,
                      base::Owned(MakeCallbackArgument(name).release()));
  }

  bool CallbackWasRun(const std::string& name) { return callbacks_[name].run; }

  bool CallbackWasDeleted(const std::string& name) {
    return callbacks_[name].deleted;
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  scoped_ptr<safe_browsing::DelayedCallbackRunner> instance_;

 private:
  struct CallbackState {
    CallbackState() : run(), deleted() {}
    bool run;
    bool deleted;
  };

  std::map<std::string, CallbackState> callbacks_;
};

// Tests that a callback is deleted when not run before the runner is destroyed.
TEST_F(DelayedCallbackRunnerTest, NotRunDeleted) {
  const std::string name("one");
  RegisterTestCallback(name);
  instance_.reset();
  EXPECT_FALSE(CallbackWasRun(name));
  EXPECT_TRUE(CallbackWasDeleted(name));
}

// Tests that a callback is run and deleted while the runner is alive.
TEST_F(DelayedCallbackRunnerTest, RunDeleted) {
  const std::string name("one");
  RegisterTestCallback(name);
  instance_->Start();
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(CallbackWasRun(name));
  EXPECT_TRUE(CallbackWasDeleted(name));
}

// Tests that a callback registered after Start() is called is also run and
// deleted.
TEST_F(DelayedCallbackRunnerTest, AddWhileRunningRun) {
  const std::string name("one");
  const std::string name2("two");

  // Post a task to register a new callback after Start() is called.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DelayedCallbackRunnerTest::RegisterTestCallback,
                 base::Unretained(this),
                 name2));

  RegisterTestCallback(name);
  instance_->Start();
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(CallbackWasRun(name));
  EXPECT_TRUE(CallbackWasDeleted(name));
  EXPECT_TRUE(CallbackWasRun(name2));
  EXPECT_TRUE(CallbackWasDeleted(name2));
}

TEST_F(DelayedCallbackRunnerTest, MultipleRuns) {
  const std::string name("one");
  const std::string name2("two");

  RegisterTestCallback(name);
  instance_->Start();
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(CallbackWasRun(name));
  EXPECT_TRUE(CallbackWasDeleted(name));

  RegisterTestCallback(name2);
  instance_->Start();
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(CallbackWasRun(name2));
  EXPECT_TRUE(CallbackWasDeleted(name2));
}
