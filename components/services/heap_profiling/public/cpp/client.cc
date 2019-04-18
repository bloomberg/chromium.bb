// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/client.h"

#include "base/allocator/allocator_interception_mac.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/trace_event/malloc_dump_provider.h"
#include "build/build_config.h"
#include "components/services/heap_profiling/public/cpp/sampling_profiler_wrapper.h"

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
#include "base/trace_event/cfi_backtrace_android.h"
#endif

namespace heap_profiling {

Client::Client() : sampling_profiler_(new SamplingProfilerWrapper()) {}

Client::~Client() {
  if (!started_profiling_)
    return;
  sampling_profiler_->StopProfiling();
  base::trace_event::MallocDumpProvider::GetInstance()->EnableMetrics();
}

void Client::BindToInterface(mojom::ProfilingClientRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void Client::StartProfiling(mojom::ProfilingParamsPtr params) {
  if (started_profiling_)
    return;
  started_profiling_ = true;
  base::trace_event::MallocDumpProvider::GetInstance()->DisableMetrics();

#if defined(OS_MACOSX)
  // On macOS, this call is necessary to shim malloc zones that were created
  // after startup. This cannot be done during shim initialization because the
  // task scheduler has not yet been initialized.
  base::allocator::PeriodicallyShimNewMallocZones();
#endif

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
  // On Android the unwinder initialization requires file reading before
  // initializing shim. So, post task on background thread.
  base::PostTaskWithTraitsAndReply(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce([]() {
        bool can_unwind =
            base::trace_event::CFIBacktraceAndroid::GetInitializedInstance()
                ->can_unwind_stack_frames();
        DCHECK(can_unwind);
      }),
      base::BindOnce(&Client::StartProfilingInternal,
                     weak_factory_.GetWeakPtr(), std::move(params)));
#else
  StartProfilingInternal(std::move(params));
#endif
}

void Client::RetrieveHeapProfile(RetrieveHeapProfileCallback callback) {
  std::move(callback).Run(sampling_profiler_->RetrieveHeapProfile());
}

void Client::StartProfilingInternal(mojom::ProfilingParamsPtr params) {
  sampling_profiler_->StartProfiling(std::move(params));
}

}  // namespace heap_profiling
