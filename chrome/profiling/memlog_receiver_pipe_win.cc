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
#include "chrome/profiling/memlog_stream_receiver.h"

namespace profiling {

namespace {

// Use a large buffer for our pipe. We don't want the sender to block
// if at all possible since it will slow the app down quite a bit. Windows
// seems to max out a 64K per read so we use that (larger would be better if it
// worked).
const int kReadBufferSize = 1024 * 64;

}  // namespace

MemlogReceiverPipe::CompletionThunk::CompletionThunk(HANDLE handle, Callback cb)
    : handle_(handle), callback_(cb) {
  base::MessageLoopForIO::current()->RegisterIOHandler(handle, this);
  ZeroOverlapped();
}

MemlogReceiverPipe::CompletionThunk::~CompletionThunk() {
  if (handle_ != INVALID_HANDLE_VALUE)
    ::CloseHandle(handle_);
}

void MemlogReceiverPipe::CompletionThunk::ZeroOverlapped() {
  memset(overlapped(), 0, sizeof(OVERLAPPED));
}

void MemlogReceiverPipe::CompletionThunk::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD bytes_transfered,
    DWORD error) {
  // Note: any crashes with this on the stack are likely a result of destroying
  // a relevant class while there is I/O pending.
  callback_.Run(static_cast<size_t>(bytes_transfered), error);
}

MemlogReceiverPipe::MemlogReceiverPipe(std::unique_ptr<CompletionThunk> thunk)
    : thunk_(std::move(thunk)), read_buffer_(new char[kReadBufferSize]) {
  // Need Unretained to avoid a reference cycle.
  thunk_->set_callback(base::BindRepeating(&MemlogReceiverPipe::OnIOCompleted,
                                           base::Unretained(this)));
}

MemlogReceiverPipe::~MemlogReceiverPipe() {}

void MemlogReceiverPipe::StartReadingOnIOThread() {
  ReadUntilBlocking();
}

int MemlogReceiverPipe::GetRemoteProcessID() {
  ULONG id = 0;
  ::GetNamedPipeClientProcessId(thunk_->handle(), &id);
  return static_cast<int>(id);
}

void MemlogReceiverPipe::SetReceiver(
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<MemlogStreamReceiver> receiver) {
  receiver_task_runner_ = task_runner;
  receiver_ = receiver;
}

void MemlogReceiverPipe::OnIOCompleted(size_t bytes_transfered, DWORD error) {
  DCHECK(read_outstanding_);
  read_outstanding_ = false;

  // This will get called both for async completion of ConnectNamedPipe as well
  // as async read completions.
  if (bytes_transfered && receiver_) {
    receiver_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MemlogStreamReceiver::OnStreamData, receiver_,
                       std::move(read_buffer_), bytes_transfered));
    read_buffer_.reset(new char[kReadBufferSize]);
  }
  ReadUntilBlocking();
}

void MemlogReceiverPipe::ReadUntilBlocking() {
  // TODO(brettw) note that the IO completion callback will always be issued,
  // even for sync returns of ReadFile. If there is a lot of data ready to be
  // read, it would be nice to process them all in this loop rather than having
  // to go back to the message loop for each block, but that will require
  // different IOContext structures for each one.
  DWORD bytes_read = 0;
  thunk_->ZeroOverlapped();

  DCHECK(!read_outstanding_);
  read_outstanding_ = true;
  if (!::ReadFile(thunk_->handle(), read_buffer_.get(), kReadBufferSize,
                  &bytes_read, thunk_->overlapped())) {
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

}  // namespace profiling
