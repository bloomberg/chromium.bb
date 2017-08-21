// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_impl.h"

#include "base/trace_event/memory_dump_request_args.h"
#include "chrome/profiling/memlog_receiver_pipe.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace profiling {

MemlogImpl::MemlogImpl()
    : io_runner_(content::ChildThread::Get()->GetIOTaskRunner()),
      connection_manager_(new MemlogConnectionManager(io_runner_),
                          DeleteOnRunner(FROM_HERE, io_runner_.get())),
      weak_factory_(this) {}

MemlogImpl::~MemlogImpl() {}

void MemlogImpl::AddSender(base::ProcessId pid,
                           mojo::ScopedHandle sender_pipe,
                           AddSenderCallback callback) {
  base::PlatformFile platform_file;
  CHECK_EQ(MOJO_RESULT_OK,
           mojo::UnwrapPlatformFile(std::move(sender_pipe), &platform_file));

  // MemlogConnectionManager is deleted on the IOThread thus using
  // base::Unretained() is safe here.
  io_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MemlogConnectionManager::OnNewConnection,
                                base::Unretained(connection_manager_.get()),
                                base::ScopedPlatformFile(platform_file), pid));

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

  io_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MemlogConnectionManager::DumpProcess,
          base::Unretained(connection_manager_.get()), pid, std::move(metadata),
          std::move(process_dump->os_dump->memory_maps_for_heap_profiler),
          std::move(file)));
}

}  // namespace profiling
