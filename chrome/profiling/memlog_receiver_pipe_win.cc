// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_receiver_pipe_win.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "chrome/profiling/memlog_receiver_pipe.h"
#include "chrome/profiling/memlog_stream_receiver.h"

namespace profiling {

namespace {

// Use a large buffer for our pipe. We don't want the sender to block
// if at all possible since it will slow the app down quite a bit. Windows
// seems to max out a 64K per read so we use that (larger would be better if it
// worked).
const int kReadBufferSize = 1024 * 64;

}  // namespace

MemlogReceiverPipe::MemlogReceiverPipe(base::ScopedPlatformFile handle)
    : handle_(std::move(handle)), read_buffer_(new char[kReadBufferSize]) {
  ZeroOverlapped();
  base::MessageLoopForIO::current()->RegisterIOHandler(handle_.Get(), this);
}

MemlogReceiverPipe::~MemlogReceiverPipe() {
}

void MemlogReceiverPipe::StartReadingOnIOThread() {
  ReadUntilBlocking();
}

void MemlogReceiverPipe::SetReceiver(
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<MemlogStreamReceiver> receiver) {
  receiver_task_runner_ = task_runner;
  receiver_ = receiver;
}

void MemlogReceiverPipe::ReportError() {
  handle_.Close();
}

void MemlogReceiverPipe::ReadUntilBlocking() {
  // TODO(brettw) note that the IO completion callback will always be issued,
  // even for sync returns of ReadFile. If there is a lot of data ready to be
  // read, it would be nice to process them all in this loop rather than having
  // to go back to the message loop for each block, but that will require
  // different IOContext structures for each one.
  DWORD bytes_read = 0;
  ZeroOverlapped();

  DCHECK(!read_outstanding_);
  read_outstanding_ = true;
  if (!::ReadFile(handle_.Get(), read_buffer_.get(), kReadBufferSize,
                  &bytes_read, &context_.overlapped)) {
    if (GetLastError() == ERROR_IO_PENDING) {
      return;
    } else {
      if (receiver_) {
        receiver_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&MemlogStreamReceiver::OnStreamComplete, receiver_));
      }
      return;
    }
  }
}

void MemlogReceiverPipe::ZeroOverlapped() {
  memset(&context_.overlapped, 0, sizeof(OVERLAPPED));
}

void MemlogReceiverPipe::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD bytes_transfered,
    DWORD error) {
  // Note: any crashes with this on the stack are likely a result of destroying
  // a relevant class while there is I/O pending.
  DCHECK(read_outstanding_);
  read_outstanding_ = false;

  if (bytes_transfered && receiver_) {
    receiver_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ReceiverPipeStreamDataThunk,
                                  base::MessageLoop::current()->task_runner(),
                                  scoped_refptr<MemlogReceiverPipe>(this),
                                  receiver_, std::move(read_buffer_),
                                  static_cast<size_t>(bytes_transfered)));
    read_buffer_.reset(new char[kReadBufferSize]);
  }
  ReadUntilBlocking();
}

}  // namespace profiling
