// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/tracing_sampler_profiler.h"

#include <cinttypes>

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if defined(OS_ANDROID)
#include "base/android/reached_code_profiler.h"
#endif

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
#include <dlfcn.h>

#include "base/trace_event/cfi_backtrace_android.h"
#include "components/tracing/common/stack_sampler_android.h"
#endif

namespace tracing {

namespace {

std::string GetFrameNameFromOffsetAddr(uintptr_t offset_from_module_base) {
  return base::StringPrintf("off:0x%" PRIxPTR, offset_from_module_base);
}

}  // namespace

TracingSamplerProfiler::TracingProfileBuilder::TracingProfileBuilder(
    base::PlatformThreadId sampled_thread_id)
    : sampled_thread_id_(sampled_thread_id) {}

base::ModuleCache*
TracingSamplerProfiler::TracingProfileBuilder::GetModuleCache() {
  return &module_cache_;
}

void TracingSamplerProfiler::TracingProfileBuilder::OnSampleCompleted(
    std::vector<base::Frame> frames) {
  int process_priority = base::Process::Current().GetPriority();
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                       "ProcessPriority", TRACE_EVENT_SCOPE_THREAD, "priority",
                       process_priority);

  if (frames.empty()) {
    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                         "StackCpuSampling", TRACE_EVENT_SCOPE_THREAD, "frames",
                         "empty", "thread_id", sampled_thread_id_);

    return;
  }
  // Insert an event with the frames rendered as a string with the following
  // formats:
  //   offset - module [debugid]
  //  [OR]
  //   symbol - module []
  // The offset is difference between the load module address and the
  // frame address.
  //
  // Example:
  //
  //   "malloc             - libc.so    []
  //    std::string::alloc - stdc++.so  []
  //    off:7ffb3f991b2d   - USER32.dll [2103C0950C7DEC7F7AAA44348EDC1DDD1]
  //    off:7ffb3d439164   - win32u.dll [B3E4BE89CA7FB42A2AC1E1C475284CA11]
  //    off:7ffaf3e26201   - chrome.dll [8767EB7E1C77DD10014E8152A34786B812]
  //    off:7ffaf3e26008   - chrome.dll [8767EB7E1C77DD10014E8152A34786B812]
  //    [...] "
  std::string result;
  for (const auto& frame : frames) {
    std::string frame_name;
    std::string module_name;
    std::string module_id;
#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
      Dl_info info = {};
      // For chrome address we do not have symbols on the binary. So, just write
      // the offset address. For addresses on framework libraries, symbolize
      // and write the function name.
      if (frame.instruction_pointer == 0) {
        frame_name = "Scanned";
      } else if (base::trace_event::CFIBacktraceAndroid::is_chrome_address(
                     frame.instruction_pointer)) {
        frame_name = GetFrameNameFromOffsetAddr(
            frame.instruction_pointer -
            base::trace_event::CFIBacktraceAndroid::executable_start_addr());
      } else if (dladdr(reinterpret_cast<void*>(frame.instruction_pointer),
                        &info) != 0) {
        // TODO(ssid): Add offset and module debug id if symbol was not resolved
        // in case this might be useful to send report to vendors.
        if (info.dli_sname)
          frame_name = info.dli_sname;
        if (info.dli_fname)
          module_name = info.dli_fname;
      }
      // If no module is available, then name it unknown. Adding PC would be
      // useless anyway.
      if (frame_name.empty())
        frame_name = "Unknown";
#else
    if (frame.module) {
      module_name = frame.module->GetDebugBasename().MaybeAsASCII();
      module_id = frame.module->GetId();
      frame_name = GetFrameNameFromOffsetAddr(frame.instruction_pointer -
                                              frame.module->GetBaseAddress());
    } else {
      module_name = module_id = "";
      frame_name = "Unknown";
    }
#endif
      base::StringAppendF(&result, "%s - %s [%s]\n", frame_name.c_str(),
                          module_name.c_str(), module_id.c_str());
  }

    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                         "StackCpuSampling", TRACE_EVENT_SCOPE_THREAD, "frames",
                         result, "thread_id", sampled_thread_id_);
}

// static
std::unique_ptr<TracingSamplerProfiler>
TracingSamplerProfiler::CreateOnMainThread() {
  return base::WrapUnique(
      new TracingSamplerProfiler(base::PlatformThread::CurrentId()));
}

// static
void TracingSamplerProfiler::CreateOnChildThread() {
  using ProfilerSlot =
      base::SequenceLocalStorageSlot<std::unique_ptr<TracingSamplerProfiler>>;
  static base::NoDestructor<ProfilerSlot> slot;
  if (!*slot) {
    slot->emplace(
        new TracingSamplerProfiler(base::PlatformThread::CurrentId()));
  }
}

TracingSamplerProfiler::TracingSamplerProfiler(
    base::PlatformThreadId sampled_thread_id)
    : sampled_thread_id_(sampled_thread_id) {
  DCHECK_NE(sampled_thread_id_, base::kInvalidThreadId);

  // If tracing was enabled before initializing this class, we missed the
  // OnTraceLogEnabled() event. Synthesize it so we can late-join the party.
  // If the observer is added after the calling |OnTraceLogEnabled|, there is
  // a race condition where tracing can be turned on between. By using this
  // ordering, the |OnTraceLogEnabled| will be called twice if tracing is turned
  // on between.
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
  OnTraceLogEnabled();
}

TracingSamplerProfiler::~TracingSamplerProfiler() {
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
}

void TracingSamplerProfiler::OnTraceLogEnabled() {
  base::AutoLock lock(lock_);
  // Ensure there was not an instance of the profiler already running.
  if (profiler_.get())
    return;

  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                                     &enabled);
  if (!enabled)
    return;

#if defined(OS_ANDROID)
  // The sampler profiler would conflict with the reached code profiler if they
  // run at the same time because they use the same signal to suspend threads.
  if (base::android::IsReachedCodeProfilerEnabled())
    return;
#endif

  base::StackSamplingProfiler::SamplingParams params;
  params.samples_per_profile = std::numeric_limits<int>::max();
  params.sampling_interval = base::TimeDelta::FromMilliseconds(50);
  // If the sampled thread is stopped for too long for sampling then it is ok to
  // get next sample at a later point of time. We do not want very accurate
  // metrics when looking at traces.
  params.keep_consistent_sampling_interval = false;

  // Create and start the stack sampling profiler.
#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      sampled_thread_id_, params,
      std::make_unique<TracingProfileBuilder>(sampled_thread_id_),
      std::make_unique<StackSamplerAndroid>(sampled_thread_id_));
#else
  profiler_ = std::make_unique<base::StackSamplingProfiler>(
      sampled_thread_id_, params,
      std::make_unique<TracingProfileBuilder>(sampled_thread_id_));
#endif
  profiler_->Start();
}

void TracingSamplerProfiler::OnTraceLogDisabled() {
  base::AutoLock lock(lock_);
  if (!profiler_.get())
    return;
  // Stop and release the stack sampling profiler.
  profiler_->Stop();
  profiler_.reset();
}

}  // namespace tracing
