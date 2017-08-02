// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_
#define CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/platform_file.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "chrome/profiling/backtrace_storage.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

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
  MemlogConnectionManager(scoped_refptr<base::SequencedTaskRunner> io_runner,
                          BacktraceStorage* backtrace_storage);
  ~MemlogConnectionManager();

  // Dumps the memory log for the given process into |output_file|.
  void DumpProcess(int32_t sender_id, base::File output_file);

  void OnNewConnection(base::ScopedPlatformFile file, int sender_id);

 private:
  struct Connection;

  // Notification that a connection is complete. Unlike OnNewConnection which
  // is signaled by the pipe server, this is signaled by the allocation tracker
  // to ensure that the pipeline for this process has been flushed of all
  // messages.
  void OnConnectionComplete(int sender_id);

  void OnConnectionCompleteThunk(
      scoped_refptr<base::SingleThreadTaskRunner> main_loop,
      int process_id);

  scoped_refptr<base::SequencedTaskRunner> io_runner_;
  BacktraceStorage* backtrace_storage_;  // Not owned.

  // Maps process ID to the connection information for it.
  base::flat_map<int, std::unique_ptr<Connection>> connections_;
  base::Lock connections_lock_;

  DISALLOW_COPY_AND_ASSIGN(MemlogConnectionManager);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_
