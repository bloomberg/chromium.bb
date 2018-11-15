// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_TRACING_SAMPLER_PROFILER_H_
#define COMPONENTS_TRACING_COMMON_TRACING_SAMPLER_PROFILER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_log.h"
#include "components/tracing/tracing_export.h"

namespace base {
class StackSamplingProfiler;
}

namespace tracing {

// This class is a bridge between the base stack sampling profiler and chrome
// tracing. It's listening to TraceLog enabled/disabled events and it's starting
// a stack profiler on the current thread if needed.
class TRACING_EXPORT TracingSamplerProfiler
    : public base::trace_event::TraceLog::AsyncEnabledStateObserver {
 public:
  // Creates sampling profiler on main thread. Since the message loop might not
  // be setup when creating this profiler, the client must call
  // OnMessageLoopStarted() when setup.
  static std::unique_ptr<TracingSamplerProfiler> CreateOnMainThread();

  // Sets up tracing sampling profiler on a child thread. The profiler will be
  // stored in SequencedLocalStorageSlot and will be destroyed with the thread
  // task runner.
  static void CreateOnChildThread();

  ~TracingSamplerProfiler() override;

  // Notify the profiler that the message loop for current thread is started.
  // Only required for main thread.
  void OnMessageLoopStarted();

  // trace_event::TraceLog::EnabledStateObserver implementation:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

 private:
  explicit TracingSamplerProfiler(base::PlatformThreadId sampled_thread_id);

  const base::PlatformThreadId sampled_thread_id_;
  std::unique_ptr<base::StackSamplingProfiler> profiler_;

  base::WeakPtrFactory<TracingSamplerProfiler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TracingSamplerProfiler);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_TRACING_SAMPLER_PROFILER_H_
