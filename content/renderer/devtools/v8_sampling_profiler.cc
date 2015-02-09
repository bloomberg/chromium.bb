// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/v8_sampling_profiler.h"

#include "base/synchronization/cancellation_flag.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"

namespace content {

class V8SamplingThread : public base::PlatformThread::Delegate {
 public:
  explicit V8SamplingThread(base::WaitableEvent* event);

  // Implementation of PlatformThread::Delegate:
  void ThreadMain() override;

  void Start();
  void Stop();

 private:
  base::CancellationFlag cancellation_flag_;
  base::WaitableEvent* waitable_event_for_testing_;
  base::PlatformThreadHandle sampling_thread_handle_;

  DISALLOW_COPY_AND_ASSIGN(V8SamplingThread);
};

V8SamplingThread::V8SamplingThread(base::WaitableEvent* event)
    : waitable_event_for_testing_(event) {
}

void V8SamplingThread::ThreadMain() {
  base::PlatformThread::SetName("V8 Sampling Profiler Thread");
  const int kSamplingFrequencyMicroseconds = 1000;
  while (!cancellation_flag_.IsSet()) {
    if (waitable_event_for_testing_) {
      waitable_event_for_testing_->Signal();
    }
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(kSamplingFrequencyMicroseconds));
  }
}

void V8SamplingThread::Start() {
  if (!base::PlatformThread::Create(0, this, &sampling_thread_handle_)) {
    DCHECK(false) << "failed to create thread";
  }
}

void V8SamplingThread::Stop() {
  cancellation_flag_.Set();
  base::PlatformThread::Join(sampling_thread_handle_);
}

V8SamplingProfiler::V8SamplingProfiler() : sampling_thread_(nullptr) {
  // Force the "v8_cpu_profile" category to show up in the trace viewer.
  base::trace_event::TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("v8_cpu_profile"));
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
}

V8SamplingProfiler::~V8SamplingProfiler() {
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
  DCHECK(!sampling_thread_.get());
}

void V8SamplingProfiler::OnTraceLogEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8_cpu_profile"), &enabled);
  if (!enabled)
    return;
  DCHECK(!sampling_thread_.get());
  sampling_thread_.reset(
      new V8SamplingThread(waitable_event_for_testing_.get()));
  sampling_thread_->Start();
}

void V8SamplingProfiler::OnTraceLogDisabled() {
  if (!sampling_thread_.get())
    return;
  sampling_thread_->Stop();
  sampling_thread_.reset();
}

void V8SamplingProfiler::EnableSamplingEventForTesting() {
  waitable_event_for_testing_.reset(new base::WaitableEvent(false, false));
}

void V8SamplingProfiler::WaitSamplingEventForTesting() {
  waitable_event_for_testing_->Wait();
}

}  // namespace blink
