// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_
#define CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/platform_file.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/profiling/backtrace_storage.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace base {

class SequencedTaskRunner;
class SingleThreadTaskRunner;

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
 public:
  explicit MemlogConnectionManager(
      scoped_refptr<base::SequencedTaskRunner> io_runner);
  ~MemlogConnectionManager();

  // Dumps the memory log for the given process into |output_file|.  This must
  // be provided the memory map for the given process since that is not tracked
  // as part of the normal allocation process.
  void DumpProcess(
      base::ProcessId pid,
      std::unique_ptr<base::DictionaryValue> metadata,
      const std::vector<memory_instrumentation::mojom::VmRegionPtr>& maps,
      base::File output_file);

  void OnNewConnection(base::ScopedPlatformFile file, base::ProcessId pid);

 private:
  struct Connection;

  // Notification that a connection is complete. Unlike OnNewConnection which
  // is signaled by the pipe server, this is signaled by the allocation tracker
  // to ensure that the pipeline for this process has been flushed of all
  // messages.
  void OnConnectionComplete(base::ProcessId pid);

  void OnConnectionCompleteThunk(
      scoped_refptr<base::SingleThreadTaskRunner> main_loop,
      base::ProcessId process_id);

  scoped_refptr<base::SequencedTaskRunner> io_runner_;
  BacktraceStorage backtrace_storage_;

  // Maps process ID to the connection information for it.
  base::flat_map<base::ProcessId, std::unique_ptr<Connection>> connections_;
  base::Lock connections_lock_;

  DISALLOW_COPY_AND_ASSIGN(MemlogConnectionManager);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_
