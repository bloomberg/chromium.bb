// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/test_util.h"

#include "base/bind.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

namespace {

class MockTimer : public MockableTime::Timer {
 public:
  MockTimer(MockTime* time)
      : time_(time),
        running_(false),
        callback_sequence_number_(0),
        weak_factory_(this) {
    DCHECK(time);
  }
  virtual ~MockTimer() {}

  virtual void Start(const tracked_objects::Location& posted_from,
                     base::TimeDelta delay,
                     const base::Closure& user_task) OVERRIDE {
    DCHECK(!user_task.is_null());

    if (running_)
      ++callback_sequence_number_;
    running_ = true;
    user_task_ = user_task;
    time_->AddTask(delay,
        base::Bind(&MockTimer::OnDelayPassed,
                   weak_factory_.GetWeakPtr(),
                   callback_sequence_number_));
  }

  virtual void Stop() OVERRIDE {
    if (running_) {
      ++callback_sequence_number_;
      running_ = false;
    }
  }

  virtual bool IsRunning() OVERRIDE { return running_; }

 private:
  void OnDelayPassed(int expected_callback_sequence_number) {
    if (callback_sequence_number_ != expected_callback_sequence_number)
      return;

    DCHECK(running_);
    running_ = false;

    // Grab user task in case it re-entrantly starts the timer again.
    base::Closure task_to_run = user_task_;
    user_task_.Reset();
    task_to_run.Run();
  }

  MockTime* time_;
  bool running_;
  int callback_sequence_number_;
  base::Closure user_task_;
  base::WeakPtrFactory<MockTimer> weak_factory_;
};

}  // namespace

TestCallback::TestCallback()
    : callback_(base::Bind(&TestCallback::OnCalled,
                           base::Unretained(this))),
      called_(false) {}

TestCallback::~TestCallback() {}

void TestCallback::OnCalled() {
  EXPECT_FALSE(called_);
  called_ = true;
}

MockTime::MockTime()
    : now_(base::TimeTicks::Now()),
      epoch_(now_),
      task_sequence_number_(0) {
  VLOG(1) << "Creating mock time: T=" << elapsed_sec() << "s";
}

MockTime::~MockTime() {}

base::TimeTicks MockTime::Now() { return now_; }

scoped_ptr<MockableTime::Timer> MockTime::CreateTimer() {
  return scoped_ptr<MockableTime::Timer>(new MockTimer(this));
}

void MockTime::Advance(base::TimeDelta delta) {
  base::TimeTicks target = now_ + delta;

  while (!tasks_.empty() && tasks_.begin()->first.time <= target) {
    TaskKey key = tasks_.begin()->first;
    base::Closure task = tasks_.begin()->second;
    tasks_.erase(tasks_.begin());

    DCHECK(now_ <= key.time);
    DCHECK(key.time <= target);
    now_ = key.time;
    VLOG(1) << "Advancing mock time: task at T=" << elapsed_sec() << "s";

    task.Run();
  }

  DCHECK(now_ <= target);
  now_ = target;
  VLOG(1) << "Advanced mock time: T=" << elapsed_sec() << "s";
}

void MockTime::AddTask(base::TimeDelta delay, const base::Closure& task) {
  tasks_[TaskKey(now_ + delay, task_sequence_number_++)] = task;
}

}  // namespace domain_reliability
