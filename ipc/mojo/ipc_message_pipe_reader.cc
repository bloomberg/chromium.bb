// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_message_pipe_reader.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "mojo/public/cpp/environment/environment.h"

namespace IPC {
namespace internal {

MessagePipeReader::MessagePipeReader(mojo::ScopedMessagePipeHandle handle)
    : pipe_wait_id_(0),
      pipe_(handle.Pass()) {
  StartWaiting();
}

MessagePipeReader::~MessagePipeReader() {
  CHECK(!IsValid());
}

void MessagePipeReader::Close() {
  StopWaiting();
  pipe_.reset();
  OnPipeClosed();
}

void MessagePipeReader::CloseWithError(MojoResult error) {
  OnPipeError(error);
  Close();
}

// static
void MessagePipeReader::InvokePipeIsReady(void* closure, MojoResult result) {
  reinterpret_cast<MessagePipeReader*>(closure)->PipeIsReady(result);
}

void MessagePipeReader::StartWaiting() {
  DCHECK(pipe_.is_valid());
  DCHECK(!pipe_wait_id_);
  // Not using MOJO_HANDLE_SIGNAL_WRITABLE here, expecting buffer in
  // MessagePipe.
  //
  // TODO(morrita): Should we re-set the signal when we get new
  // message to send?
  pipe_wait_id_ = mojo::Environment::GetDefaultAsyncWaiter()->AsyncWait(
      pipe_.get().value(),
      MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_DEADLINE_INDEFINITE,
      &InvokePipeIsReady,
      this);
}

void MessagePipeReader::StopWaiting() {
  if (!pipe_wait_id_)
    return;
  mojo::Environment::GetDefaultAsyncWaiter()->CancelWait(pipe_wait_id_);
  pipe_wait_id_ = 0;
}

void MessagePipeReader::PipeIsReady(MojoResult wait_result) {
  pipe_wait_id_ = 0;

  if (wait_result != MOJO_RESULT_OK) {
    // FAILED_PRECONDITION happens when the pipe is
    // closed before the waiter is scheduled in a backend thread.
    if (wait_result != MOJO_RESULT_ABORTED &&
        wait_result != MOJO_RESULT_FAILED_PRECONDITION) {
      DLOG(WARNING) << "Pipe got error from the waiter. Closing: "
                    << wait_result;
      OnPipeError(wait_result);
    }

    Close();
    return;
  }

  while (pipe_.is_valid()) {
    MojoResult read_result = ReadMessageBytes();
    if (read_result == MOJO_RESULT_SHOULD_WAIT)
      break;
    if (read_result != MOJO_RESULT_OK) {
      // FAILED_PRECONDITION means that all the received messages
      // got consumed and the peer is already closed.
      if (read_result != MOJO_RESULT_FAILED_PRECONDITION) {
        DLOG(WARNING)
            << "Pipe got error from ReadMessage(). Closing: " << read_result;
        OnPipeError(read_result);
      }

      Close();
      break;
    }

    OnMessageReceived();
  }

  if (pipe_.is_valid())
    StartWaiting();
}

MojoResult MessagePipeReader::ReadMessageBytes() {
  DCHECK(handle_buffer_.empty());

  uint32_t num_bytes = static_cast<uint32_t>(data_buffer_.size());
  uint32_t num_handles = 0;
  MojoResult result = MojoReadMessage(pipe_.get().value(),
                                      num_bytes ? &data_buffer_[0] : NULL,
                                      &num_bytes,
                                      NULL,
                                      &num_handles,
                                      MOJO_READ_MESSAGE_FLAG_NONE);
  data_buffer_.resize(num_bytes);
  handle_buffer_.resize(num_handles);
  if (result == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    // MOJO_RESULT_RESOURCE_EXHAUSTED was asking the caller that
    // it needs more bufer. So we re-read it with resized buffers.
    result = MojoReadMessage(pipe_.get().value(),
                             num_bytes ? &data_buffer_[0] : NULL,
                             &num_bytes,
                             num_handles ? &handle_buffer_[0] : NULL,
                             &num_handles,
                             MOJO_READ_MESSAGE_FLAG_NONE);
  }

  DCHECK(0 == num_bytes || data_buffer_.size() == num_bytes);
  DCHECK(0 == num_handles || handle_buffer_.size() == num_handles);
  return result;
}

void MessagePipeReader::DelayedDeleter::operator()(
    MessagePipeReader* ptr) const {
  ptr->Close();
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(&DeleteNow, ptr));
}

}  // namespace internal
}  // namespace IPC
