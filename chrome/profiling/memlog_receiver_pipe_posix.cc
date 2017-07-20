// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_receiver_pipe_posix.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/threading/thread.h"
#include "chrome/profiling/memlog_stream_receiver.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle.h"

namespace profiling {

namespace {

// Use a large buffer for our pipe. We don't want the sender to block
// if at all possible since it will slow the app down quite a bit.
//
// TODO(ajwong): Figure out how to size this. Currently the number is cribbed
// from the right size for windows named pipes.
const int kReadBufferSize = 1024 * 64;

}  // namespace

MemlogReceiverPipe::MemlogReceiverPipe(base::ScopedFD fd)
    : handle_(mojo::edk::PlatformHandle(fd.release())),
      controller_(FROM_HERE),
      read_buffer_(new char[kReadBufferSize]) {}

MemlogReceiverPipe::~MemlogReceiverPipe() {}

void MemlogReceiverPipe::ReadUntilBlocking() {
  ssize_t bytes_read = 0;
  do {
    std::deque<mojo::edk::PlatformHandle> dummy_for_receive;
    bytes_read = mojo::edk::PlatformChannelRecvmsg(
        handle_.get(), read_buffer_.get(), kReadBufferSize, &dummy_for_receive);
    if (bytes_read > 0) {
      receiver_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&MemlogStreamReceiver::OnStreamData, receiver_,
                         std::move(read_buffer_), bytes_read));
      read_buffer_.reset(new char[kReadBufferSize]);
      return;
    } else if (bytes_read == 0) {
      // Other end closed the pipe.
      if (receiver_) {
        controller_.StopWatchingFileDescriptor();
        DCHECK(receiver_task_runner_);
        receiver_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&MemlogStreamReceiver::OnStreamComplete, receiver_));
      }
      return;
    } else {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        controller_.StopWatchingFileDescriptor();
        PLOG(ERROR) << "Problem reading socket.";
      }
    }
  } while (bytes_read > 0);
}

void MemlogReceiverPipe::SetReceiver(
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<MemlogStreamReceiver> receiver) {
  receiver_task_runner_ = task_runner;
  receiver_ = receiver;
}

}  // namespace profiling
