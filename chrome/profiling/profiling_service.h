// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_PROFILING_SERVICE_H_
#define CHROME_PROFILING_PROFILING_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/profiling/profiling_service.mojom.h"
#include "chrome/profiling/memlog_connection_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace profiling {

class MemlogImpl;

// Service implementation for Profiling. This will be called in the profiling
// process (which is a sandboxed utility process created on demand by the
// ServiceManager) to set manage the global state as well as the bound
// interface.
//
// This class lives in the I/O thread of the Utility process.
class ProfilingService : public service_manager::Service,
                         public mojom::ProfilingService,
                         public memory_instrumentation::mojom::HeapProfiler {
  using DumpProcessesForTracingCallback = memory_instrumentation::mojom::
      HeapProfiler::DumpProcessesForTracingCallback;

 public:
  ProfilingService();
  ~ProfilingService() override;

  // Factory method for creating the service. Used by the ServiceManager piping
  // to instantiate this thing.
  static std::unique_ptr<service_manager::Service> CreateService();

  // Lifescycle events that occur after the service has started to spinup.
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // ProfilingService implementation.
  void AddProfilingClient(base::ProcessId pid,
                          mojom::ProfilingClientPtr client,
                          mojo::ScopedHandle memlog_pipe_receiver,
                          mojom::ProcessType process_type,
                          mojom::ProfilingParamsPtr params) override;
  void SetKeepSmallAllocations(bool keep_small_allocations) override;
  void GetProfiledPids(GetProfiledPidsCallback callback) override;

  // HeapProfiler implementation.
  void DumpProcessesForTracing(
      bool strip_path_from_mapped_files,
      const DumpProcessesForTracingCallback& callback) override;

 private:
  void OnProfilingServiceRequest(
      mojom::ProfilingServiceRequest request);
  void OnHeapProfilerRequest(
      memory_instrumentation::mojom::HeapProfilerRequest request);

  void OnGetVmRegionsCompleteForDumpProcessesForTracing(
      bool strip_path_from_mapped_files,
      const DumpProcessesForTracingCallback& callback,
      VmRegions vm_regions);

  service_manager::BinderRegistry registry_;
  mojo::Binding<mojom::ProfilingService> binding_;

  mojo::Binding<memory_instrumentation::mojom::HeapProfiler>
      heap_profiler_binding_;

  memory_instrumentation::mojom::HeapProfilerHelperPtr helper_;
  MemlogConnectionManager connection_manager_;

  bool keep_small_allocations_ = false;

  // Must be last.
  base::WeakPtrFactory<ProfilingService> weak_factory_;
};

}  // namespace profiling

#endif  // CHROME_PROFILING_PROFILING_SERVICE_H_
