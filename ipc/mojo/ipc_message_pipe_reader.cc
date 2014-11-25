// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_message_pipe_reader.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "ipc/mojo/ipc_channel_mojo.h"
#include "mojo/public/cpp/environment/environment.h"

namespace IPC {
namespace internal {

MessagePipeReader::MessagePipeReader(mojo::ScopedMessagePipeHandle handle,
                                     MessagePipeReader::Delegate* delegate)
    : pipe_wait_id_(0),
      pipe_(handle.Pass()),
      delegate_(delegate) {
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

bool MessagePipeReader::Send(scoped_ptr<Message> message) {
  DCHECK(IsValid());

  message->TraceMessageBegin();
  std::vector<MojoHandle> handles;
  MojoResult result = MOJO_RESULT_OK;
#if defined(OS_POSIX) && !defined(OS_NACL)
  result = ChannelMojo::ReadFromFileDescriptorSet(message.get(), &handles);
#endif
  if (result == MOJO_RESULT_OK) {
    result = MojoWriteMessage(handle(),
                              message->data(),
                              static_cast<uint32>(message->size()),
                              handles.empty() ? nullptr : &handles[0],
                              static_cast<uint32>(handles.size()),
                              MOJO_WRITE_MESSAGE_FLAG_NONE);
  }

  if (result != MOJO_RESULT_OK) {
    std::for_each(handles.begin(), handles.end(), &MojoClose);
    CloseWithError(result);
    return false;
  }

  return true;
}

// static
void MessagePipeReader::InvokePipeIsReady(void* closure, MojoResult result) {
  reinterpret_cast<MessagePipeReader*>(closure)->PipeIsReady(result);
}

void MessagePipeReader::OnMessageReceived() {
  Message message(data_buffer().empty() ? "" : &data_buffer()[0],
                  static_cast<uint32>(data_buffer().size()));

  std::vector<MojoHandle> handle_buffer;
  TakeHandleBuffer(&handle_buffer);
#if defined(OS_POSIX) && !defined(OS_NACL)
  MojoResult write_result =
      ChannelMojo::WriteToFileDescriptorSet(handle_buffer, &message);
  if (write_result != MOJO_RESULT_OK) {
    CloseWithError(write_result);
    return;
  }
#else
  DCHECK(handle_buffer.empty());
#endif

  message.TraceMessageEnd();
  delegate_->OnMessageReceived(message);
}

void MessagePipeReader::OnPipeClosed() {
  if (!delegate_)
    return;
  delegate_->OnPipeClosed(this);
  delegate_ = nullptr;
}

void MessagePipeReader::OnPipeError(MojoResult error) {
  if (!delegate_)
    return;
  delegate_->OnPipeError(this);
}

MojoResult MessagePipeReader::ReadMessageBytes() {
  DCHECK(handle_buffer_.empty());

  uint32_t num_bytes = static_cast<uint32_t>(data_buffer_.size());
  uint32_t num_handles = 0;
  MojoResult result = MojoReadMessage(pipe_.get().value(),
                                      num_bytes ? &data_buffer_[0] : nullptr,
                                      &num_bytes,
                                      nullptr,
                                      &num_handles,
                                      MOJO_READ_MESSAGE_FLAG_NONE);
  data_buffer_.resize(num_bytes);
  handle_buffer_.resize(num_handles);
  if (result == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    // MOJO_RESULT_RESOURCE_EXHAUSTED was asking the caller that
    // it needs more bufer. So we re-read it with resized buffers.
    result = MojoReadMessage(pipe_.get().value(),
                             num_bytes ? &data_buffer_[0] : nullptr,
                             &num_bytes,
                             num_handles ? &handle_buffer_[0] : nullptr,
                             &num_handles,
                             MOJO_READ_MESSAGE_FLAG_NONE);
  }

  DCHECK(0 == num_bytes || data_buffer_.size() == num_bytes);
  DCHECK(0 == num_handles || handle_buffer_.size() == num_handles);
  return result;
}

void MessagePipeReader::PipeIsReady(MojoResult wait_result) {
  pipe_wait_id_ = 0;

  if (wait_result != MOJO_RESULT_OK) {
    if (wait_result != MOJO_RESULT_ABORTED) {
      // FAILED_PRECONDITION happens every time the peer is dead so
      // it isn't worth polluting the log message.
      DLOG_IF(WARNING, wait_result != MOJO_RESULT_FAILED_PRECONDITION)
          << "Pipe got error from the waiter. Closing: "
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

void MessagePipeReader::DelayedDeleter::operator()(
    MessagePipeReader* ptr) const {
  ptr->Close();
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(&DeleteNow, ptr));
}

}  // namespace internal
}  // namespace IPC
