// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_
#define CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/profiling/profiling_service.mojom.h"
#include "chrome/profiling/allocation_event.h"
#include "chrome/profiling/allocation_tracker.h"
#include "chrome/profiling/backtrace_storage.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace profiling {

// Manages all connections and logging for each process. Pipes are supplied by
// the pipe server and this class will connect them to a parser and logger.
//
// Note |backtrace_storage| must outlive MemlogConnectionManager.
//
// This object is constructed on the UI thread, but the rest of the usage
// (including deletion) is on the IO thread.
class MemlogConnectionManager {
 private:
 public:
  MemlogConnectionManager();
  ~MemlogConnectionManager();

  // Shared types for the dump-type-specific args structures.
  struct DumpArgs {
    DumpArgs();
    DumpArgs(DumpArgs&&) noexcept;
    ~DumpArgs();

   private:
    friend MemlogConnectionManager;

    // This lock keeps the backtrace atoms alive throughout the dumping
    // process. It will be initialized by DumpProcess.
    BacktraceStorage::Lock backtrace_storage_lock;

    DISALLOW_COPY_AND_ASSIGN(DumpArgs);
  };

  // Parameters to DumpProcess().
  struct DumpProcessArgs : public DumpArgs {
    DumpProcessArgs();
    DumpProcessArgs(DumpProcessArgs&&) noexcept;
    ~DumpProcessArgs();

    // Process ID to dump.
    base::ProcessId pid;

    // The memory map for the given process for the dumped process must be
    // provided here since that is not tracked as part of the normal allocation
    // process.
    std::vector<memory_instrumentation::mojom::VmRegionPtr> maps;

    std::unique_ptr<base::DictionaryValue> metadata;

    // File to dump the output to.
    base::File file;
    mojom::ProfilingService::DumpProcessCallback callback;
  };

  // Dumping is asynchronous so will not be complete when this function
  // returns. The dump is complete when the callback provided in the args is
  // fired.
  void DumpProcess(DumpProcessArgs args);
  void DumpProcessesForTracing(
      mojom::ProfilingService::DumpProcessesForTracingCallback callback,
      memory_instrumentation::mojom::GlobalMemoryDumpPtr dump);

  void OnNewConnection(base::ProcessId pid,
                       mojom::ProfilingClientPtr client,
                       mojo::ScopedHandle sender_pipe_end,
                       mojo::ScopedHandle receiver_pipe_end);

 private:
  struct Connection;
  struct DumpProcessesForTracingTracking;

  // Schedules the given callback to execute after the given process ID has
  // been synchronized. If the process ID isn't found, the callback will be
  // asynchronously run with "false" as the success parameter.
  void SynchronizeOnPid(base::ProcessId process_id,
                        AllocationTracker::SnapshotCallback callback);

  // Actually does the dump assuming the given process has been synchronized.
  void DoDumpProcess(DumpProcessArgs args,
                     bool success,
                     AllocationCountMap counts,
                     AllocationTracker::ContextMap context);
  void DoDumpOneProcessForTracing(
      scoped_refptr<DumpProcessesForTracingTracking> tracking,
      base::ProcessId pid,
      bool success,
      AllocationCountMap counts,
      AllocationTracker::ContextMap context);

  // Notification that a connection is complete. Unlike OnNewConnection which
  // is signaled by the pipe server, this is signaled by the allocation tracker
  // to ensure that the pipeline for this process has been flushed of all
  // messages.
  void OnConnectionComplete(base::ProcessId pid);

  // These thunks post the request back to the given thread.
  static void OnConnectionCompleteThunk(
      scoped_refptr<base::SequencedTaskRunner> main_loop,
      base::WeakPtr<MemlogConnectionManager> connection_manager,
      base::ProcessId process_id);

  BacktraceStorage backtrace_storage_;

  // Next ID to use for a barrier request. This is incremented for each use
  // to ensure barrier IDs are unique.
  uint32_t next_barrier_id_ = 1;

  // Maps process ID to the connection information for it.
  base::flat_map<base::ProcessId, std::unique_ptr<Connection>> connections_;
  base::Lock connections_lock_;

  // Must be last.
  base::WeakPtrFactory<MemlogConnectionManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MemlogConnectionManager);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_
