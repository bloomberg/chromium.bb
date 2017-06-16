// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_SERVER_WIN_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_SERVER_WIN_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_win.h"
#include "base/strings/string16.h"
#include "chrome/profiling/memlog_receiver_pipe_win.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace profiling {

class MemlogReceiverPipe;

// This class listens for new pipe connections and creates new
// MemlogReceiverPipe objects for each one.
class MemlogReceiverPipeServer
    : public base::RefCountedThreadSafe<MemlogReceiverPipeServer> {
 public:
  using NewConnectionCallback =
      base::RepeatingCallback<void(scoped_refptr<MemlogReceiverPipe>)>;

  // |io_runner| is the task runner for the I/O thread. When a new connection is
  // established, the |on_new_conn| callback is called with the pipe.
  MemlogReceiverPipeServer(base::TaskRunner* io_runner,
                           const std::string& pipe_id,
                           NewConnectionCallback on_new_conn);
  ~MemlogReceiverPipeServer();

  void set_on_new_connection(NewConnectionCallback on_new_connection) {
    on_new_connection_ = on_new_connection;
  }

  // Starts the server which opens the pipe and begins accepting connections.
  void Start();

 private:
  base::string16 GetPipeName() const;

  HANDLE CreatePipeInstance(bool first_instance) const;

  // Called on the IO thread.
  void ScheduleNewConnection(bool first_instance);

  void OnIOCompleted(size_t bytes_transfered, DWORD error);

  scoped_refptr<base::TaskRunner> io_runner_;
  base::string16 pipe_id_;
  NewConnectionCallback on_new_connection_;

  // Current connection we're waiting on creation for.
  std::unique_ptr<MemlogReceiverPipe::CompletionThunk> current_;
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_SERVER_WIN_H_
