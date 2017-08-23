// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_impl.h"

#include "base/trace_event/memory_dump_request_args.h"
#include "chrome/profiling/memlog_receiver_pipe.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace profiling {

MemlogImpl::MemlogImpl()
    : connection_manager_(new MemlogConnectionManager), weak_factory_(this) {}

MemlogImpl::~MemlogImpl() {}

void MemlogImpl::AddSender(base::ProcessId pid,
                           mojo::ScopedHandle sender_pipe,
                           AddSenderCallback callback) {
  base::PlatformFile platform_file;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::UnwrapPlatformFile(std::move(sender_pipe), &platform_file));

  connection_manager_->OnNewConnection(base::ScopedPlatformFile(platform_file),
                                       pid);

  std::move(callback).Run();
}

void MemlogImpl::DumpProcess(base::ProcessId pid,
                             mojo::ScopedHandle output_file,
                             std::unique_ptr<base::DictionaryValue> metadata) {
  base::PlatformFile platform_file;
  MojoResult result =
      UnwrapPlatformFile(std::move(output_file), &platform_file);
  if (result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed to unwrap output file " << result;
    return;
  }
  base::File file(platform_file);

  // Need a memory map to make sense of the dump. The dump will be triggered
  // in the memory map global dump callback.
  // TODO(brettw) this should be a OnceCallback to avoid base::Passed.
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->GetVmRegionsForHeapProfiler(base::Bind(
          &MemlogImpl::OnGetVmRegionsComplete, weak_factory_.GetWeakPtr(), pid,
          base::Passed(std::move(metadata)), base::Passed(std::move(file))));
}

void MemlogImpl::OnGetVmRegionsComplete(
    base::ProcessId pid,
    std::unique_ptr<base::DictionaryValue> metadata,
    base::File file,
    bool success,
    memory_instrumentation::mojom::GlobalMemoryDumpPtr dump) {
  if (!success) {
    LOG(ERROR) << "Global dump failed";
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
    LOG(ERROR) << "Don't have a memory dump for PID " << pid;
    return;
  }

  connection_manager_->DumpProcess(
      pid, std::move(metadata),
      std::move(process_dump->os_dump->memory_maps_for_heap_profiler),
      std::move(file));
}

}  // namespace profiling
