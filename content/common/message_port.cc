// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/message_port.h"

#include "base/logging.h"

namespace content {

MessagePort::~MessagePort() {
}

MessagePort::MessagePort() : state_(new State()) {
}

MessagePort::MessagePort(const MessagePort& other) : state_(other.state_) {
}

MessagePort& MessagePort::operator=(const MessagePort& other) {
  state_ = other.state_;
  return *this;
}

MessagePort::MessagePort(mojo::ScopedMessagePipeHandle handle)
    : state_(new State(std::move(handle))) {
}

const mojo::ScopedMessagePipeHandle& MessagePort::GetHandle() const {
  return state_->handle_;
}

mojo::ScopedMessagePipeHandle MessagePort::ReleaseHandle() const {
  state_->CancelWatch();
  return std::move(state_->handle_);
}

// static
std::vector<mojo::ScopedMessagePipeHandle> MessagePort::ReleaseHandles(
    const std::vector<MessagePort>& ports) {
  std::vector<mojo::ScopedMessagePipeHandle> handles(ports.size());
  for (size_t i = 0; i < ports.size(); ++i)
    handles[i] = ports[i].ReleaseHandle();
  return handles;
}

void MessagePort::PostMessage(const base::string16& encoded_message,
                              std::vector<MessagePort> ports) {
  DCHECK(state_->handle_.is_valid());

  uint32_t num_bytes = encoded_message.size() * sizeof(base::char16);

  // NOTE: It is OK to ignore the return value of MojoWriteMessage here. HTML
  // MessagePorts have no way of reporting when the peer is gone.

  if (ports.empty()) {
    MojoWriteMessage(state_->handle_.get().value(),
                     encoded_message.data(),
                     num_bytes,
                     nullptr,
                     0,
                     MOJO_WRITE_MESSAGE_FLAG_NONE);
  } else {
    uint32_t num_handles = static_cast<uint32_t>(ports.size());
    std::unique_ptr<MojoHandle[]> handles(new MojoHandle[num_handles]);
    for (uint32_t i = 0; i < num_handles; ++i)
      handles[i] = ports[i].ReleaseHandle().release().value();
    MojoWriteMessage(state_->handle_.get().value(),
                     encoded_message.data(),
                     num_bytes,
                     handles.get(),
                     num_handles,
                     MOJO_WRITE_MESSAGE_FLAG_NONE);
  }
}

bool MessagePort::GetMessage(base::string16* encoded_message,
                             std::vector<MessagePort>* ports) {
  DCHECK(state_->handle_.is_valid());

  uint32_t num_bytes = 0;
  uint32_t num_handles = 0;

  MojoResult rv = MojoReadMessage(state_->handle_.get().value(),
                                  nullptr,
                                  &num_bytes,
                                  nullptr,
                                  &num_handles,
                                  MOJO_READ_MESSAGE_FLAG_NONE);
  if (rv == MOJO_RESULT_OK) {
    encoded_message->clear();
    ports->clear();
    return true;
  }
  if (rv != MOJO_RESULT_RESOURCE_EXHAUSTED)
    return false;

  CHECK(num_bytes % 2 == 0);

  base::string16 buffer;
  buffer.resize(num_bytes / sizeof(base::char16));

  std::unique_ptr<MojoHandle[]> handles;
  if (num_handles)
    handles.reset(new MojoHandle[num_handles]);

  rv = MojoReadMessage(state_->handle_.get().value(),
                       num_bytes ? &buffer[0] : nullptr,
                       &num_bytes,
                       handles.get(),
                       &num_handles,
                       MOJO_READ_MESSAGE_FLAG_NONE);
  if (rv != MOJO_RESULT_OK)
    return false;

  buffer.swap(*encoded_message);

  if (num_handles) {
    ports->resize(static_cast<size_t>(num_handles));
    for (uint32_t i = 0; i < num_handles; ++i) {
      ports->at(i) = MessagePort(
          mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(handles[i])));
    }
  }
  return true;
}

void MessagePort::SetCallback(const base::Closure& callback) {
  state_->CancelWatch();
  state_->callback_ = callback;
  state_->AddWatch();
}

void MessagePort::ClearCallback() {
  state_->CancelWatch();
  state_->callback_.Reset();
}

MessagePort::State::State() {
}

MessagePort::State::State(mojo::ScopedMessagePipeHandle handle)
    : handle_(std::move(handle)) {
}

void MessagePort::State::AddWatch() {
  if (!callback_)
    return;

  // NOTE: An HTML MessagePort does not receive an event to tell it when the
  // peer has gone away, so we only watch for readability here.
  MojoResult rv = MojoWatch(handle_.get().value(),
                            MOJO_HANDLE_SIGNAL_READABLE,
                            &MessagePort::State::OnHandleReady,
                            reinterpret_cast<uintptr_t>(this));
  if (rv != MOJO_RESULT_OK)
    DVLOG(1) << this << " MojoWatch failed: " << rv;
}

void MessagePort::State::CancelWatch() {
  if (!callback_)
    return;

  // NOTE: This synchronizes with the thread where OnHandleReady runs so we are
  // sure to not be racing with it.
  MojoCancelWatch(handle_.get().value(), reinterpret_cast<uintptr_t>(this));
}

// static
void MessagePort::State::OnHandleReady(
    uintptr_t context,
    MojoResult result,
    MojoHandleSignalsState signals_state,
    MojoWatchNotificationFlags flags) {
  if (result == MOJO_RESULT_OK) {
    reinterpret_cast<MessagePort::State*>(context)->callback_.Run();
  } else {
    // And now his watch is ended.
  }
}

MessagePort::State::~State() {
  CancelWatch();
}

}  // namespace content
