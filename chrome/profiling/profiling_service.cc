// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_service.h"

#include "base/logging.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace profiling {

ProfilingService::ProfilingService() : weak_factory_(this) {}

ProfilingService::~ProfilingService() {}

std::unique_ptr<service_manager::Service> ProfilingService::CreateService() {
  return base::MakeUnique<ProfilingService>();
}

void ProfilingService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(base::Bind(
      &ProfilingService::MaybeRequestQuitDelayed, base::Unretained(this))));
  registry_.AddInterface(
      base::Bind(&ProfilingService::OnProfilingServiceRequest,
                 base::Unretained(this), ref_factory_.get()));
}

void ProfilingService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));

  // TODO(ajwong): Maybe signal shutdown when all interfaces are closed?  What
  // does ServiceManager actually do?
}

void ProfilingService::MaybeRequestQuitDelayed() {
  // TODO(ajwong): What does this and the MaybeRequestQuit() function actually
  // do? This is just cargo-culted from another mojo service.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ProfilingService::MaybeRequestQuit,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(5));
}

void ProfilingService::MaybeRequestQuit() {
  DCHECK(ref_factory_);
  if (ref_factory_->HasNoRefs())
    context()->RequestQuit();
}

void ProfilingService::OnProfilingServiceRequest(
    service_manager::ServiceContextRefFactory* ref_factory,
    mojom::ProfilingServiceRequest request) {
  binding_set_.AddBinding(this, std::move(request));
}

void ProfilingService::AddProfilingClient(
    base::ProcessId pid,
    mojom::ProfilingClientPtr client,
    mojo::ScopedHandle memlog_pipe_sender,
    mojo::ScopedHandle memlog_pipe_receiver,
    mojom::ProcessType process_type,
    profiling::mojom::StackMode stack_mode) {
  connection_manager_.OnNewConnection(
      pid, std::move(client), std::move(memlog_pipe_sender),
      std::move(memlog_pipe_receiver), process_type, stack_mode);
}

void ProfilingService::DumpProcessesForTracing(
    bool keep_small_allocations,
    bool strip_path_from_mapped_files,
    DumpProcessesForTracingCallback callback) {
  // Need a memory map to make sense of the dump. The dump will be triggered
  // in the memory map global dump callback.
  // TODO(brettw) this should be a OnceCallback to avoid base::Passed.
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->GetVmRegionsForHeapProfiler(base::Bind(
          &ProfilingService::OnGetVmRegionsCompleteForDumpProcessesForTracing,
          weak_factory_.GetWeakPtr(), keep_small_allocations,
          strip_path_from_mapped_files, base::Passed(&callback)));
}

void ProfilingService::GetProfiledPids(GetProfiledPidsCallback callback) {
  std::move(callback).Run(connection_manager_.GetConnectionPids());
}

void ProfilingService::OnGetVmRegionsCompleteForDumpProcessesForTracing(
    bool keep_small_allocations,
    bool strip_path_from_mapped_files,
    mojom::ProfilingService::DumpProcessesForTracingCallback callback,
    bool success,
    memory_instrumentation::mojom::GlobalMemoryDumpPtr dump) {
  if (!success) {
    DLOG(ERROR) << "GetVMRegions failed";
    std::move(callback).Run(
        std::vector<profiling::mojom::SharedBufferWithSizePtr>());
    return;
  }
  // TODO(bug 752621) we should be asking and getting the memory map of only
  // the process we want rather than querying all processes and filtering.
  connection_manager_.DumpProcessesForTracing(
      keep_small_allocations, strip_path_from_mapped_files, std::move(callback),
      std::move(dump));
}

}  // namespace profiling
