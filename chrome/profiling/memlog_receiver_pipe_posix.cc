// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_receiver_pipe_posix.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "chrome/profiling/memlog_stream_receiver.h"

namespace profiling {

MemlogReceiverPipe::CompletionThunk::CompletionThunk(int fd, Callback cb)
    : controller_(FROM_HERE), fd_(fd), callback_(cb) {
  base::MessageLoopForIO::current()->WatchFileDescriptor(
      fd_, true, base::MessageLoopForIO::WATCH_READ, &controller_, this);
}

MemlogReceiverPipe::CompletionThunk::~CompletionThunk() {
  if (fd_ != -1)
    IGNORE_EINTR(::close(fd_));
}

void MemlogReceiverPipe::CompletionThunk::OnFileCanReadWithoutBlocking(int fd) {
  callback_.Run(fd);
}

void MemlogReceiverPipe::CompletionThunk::OnFileCanWriteWithoutBlocking(
    int fd) {
  NOTREACHED();
}

MemlogReceiverPipe::MemlogReceiverPipe(std::unique_ptr<CompletionThunk> thunk) {
}

MemlogReceiverPipe::~MemlogReceiverPipe() {}

void MemlogReceiverPipe::StartReadingOnIOThread() {
  // TODO(ajwong): Implement with something useful.
}

int MemlogReceiverPipe::GetRemoteProcessID() {
  // TODO(ajwong): Implement with something useful.
  return 0;
}

void MemlogReceiverPipe::SetReceiver(
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<MemlogStreamReceiver> receiver) {
  receiver_task_runner_ = task_runner;
  receiver_ = receiver;
}

}  // namespace profiling
