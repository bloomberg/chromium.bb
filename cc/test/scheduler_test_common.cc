// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/scheduler_test_common.h"

#include "base/logging.h"

namespace cc {

void FakeTimeSourceClient::OnTimerTick() { tick_called_ = true; }

FakeThread::FakeThread() { Reset(); }

FakeThread::~FakeThread() {}

void FakeThread::RunPendingTask() {
  ASSERT_TRUE(pending_task_);
  scoped_ptr<base::Closure> task = pending_task_.Pass();
  task->Run();
}

void FakeThread::PostTask(base::Closure cb) {
  PostDelayedTask(cb, base::TimeDelta());
}

void FakeThread::PostDelayedTask(base::Closure cb, base::TimeDelta delay) {
  if (run_pending_task_on_overwrite_ && HasPendingTask())
    RunPendingTask();

  ASSERT_FALSE(HasPendingTask());
  pending_task_.reset(new base::Closure(cb));
  pending_task_delay_ = delay.InMilliseconds();
}

bool FakeThread::BelongsToCurrentThread() const { return true; }

void FakeTimeSource::SetClient(TimeSourceClient* client) { client_ = client; }

void FakeTimeSource::SetActive(bool b) { active_ = b; }

bool FakeTimeSource::Active() const { return active_; }

base::TimeTicks FakeTimeSource::LastTickTime() { return base::TimeTicks(); }

base::TimeTicks FakeTimeSource::NextTickTime() { return base::TimeTicks(); }

base::TimeTicks FakeDelayBasedTimeSource::Now() const { return now_; }

}  // namespace cc
