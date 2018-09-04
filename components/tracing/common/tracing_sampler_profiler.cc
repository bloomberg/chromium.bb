// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/tracing_sampler_profiler.h"

#include <cinttypes>

#include "base/format_macros.h"
#include "base/no_destructor.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"

namespace tracing {

namespace {

// This class will receive the sampling profiler stackframes and output them
// to the chrome trace via an event.
class TracingProfileBuilder
    : public base::StackSamplingProfiler::ProfileBuilder {
 public:
  void RecordAnnotations() override {}

  void OnSampleCompleted(
      std::vector<base::StackSamplingProfiler::Frame> frames) override {
    if (frames.empty())
      return;
    // Insert an event with the frames rendered as a string with the following
    // format:
    //   offset - module [debugid]
    // The offset is difference between the load module address and the
    // frame address.
    //
    // Example:
    //
    //   "7ffb3d439164 - win32u.dll [B3E4BE89CA7FB42A2AC1E1C475284CA11]
    //    7ffb3f991b2d - USER32.dll [2103C0950C7DEC7F7AAA44348EDC1DDD1]
    //    7ffaf3e26201 - chrome.dll [8767EB7E1C77DD10014E8152A34786B812]
    //    7ffaf3e26008 - chrome.dll [8767EB7E1C77DD10014E8152A34786B812]
    //    7ffaf3e25afe - chrome.dll [8767EB7E1C77DD10014E8152A34786B812]
    //    7ffaf3e436e1 - chrome.dll [8767EB7E1C77DD10014E8152A34786B812]
    //    [...] "
    std::string result;
    for (const auto& frame : frames) {
      base::StringAppendF(
          &result, "0x%" PRIxPTR " - %s [%s]\n",
          frame.instruction_pointer - frame.module.base_address,
          frame.module.filename.BaseName().MaybeAsASCII().c_str(),
          frame.module.id.c_str());
    }

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                         "StackCpuSampling", TRACE_EVENT_SCOPE_THREAD, "frames",
                         result);
  }

  void OnProfileCompleted(base::TimeDelta profile_duration,
                          base::TimeDelta sampling_period) override {}
};

}  // namespace

TracingSamplerProfiler::TracingSamplerProfiler()
    : sampled_thread_id_(base::PlatformThread::CurrentId()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure tracing system notices profiler category.
  TRACE_EVENT_WARMUP_CATEGORY(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"));

  // If tracing is currently running, start the sample profiler.
  bool is_already_enabled =
      base::trace_event::TraceLog::GetInstance()->IsEnabled();
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
  if (is_already_enabled)
    OnTraceLogEnabled();
}

TracingSamplerProfiler::~TracingSamplerProfiler() {
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
}

void TracingSamplerProfiler::OnTraceLogEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(sampled_thread_id_, base::kInvalidThreadId);

  // Ensure there was not an instance of the profiler already running.
  if (profiler_.get())
    return;

  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                                     &enabled);
  if (!enabled)
    return;

  base::StackSamplingProfiler::SamplingParams params;
  params.samples_per_profile = std::numeric_limits<int>::max();
  params.sampling_interval = base::TimeDelta::FromMilliseconds(50);

  // Create and start the stack sampling profiler.
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      sampled_thread_id_, params, std::make_unique<TracingProfileBuilder>());
  profiler_->Start();
}

void TracingSamplerProfiler::OnTraceLogDisabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!profiler_.get())
    return;
  // Stop and release the stack sampling profiler.
  profiler_->Stop();
  profiler_.reset();
}

}  // namespace tracing
