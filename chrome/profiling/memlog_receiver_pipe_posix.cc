// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_receiver_pipe_posix.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/common/profiling/memlog_sender_pipe.h"
#include "chrome/profiling/memlog_receiver_pipe.h"
#include "chrome/profiling/memlog_stream_receiver.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle.h"

namespace profiling {

MemlogReceiverPipe::MemlogReceiverPipe(mojo::edk::ScopedPlatformHandle handle)
    : MemlogReceiverPipeBase(std::move(handle)),
      controller_(FROM_HERE),
      read_buffer_(new char[MemlogSenderPipe::kPipeSize]) {}

MemlogReceiverPipe::~MemlogReceiverPipe() {}

void MemlogReceiverPipe::StartReadingOnIOThread() {
  base::MessageLoopForIO::current()->WatchFileDescriptor(
      handle_.get().handle, true, base::MessageLoopForIO::WATCH_READ,
      &controller_, this);
  OnFileCanReadWithoutBlocking(handle_.get().handle);
}

void MemlogReceiverPipe::OnFileCanReadWithoutBlocking(int fd) {
  ssize_t bytes_read = 0;
  do {
    base::circular_deque<mojo::edk::PlatformHandle> dummy_for_receive;

    bytes_read = HANDLE_EINTR(read(handle_.get().handle, read_buffer_.get(),
                                   MemlogSenderPipe::kPipeSize));
    if (bytes_read > 0) {
      receiver_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&MemlogReceiverPipe::OnStreamDataThunk, this,
                         base::MessageLoop::current()->task_runner(),
                         std::move(read_buffer_),
                         static_cast<size_t>(bytes_read)));
      read_buffer_.reset(new char[MemlogSenderPipe::kPipeSize]);
      return;
    } else if (bytes_read == 0) {
      // Other end closed the pipe.
      controller_.StopWatchingFileDescriptor();
      DCHECK(receiver_task_runner_);
      receiver_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&MemlogStreamReceiver::OnStreamComplete, receiver_));
      return;
    } else {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        controller_.StopWatchingFileDescriptor();
        PLOG(ERROR) << "Problem reading socket.";
        DCHECK(receiver_task_runner_);
        receiver_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&MemlogStreamReceiver::OnStreamComplete, receiver_));
      }
    }
  } while (bytes_read > 0);
}

void MemlogReceiverPipe::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace profiling
