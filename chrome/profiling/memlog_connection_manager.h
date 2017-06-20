// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_
#define CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/profiling/backtrace_storage.h"
#include "chrome/profiling/memlog_receiver_pipe_server.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace profiling {

// Manages all connections and logging for each process. Pipes are supplied by
// the pipe server and this class will connect them to a parser and logger.
class MemlogConnectionManager {
 public:
  MemlogConnectionManager();
  ~MemlogConnectionManager();

  // Starts listening for connections.
  void StartConnections(const std::string& pipe_id);

 private:
  struct Connection;

  // Called by the pipe server when a new pipe is created.
  void OnNewConnection(scoped_refptr<MemlogReceiverPipe> new_pipe);

  // Notification that a connection is complete. Unlike OnNewConnection which
  // is signaled by the pipe server, this is signaled by the allocation tracker
  // to ensure that the pipeline for this process has been flushed of all
  // messages.
  void OnConnectionComplete(int process_id);

  void OnConnectionCompleteThunk(
      scoped_refptr<base::SingleThreadTaskRunner> main_loop,
      int process_id);

  scoped_refptr<MemlogReceiverPipeServer> server_;

  // Maps process ID to the connection information for it.
  base::flat_map<int, std::unique_ptr<Connection>> connections_;

  DISALLOW_COPY_AND_ASSIGN(MemlogConnectionManager);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_CONNECTION_MANAGER_H_
