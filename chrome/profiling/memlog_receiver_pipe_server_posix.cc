// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_receiver_pipe_server_posix.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/profiling/memlog_stream.h"

namespace profiling {

MemlogReceiverPipeServer::MemlogReceiverPipeServer(
    base::TaskRunner* io_runner,
    const std::string& first_pipe_id,
    NewConnectionCallback on_new_conn)
    : io_runner_(io_runner), on_new_connection_(on_new_conn) {
  int fd;
  CHECK(base::StringToInt(first_pipe_id, &fd));
  first_pipe_.reset(fd);
}

MemlogReceiverPipeServer::~MemlogReceiverPipeServer() {}

void MemlogReceiverPipeServer::Start() {
  // TODO(ajwong): This uses 0 as the sender_pid.  Decide if we want to use
  // read PIDs, or just the browser concept of a child_process_id for an
  // identifier.
  io_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MemlogReceiverPipeServer::OnNewPipe, this,
                                std::move(first_pipe_), 0));
}

void MemlogReceiverPipeServer::OnNewPipe(base::ScopedFD pipe, int sender_pid) {
  DCHECK(io_runner_->RunsTasksInCurrentSequence());

  scoped_refptr<MemlogReceiverPipe> receiver_pipe(
      pipe_poller_.CreatePipe(std::move(pipe)));
  on_new_connection_.Run(receiver_pipe, sender_pid);
  receiver_pipe->ReadUntilBlocking();
}

MemlogReceiverPipeServer::PipePoller::PipePoller() {}

MemlogReceiverPipeServer::PipePoller::~PipePoller() {}

scoped_refptr<MemlogReceiverPipe>
MemlogReceiverPipeServer::PipePoller::CreatePipe(base::ScopedFD fd) {
  int raw_fd = fd.get();
  scoped_refptr<MemlogReceiverPipe> pipe(new MemlogReceiverPipe(std::move(fd)));
  pipes_[raw_fd] = pipe;
  base::MessageLoopForIO::current()->WatchFileDescriptor(
      raw_fd, true, base::MessageLoopForIO::WATCH_READ, pipe->controller(),
      this);
  return pipe;
}

void MemlogReceiverPipeServer::PipePoller::OnFileCanReadWithoutBlocking(
    int fd) {
  const auto& it = pipes_.find(fd);
  if (it != pipes_.end()) {
    it->second->ReadUntilBlocking();
  }
}

void MemlogReceiverPipeServer::PipePoller::OnFileCanWriteWithoutBlocking(
    int fd) {
  NOTIMPLEMENTED();
}

}  // namespace profiling
