// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/scheduler_test_common.h"

#include <string>

#include "base/logging.h"

namespace cc {

void FakeTimeSourceClient::OnTimerTick() {
  tick_called_ = true;
}

base::TimeTicks FakeDelayBasedTimeSource::Now() const { return now_; }

TestDelayBasedTimeSource::TestDelayBasedTimeSource(
    scoped_refptr<TestNowSource> now_src,
    base::TimeDelta interval,
    OrderedSimpleTaskRunner* task_runner)
    : DelayBasedTimeSource(interval, task_runner), now_src_(now_src) {
}

base::TimeTicks TestDelayBasedTimeSource::Now() const {
  return now_src_->Now();
}

std::string TestDelayBasedTimeSource::TypeString() const {
  return "TestDelayBasedTimeSource";
}

TestDelayBasedTimeSource::~TestDelayBasedTimeSource() {
}

TestScheduler::TestScheduler(
    scoped_refptr<TestNowSource> now_src,
    SchedulerClient* client,
    const SchedulerSettings& scheduler_settings,
    int layer_tree_host_id,
    const scoped_refptr<OrderedSimpleTaskRunner>& test_task_runner)
    : Scheduler(client,
                scheduler_settings,
                layer_tree_host_id,
                test_task_runner),
      now_src_(now_src),
      test_task_runner_(test_task_runner.get()) {
  if (!settings_.begin_frame_scheduling_enabled) {
    scoped_refptr<DelayBasedTimeSource> time_source =
        TestDelayBasedTimeSource::Create(
            now_src, VSyncInterval(), test_task_runner_);
    synthetic_begin_frame_source_.reset(
        new SyntheticBeginFrameSource(this, time_source));
  }
}

base::TimeTicks TestScheduler::Now() const {
  return now_src_->Now();
}

TestScheduler::~TestScheduler() {
}

}  // namespace cc
