// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_service.h"

#include "base/logging.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace profiling {

ProfilingService::ProfilingService() : binding_(this), weak_factory_(this) {}

ProfilingService::~ProfilingService() {}

std::unique_ptr<service_manager::Service> ProfilingService::CreateService() {
  return base::MakeUnique<ProfilingService>();
}

void ProfilingService::OnStart() {
  registry_.AddInterface(base::Bind(
      &ProfilingService::OnProfilingServiceRequest, base::Unretained(this)));
}

void ProfilingService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void ProfilingService::OnProfilingServiceRequest(
    mojom::ProfilingServiceRequest request) {
  binding_.Bind(std::move(request));
}

void ProfilingService::AddProfilingClient(
    base::ProcessId pid,
    mojom::ProfilingClientPtr client,
    mojo::ScopedHandle memlog_pipe_receiver,
    mojom::ProcessType process_type,
    mojom::ProfilingParamsPtr params) {
  connection_manager_.OnNewConnection(pid, std::move(client),
                                      std::move(memlog_pipe_receiver),
                                      process_type, std::move(params));
}

void ProfilingService::DumpProcessesForTracing(
    bool keep_small_allocations,
    bool strip_path_from_mapped_files,
    DumpProcessesForTracingCallback callback) {
  if (!helper_) {
    context()->connector()->BindInterface(
        resource_coordinator::mojom::kServiceName, &helper_);
  }

  // Need a memory map to make sense of the dump. The dump will be triggered
  // in the memory map global dump callback.
  helper_->GetVmRegionsForHeapProfiler(base::Bind(
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
