// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_message_pipe_reader.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "ipc/mojo/ipc_channel_mojo.h"

namespace IPC {
namespace internal {

MessagePipeReader::MessagePipeReader(
    mojom::ChannelAssociatedPtr sender,
    mojo::AssociatedInterfaceRequest<mojom::Channel> receiver,
    base::ProcessId peer_pid,
    MessagePipeReader::Delegate* delegate)
    : delegate_(delegate),
      peer_pid_(peer_pid),
      sender_(std::move(sender)),
      binding_(this, std::move(receiver)) {
  sender_.set_connection_error_handler(
      base::Bind(&MessagePipeReader::OnPipeError, base::Unretained(this),
                 MOJO_RESULT_FAILED_PRECONDITION));
  binding_.set_connection_error_handler(
      base::Bind(&MessagePipeReader::OnPipeError, base::Unretained(this),
                 MOJO_RESULT_FAILED_PRECONDITION));
}

MessagePipeReader::~MessagePipeReader() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // The pipe should be closed before deletion.
}

void MessagePipeReader::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sender_.reset();
  if (binding_.is_bound())
    binding_.Close();
  OnPipeClosed();
}

void MessagePipeReader::CloseWithError(MojoResult error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  OnPipeError(error);
}

bool MessagePipeReader::Send(scoped_ptr<Message> message) {
  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                         "MessagePipeReader::Send",
                         message->flags(),
                         TRACE_EVENT_FLAG_FLOW_OUT);
  mojo::Array<mojom::SerializedHandlePtr> handles(nullptr);
  MojoResult result = MOJO_RESULT_OK;
  result = ChannelMojo::ReadFromMessageAttachmentSet(message.get(), &handles);
  if (result != MOJO_RESULT_OK) {
    CloseWithError(result);
    return false;
  }
  mojo::Array<uint8_t> data(message->size());
  std::copy(reinterpret_cast<const uint8_t*>(message->data()),
            reinterpret_cast<const uint8_t*>(message->data()) + message->size(),
            &data[0]);
  sender_->Receive(std::move(data), std::move(handles));
  DVLOG(4) << "Send " << message->type() << ": " << message->size();
  return true;
}

void MessagePipeReader::Receive(
    mojo::Array<uint8_t> data,
    mojo::Array<mojom::SerializedHandlePtr> handles) {
  Message message(
      data.size() == 0 ? "" : reinterpret_cast<const char*>(&data[0]),
      static_cast<uint32_t>(data.size()));
  message.set_sender_pid(peer_pid_);

  DVLOG(4) << "Receive " << message.type() << ": " << message.size();
  MojoResult write_result =
      ChannelMojo::WriteToMessageAttachmentSet(std::move(handles), &message);
  if (write_result != MOJO_RESULT_OK) {
    CloseWithError(write_result);
    return;
  }

  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("ipc.flow"),
                         "MessagePipeReader::OnMessageReceived",
                         message.flags(),
                         TRACE_EVENT_FLAG_FLOW_IN);
  delegate_->OnMessageReceived(message);
}

void MessagePipeReader::OnPipeClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!delegate_)
    return;
  delegate_->OnPipeClosed(this);
  delegate_ = nullptr;
}

void MessagePipeReader::OnPipeError(MojoResult error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delegate_)
    delegate_->OnPipeError(this);
  Close();
}

void MessagePipeReader::DelayedDeleter::operator()(
    MessagePipeReader* ptr) const {
  ptr->Close();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(&DeleteNow, ptr));
}

}  // namespace internal
}  // namespace IPC
