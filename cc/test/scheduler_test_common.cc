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

void FakeBeginFrameSource::DidFinishFrame(size_t remaining_frames) {
  remaining_frames_ = remaining_frames;
}
void FakeBeginFrameSource::AsValueInto(
    base::trace_event::TracedValue* dict) const {
  dict->SetString("type", "FakeBeginFrameSource");
  BeginFrameSourceMixIn::AsValueInto(dict);
}

TestBackToBackBeginFrameSource::TestBackToBackBeginFrameSource(
    scoped_refptr<TestNowSource> now_src,
    base::SingleThreadTaskRunner* task_runner)
    : BackToBackBeginFrameSource(task_runner), now_src_(now_src) {
}

TestBackToBackBeginFrameSource::~TestBackToBackBeginFrameSource() {
}

base::TimeTicks TestBackToBackBeginFrameSource::Now() {
  return now_src_->Now();
}

TestSyntheticBeginFrameSource::TestSyntheticBeginFrameSource(
    scoped_refptr<DelayBasedTimeSource> time_source)
    : SyntheticBeginFrameSource(time_source) {
}

TestSyntheticBeginFrameSource::~TestSyntheticBeginFrameSource() {
}

TestSchedulerFrameSourcesConstructor::TestSchedulerFrameSourcesConstructor(
    OrderedSimpleTaskRunner* test_task_runner,
    TestNowSource* now_src)
    : test_task_runner_(test_task_runner), now_src_(now_src) {
}
TestSchedulerFrameSourcesConstructor::~TestSchedulerFrameSourcesConstructor() {
}

BeginFrameSource*
TestSchedulerFrameSourcesConstructor::ConstructPrimaryFrameSource(
    Scheduler* scheduler) {
  if (scheduler->settings_.use_external_begin_frame_source) {
    return SchedulerFrameSourcesConstructor::ConstructPrimaryFrameSource(
        scheduler);
  } else {
    TRACE_EVENT1(
        "cc",
        "TestSchedulerFrameSourcesConstructor::ConstructPrimaryFrameSource",
        "source",
        "TestSyntheticBeginFrameSource");
    scoped_ptr<TestSyntheticBeginFrameSource> synthetic_source =
        TestSyntheticBeginFrameSource::Create(
            now_src_, test_task_runner_, BeginFrameArgs::DefaultInterval());

    DCHECK(!scheduler->vsync_observer_);
    scheduler->vsync_observer_ = synthetic_source.get();

    DCHECK(!scheduler->primary_frame_source_internal_);
    scheduler->primary_frame_source_internal_ = synthetic_source.Pass();
    return scheduler->primary_frame_source_internal_.get();
  }
}

BeginFrameSource*
TestSchedulerFrameSourcesConstructor::ConstructBackgroundFrameSource(
    Scheduler* scheduler) {
  TRACE_EVENT1(
      "cc",
      "TestSchedulerFrameSourcesConstructor::ConstructBackgroundFrameSource",
      "source",
      "TestSyntheticBeginFrameSource");
  DCHECK(!(scheduler->background_frame_source_internal_));
  scheduler->background_frame_source_internal_ =
      TestSyntheticBeginFrameSource::Create(
          now_src_, test_task_runner_, base::TimeDelta::FromSeconds(1));
  return scheduler->background_frame_source_internal_.get();
}

BeginFrameSource*
TestSchedulerFrameSourcesConstructor::ConstructUnthrottledFrameSource(
    Scheduler* scheduler) {
  TRACE_EVENT1(
      "cc",
      "TestSchedulerFrameSourcesConstructor::ConstructUnthrottledFrameSource",
      "source", "TestBackToBackBeginFrameSource");
  DCHECK(!scheduler->unthrottled_frame_source_internal_);
  scheduler->unthrottled_frame_source_internal_ =
      TestBackToBackBeginFrameSource::Create(now_src_, test_task_runner_);
  return scheduler->unthrottled_frame_source_internal_.get();
}

TestScheduler::TestScheduler(
    scoped_refptr<TestNowSource> now_src,
    SchedulerClient* client,
    const SchedulerSettings& scheduler_settings,
    int layer_tree_host_id,
    const scoped_refptr<OrderedSimpleTaskRunner>& test_task_runner,
    TestSchedulerFrameSourcesConstructor* frame_sources_constructor,
    scoped_ptr<BeginFrameSource> external_begin_frame_source)
    : Scheduler(client,
                scheduler_settings,
                layer_tree_host_id,
                test_task_runner,
                external_begin_frame_source.Pass(),
                frame_sources_constructor),
      now_src_(now_src) {
}

base::TimeTicks TestScheduler::Now() const {
  return now_src_->Now();
}

TestScheduler::~TestScheduler() {
}

}  // namespace cc
