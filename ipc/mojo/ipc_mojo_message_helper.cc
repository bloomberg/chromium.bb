// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_mojo_message_helper.h"

#include <utility>

#include "ipc/mojo/ipc_mojo_handle_attachment.h"

namespace IPC {

// static
bool MojoMessageHelper::WriteMessagePipeTo(
    Message* message,
    mojo::ScopedMessagePipeHandle handle) {
  message->WriteAttachment(new internal::MojoHandleAttachment(
      mojo::ScopedHandle::From(std::move(handle))));
  return true;
}

// static
bool MojoMessageHelper::ReadMessagePipeFrom(
    const Message* message,
    base::PickleIterator* iter,
    mojo::ScopedMessagePipeHandle* handle) {
  scoped_refptr<MessageAttachment> attachment;
  if (!message->ReadAttachment(iter, &attachment)) {
    LOG(ERROR) << "Failed to read attachment for message pipe.";
    return false;
  }

  if (attachment->GetType() != MessageAttachment::TYPE_MOJO_HANDLE) {
    LOG(ERROR) << "Unxpected attachment type:" << attachment->GetType();
    return false;
  }

  handle->reset(mojo::MessagePipeHandle(
      static_cast<internal::MojoHandleAttachment*>(attachment.get())
          ->TakeHandle()
          .release()
          .value()));
  return true;
}

MojoMessageHelper::MojoMessageHelper() {
}

}  // namespace IPC
