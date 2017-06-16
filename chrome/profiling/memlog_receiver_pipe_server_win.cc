// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_receiver_pipe_server_win.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/profiling/memlog_stream.h"

namespace profiling {

namespace {

// Use a large buffer for our pipe. We don't want the sender to block
// if at all possible since it will slow the app down quite a bit. Windows
// seemed to max out a 64K per read when testing larger sizes, so use that.
const int kBufferSize = 1024 * 64;

}  // namespace

MemlogReceiverPipeServer::MemlogReceiverPipeServer(
    base::TaskRunner* io_runner,
    const std::string& pipe_id,
    NewConnectionCallback on_new_conn)
    : io_runner_(io_runner),
      pipe_id_(base::ASCIIToUTF16(pipe_id)),
      on_new_connection_(on_new_conn) {}

MemlogReceiverPipeServer::~MemlogReceiverPipeServer() {
  // TODO(brettw) we should ensure this destructor is not called when there are
  // pending I/O operations as per RegisterIOHandler documentation.
}

void MemlogReceiverPipeServer::Start() {
  // TODO(brettw) this should perhaps not be async in case the subprocess
  // launches and tries to connect before the first pipe instance is created.
  io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MemlogReceiverPipeServer::ScheduleNewConnection, this, true));
}

base::string16 MemlogReceiverPipeServer::GetPipeName() const {
  base::string16 pipe_name(kWindowsPipePrefix);
  pipe_name.append(pipe_id_);
  return pipe_name;
}

HANDLE MemlogReceiverPipeServer::CreatePipeInstance(bool first_instance) const {
  base::string16 pipe_name = GetPipeName();

  DWORD open_mode = PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED;
  if (first_instance)
    open_mode |= FILE_FLAG_FIRST_PIPE_INSTANCE;

  return ::CreateNamedPipe(
      pipe_name.c_str(), open_mode, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
      PIPE_UNLIMITED_INSTANCES, kBufferSize, kBufferSize, 5000, NULL);
}

void MemlogReceiverPipeServer::ScheduleNewConnection(bool first_instance) {
  DCHECK(!current_);
  // Need Unretained to avoid creating a reference cycle.
  current_ = base::MakeUnique<MemlogReceiverPipe::CompletionThunk>(
      CreatePipeInstance(first_instance),
      base::BindRepeating(&MemlogReceiverPipeServer::OnIOCompleted,
                          base::Unretained(this)));
  ::ConnectNamedPipe(current_->handle(), current_->overlapped());
}

void MemlogReceiverPipeServer::OnIOCompleted(size_t bytes_transfered,
                                             DWORD error) {
  scoped_refptr<MemlogReceiverPipe> pipe(
      new MemlogReceiverPipe(std::move(current_)));
  ScheduleNewConnection(false);

  if (!on_new_connection_.is_null())
    on_new_connection_.Run(pipe);
  pipe->StartReadingOnIOThread();
}

}  // namespace profiling
