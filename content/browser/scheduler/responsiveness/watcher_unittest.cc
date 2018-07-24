// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/watcher.h"

#include "base/location.h"
#include "base/pending_task.h"
#include "content/browser/scheduler/responsiveness/calculator.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace responsiveness {

namespace {

class FakeCalculator : public Calculator {
 public:
  void TaskOrEventFinishedOnUIThread(base::TimeTicks schedule_time,
                                     base::TimeTicks finish_time) override {
    ++tasks_on_ui_thread_;
  }

  void TaskOrEventFinishedOnIOThread(base::TimeTicks schedule_time,
                                     base::TimeTicks finish_time) override {
    ++tasks_on_io_thread_;
  }

  int NumTasksOnUIThread() { return tasks_on_ui_thread_; }
  int NumTasksOnIOThread() { return tasks_on_io_thread_; }

 private:
  int tasks_on_ui_thread_ = 0;
  int tasks_on_io_thread_ = 0;
};

class FakeWatcher : public Watcher {
 public:
  std::unique_ptr<Calculator> MakeCalculator() override {
    std::unique_ptr<FakeCalculator> calculator =
        std::make_unique<FakeCalculator>();
    calculator_ = calculator.get();
    return calculator;
  }

  FakeWatcher() : Watcher() {}

  int NumTasksOnUIThread() { return calculator_->NumTasksOnUIThread(); }
  int NumTasksOnIOThread() { return calculator_->NumTasksOnIOThread(); }

 private:
  ~FakeWatcher() override{};
  FakeCalculator* calculator_ = nullptr;
};

}  // namespace

class ResponsivenessWatcherTest : public testing::Test {
 public:
  void SetUp() override {
    // Watcher's constructor posts a task to IO thread, which in the unit test
    // is also this thread. Regardless, we need to let those tasks finish.
    watcher_ = scoped_refptr<FakeWatcher>(new FakeWatcher);
    watcher_->SetUp();
    test_browser_thread_bundle_.RunUntilIdle();
  }

  void TearDown() override {
    watcher_->Destroy();
    watcher_.reset();
  }

 protected:
  // This member sets up BrowserThread::IO and BrowserThread::UI. It must be the
  // first member, as other members may depend on these abstractions.
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  scoped_refptr<FakeWatcher> watcher_;
};

// Test that tasks are forwarded to calculator.
TEST_F(ResponsivenessWatcherTest, TaskForwarding) {
  for (int i = 0; i < 3; ++i) {
    base::PendingTask task(FROM_HERE, base::OnceClosure());
    watcher_->WillRunTaskOnUIThread(&task);
    watcher_->DidRunTaskOnUIThread(&task);
  }
  EXPECT_EQ(3, watcher_->NumTasksOnUIThread());
  EXPECT_EQ(0, watcher_->NumTasksOnIOThread());

  for (int i = 0; i < 4; ++i) {
    base::PendingTask task(FROM_HERE, base::OnceClosure());
    watcher_->WillRunTaskOnIOThread(&task);
    watcher_->DidRunTaskOnIOThread(&task);
  }
  EXPECT_EQ(3, watcher_->NumTasksOnUIThread());
  EXPECT_EQ(4, watcher_->NumTasksOnIOThread());
}

// Test that nested tasks are not forwarded to the calculator.
TEST_F(ResponsivenessWatcherTest, TaskNesting) {
  // TODO(erikchen): Check that the right task is forwarded to the calculator.
  // Requires implementation of PendingTask::queue_time.
  base::PendingTask task1(FROM_HERE, base::OnceClosure());
  base::PendingTask task2(FROM_HERE, base::OnceClosure());
  base::PendingTask task3(FROM_HERE, base::OnceClosure());

  watcher_->WillRunTaskOnUIThread(&task1);
  watcher_->WillRunTaskOnUIThread(&task2);
  watcher_->WillRunTaskOnUIThread(&task3);
  watcher_->DidRunTaskOnUIThread(&task3);
  watcher_->DidRunTaskOnUIThread(&task2);
  watcher_->DidRunTaskOnUIThread(&task1);

  EXPECT_EQ(1, watcher_->NumTasksOnUIThread());
  EXPECT_EQ(0, watcher_->NumTasksOnIOThread());
}

}  // namespace responsiveness
