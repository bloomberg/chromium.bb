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
  // Create Mojo Message Pipe here.
  io_runner_->PostTask(FROM_HERE,
                       base::Bind(&MemlogReceiverPipeServer::StartOnIO, this));
  // TODO(ajwong): Add mojo service here.
}

void MemlogReceiverPipeServer::StartOnIO() {
  scoped_refptr<MemlogReceiverPipe> pipe(
      pipe_poller_.CreatePipe(std::move(first_pipe_)));
  on_new_connection_.Run(pipe);
  pipe->ReadUntilBlocking();
}

MemlogReceiverPipeServer::PipePoller::PipePoller() : controller_(FROM_HERE) {}

MemlogReceiverPipeServer::PipePoller::~PipePoller() {}

scoped_refptr<MemlogReceiverPipe>
MemlogReceiverPipeServer::PipePoller::CreatePipe(base::ScopedFD fd) {
  int raw_fd = fd.get();
  scoped_refptr<MemlogReceiverPipe> pipe(new MemlogReceiverPipe(std::move(fd)));
  pipes_[raw_fd] = pipe;
  base::MessageLoopForIO::current()->WatchFileDescriptor(
      raw_fd, true, base::MessageLoopForIO::WATCH_READ, &controller_, this);
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
