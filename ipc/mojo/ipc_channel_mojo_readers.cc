// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo_readers.h"

#include "ipc/mojo/ipc_channel_mojo.h"

namespace IPC {
namespace internal {

//------------------------------------------------------------------------------

MessageReader::MessageReader(mojo::ScopedMessagePipeHandle pipe,
                             ChannelMojo* owner)
    : internal::MessagePipeReader(pipe.Pass()), owner_(owner) {
}

void MessageReader::OnMessageReceived() {
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
  owner_->OnMessageReceived(message);
}

void MessageReader::OnPipeClosed() {
  if (!owner_)
    return;
  owner_->OnPipeClosed(this);
  owner_ = NULL;
}

void MessageReader::OnPipeError(MojoResult error) {
  if (!owner_)
    return;
  owner_->OnPipeError(this);
}

bool MessageReader::Send(scoped_ptr<Message> message) {
  DCHECK(IsValid());

  message->TraceMessageBegin();
  std::vector<MojoHandle> handles;
#if defined(OS_POSIX) && !defined(OS_NACL)
  MojoResult read_result =
      ChannelMojo::ReadFromFileDescriptorSet(message.get(), &handles);
  if (read_result != MOJO_RESULT_OK) {
    std::for_each(handles.begin(), handles.end(), &MojoClose);
    CloseWithError(read_result);
    return false;
  }
#endif
  MojoResult write_result =
      MojoWriteMessage(handle(),
                       message->data(),
                       static_cast<uint32>(message->size()),
                       handles.empty() ? NULL : &handles[0],
                       static_cast<uint32>(handles.size()),
                       MOJO_WRITE_MESSAGE_FLAG_NONE);
  if (MOJO_RESULT_OK != write_result) {
    std::for_each(handles.begin(), handles.end(), &MojoClose);
    CloseWithError(write_result);
    return false;
  }

  return true;
}

}  // namespace internal
}  // namespace IPC
