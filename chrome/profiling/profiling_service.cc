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
    mojo::ScopedHandle memlog_pipe_receiver) {
  connection_manager_.OnNewConnection(pid, std::move(client),
                                      std::move(memlog_pipe_sender),
                                      std::move(memlog_pipe_receiver));
}

void ProfilingService::DumpProcess(
    base::ProcessId pid,
    mojo::ScopedHandle output_file,
    std::unique_ptr<base::DictionaryValue> metadata,
    DumpProcessCallback callback) {
  base::PlatformFile platform_file;
  MojoResult result =
      UnwrapPlatformFile(std::move(output_file), &platform_file);
  if (result != MOJO_RESULT_OK) {
    DLOG(ERROR) << "Failed to unwrap output file " << result;
    std::move(callback).Run(false);
    return;
  }
  base::File file(platform_file);

  // Need a memory map to make sense of the dump. The dump will be triggered
  // in the memory map global dump callback.
  // TODO(brettw) this should be a OnceCallback to avoid base::Passed.
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->GetVmRegionsForHeapProfiler(
          base::Bind(&ProfilingService::OnGetVmRegionsCompleteForDumpProcess,
                     weak_factory_.GetWeakPtr(), pid, base::Passed(&metadata),
                     base::Passed(&file), base::Passed(&callback)));
}

void ProfilingService::DumpProcessesForTracing(
    DumpProcessesForTracingCallback callback) {
  // Need a memory map to make sense of the dump. The dump will be triggered
  // in the memory map global dump callback.
  // TODO(brettw) this should be a OnceCallback to avoid base::Passed.
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->GetVmRegionsForHeapProfiler(base::Bind(
          &ProfilingService::OnGetVmRegionsCompleteForDumpProcessesForTracing,
          weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void ProfilingService::OnGetVmRegionsCompleteForDumpProcess(
    base::ProcessId pid,
    std::unique_ptr<base::DictionaryValue> metadata,
    base::File file,
    DumpProcessCallback callback,
    bool success,
    memory_instrumentation::mojom::GlobalMemoryDumpPtr dump) {
  if (!success) {
    DLOG(ERROR) << "Global dump failed";
    std::move(callback).Run(false);
    return;
  }

  // Find the process's memory dump we want.
  // TODO(bug 752621) we should be asking and getting the memory map of only
  // the process we want rather than querying all processes and filtering.
  memory_instrumentation::mojom::ProcessMemoryDump* process_dump = nullptr;
  for (const auto& proc : dump->process_dumps) {
    if (proc->pid == pid) {
      process_dump = &*proc;
      break;
    }
  }
  if (!process_dump) {
    DLOG(ERROR) << "Don't have a memory dump for PID " << pid;
    std::move(callback).Run(false);
    return;
  }

  MemlogConnectionManager::DumpProcessArgs args;
  args.pid = pid;
  args.metadata = std::move(metadata);
  args.maps = std::move(process_dump->os_dump->memory_maps_for_heap_profiler);
  args.file = std::move(file);
  args.callback = std::move(callback);
  connection_manager_.DumpProcess(std::move(args));
}

void ProfilingService::OnGetVmRegionsCompleteForDumpProcessesForTracing(
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
  connection_manager_.DumpProcessesForTracing(std::move(callback),
                                              std::move(dump));
}

}  // namespace profiling
