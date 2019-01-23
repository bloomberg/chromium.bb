// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/client.h"

#include "base/allocator/allocator_interception_mac.h"
#include "base/command_line.h"
#include "base/files/platform_file.h"
#include "base/metrics/field_trial_params.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/trace_event/malloc_dump_provider.h"
#include "build/build_config.h"
#include "components/services/heap_profiling/public/cpp/sampling_profiler_wrapper.h"
#include "components/services/heap_profiling/public/cpp/sender_pipe.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/services/heap_profiling/public/cpp/stream.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/sandbox/switches.h"

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
#include "base/trace_event/cfi_backtrace_android.h"
#endif

namespace heap_profiling {

namespace {
const int kTimeoutDurationMs = 10000;

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
void EnsureCFIInitializedOnBackgroundThread(
    scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner,
    base::OnceClosure callback) {
  bool can_unwind =
      base::trace_event::CFIBacktraceAndroid::GetInitializedInstance()
          ->can_unwind_stack_frames();
  DCHECK(can_unwind);
  callback_task_runner->PostTask(FROM_HERE, std::move(callback));
}
#endif

}  // namespace

Client::Client()
    : started_profiling_(false),
      sampling_profiler_(new SamplingProfilerWrapper()),
      weak_factory_(this) {}

Client::~Client() {
  if (!started_profiling_)
    return;

  sampling_profiler_->StopProfiling();
  SamplingProfilerWrapper::FlushBuffersAndClosePipe();

  base::trace_event::MallocDumpProvider::GetInstance()->EnableMetrics();

  // The allocator shim cannot be synchronously, consistently stopped. We leak
  // the sender_pipe_, with the idea that very few future messages will
  // be sent to it. This happens at shutdown, so resources will be reclaimed
  // by the OS after the process is terminated.
  sender_pipe_.release();
}

void Client::BindToInterface(mojom::ProfilingClientRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void Client::StartProfiling(mojom::ProfilingParamsPtr params) {
  // Never allow profiling of the profiling process. That would cause deadlock.
  std::string sandbox_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          service_manager::switches::kServiceSandboxType);
  if (sandbox_type == service_manager::switches::kProfilingSandbox)
    return;

  if (started_profiling_)
    return;
  started_profiling_ = true;

  sender_pipe_.reset(new SenderPipe(
      mojo::UnwrapPlatformHandle(std::move(params->sender_pipe))));

  StreamHeader header;
  header.signature = kStreamSignature;
  SenderPipe::Result result =
      sender_pipe_->Send(&header, sizeof(header), kTimeoutDurationMs);
  if (result != SenderPipe::Result::kSuccess) {
    sender_pipe_->Close();
    return;
  }

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
  auto init_callback =
      base::BindOnce(&Client::StartProfilingInternal,
                     weak_factory_.GetWeakPtr(), std::move(params));

  auto background_task = base::BindOnce(&EnsureCFIInitializedOnBackgroundThread,
                                        base::ThreadTaskRunnerHandle::Get(),
                                        std::move(init_callback));
  base::PostTaskWithTraits(FROM_HERE,
                           {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
                            base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                           std::move(background_task));
#else
  StartProfilingInternal(std::move(params));
#endif
}

void Client::FlushMemlogPipe(uint32_t barrier_id) {
  SamplingProfilerWrapper::FlushPipe(barrier_id);
}

void Client::StartProfilingInternal(mojom::ProfilingParamsPtr params) {
  sampling_profiler_->StartProfiling(sender_pipe_.get(), std::move(params));
}

}  // namespace heap_profiling
