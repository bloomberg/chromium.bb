// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_SERVER_POSIX_H_
#define CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_SERVER_POSIX_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_libevent.h"
#include "chrome/profiling/memlog_receiver_pipe_posix.h"

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
      base::RepeatingCallback<void(scoped_refptr<MemlogReceiverPipe>, int)>;

  // |io_runner| is the task runner for the I/O thread. When a new connection is
  // established, the |on_new_conn| callback is called with the pipe.
  MemlogReceiverPipeServer(base::TaskRunner* io_runner,
                           const std::string& pipe_id,
                           NewConnectionCallback on_new_conn);

  void set_on_new_connection(NewConnectionCallback on_new_connection) {
    on_new_connection_ = on_new_connection;
  }

  // Starts the server which opens the pipe and begins accepting connections.
  void Start();

  // Runs on IO Thread.
  // TODO(ajwong): Make private once the correct separation of responsibilities
  // is worked out between MemlogReceiverPipeServer and ProfilingProcess.
  void OnNewPipe(base::ScopedPlatformFile pipe, int sender_pid);

 private:
  friend class base::RefCountedThreadSafe<MemlogReceiverPipeServer>;
  ~MemlogReceiverPipeServer();

  // TODO(ajwong): Remove this class.  Initially it was created under the
  // mistaken understanding that one FileDescriptorWatcher could watch
  // multiple file descriptors. That's incorrect meaning each
  // MemlogReceiverPipe can own its own watcher and polling logic which
  // makes this class useless.
  class PipePoller : public base::MessageLoopForIO::Watcher {
   public:
    PipePoller();
    ~PipePoller() override;

    scoped_refptr<MemlogReceiverPipe> CreatePipe(base::ScopedFD fd);

    void OnFileCanReadWithoutBlocking(int fd) override;
    void OnFileCanWriteWithoutBlocking(int fd) override;

   private:
    base::flat_map<int, scoped_refptr<MemlogReceiverPipe>> pipes_;
  };

  scoped_refptr<base::TaskRunner> io_runner_;
  base::ScopedFD first_pipe_;
  NewConnectionCallback on_new_connection_;
  PipePoller pipe_poller_;

  DISALLOW_COPY_AND_ASSIGN(MemlogReceiverPipeServer);
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_RECEIVER_PIPE_SERVER_POSIX_H_
