// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/message_port.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

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

  DCHECK(!watcher_handle_.is_valid());
  MojoResult rv = CreateWatcher(&State::CallOnHandleReady, &watcher_handle_);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  // We use a scoped_refptr<State> instance as the watch context. This is owned
  // by the watch and deleted upon receiving a cancellation notification.
  scoped_refptr<State>* state_ref = new scoped_refptr<State>(this);
  context_ = reinterpret_cast<uintptr_t>(state_ref);

  // NOTE: An HTML MessagePort does not receive an event to tell it when the
  // peer has gone away, so we only watch for readability here.
  rv = MojoWatch(watcher_handle_.get().value(), handle_.get().value(),
                 MOJO_HANDLE_SIGNAL_READABLE, context_);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  ArmWatcher();
}

void MessagePort::State::CancelWatch() {
  watcher_handle_.reset();
  context_ = 0;
}

MessagePort::State::~State() = default;

void MessagePort::State::ArmWatcher() {
  if (!watcher_handle_.is_valid())
    return;

  uint32_t num_ready_contexts = 1;
  uintptr_t ready_context;
  MojoResult ready_result;
  MojoHandleSignalsState ready_state;
  MojoResult rv =
      MojoArmWatcher(watcher_handle_.get().value(), &num_ready_contexts,
                     &ready_context, &ready_result, &ready_state);
  if (rv == MOJO_RESULT_OK)
    return;

  // The watcher could not be armed because it would notify immediately.
  DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, rv);
  DCHECK_EQ(1u, num_ready_contexts);
  DCHECK_EQ(context_, ready_context);

  if (ready_result == MOJO_RESULT_OK) {
    // The handle is already signaled, so we trigger a callback now.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&State::OnHandleReady, this, MOJO_RESULT_OK));
    return;
  }

  if (ready_result == MOJO_RESULT_FAILED_PRECONDITION) {
    DVLOG(1) << this << " MojoArmWatcher failed because of a broken pipe.";
    return;
  }

  NOTREACHED();
}

void MessagePort::State::OnHandleReady(MojoResult result) {
  if (result == MOJO_RESULT_OK && callback_) {
    callback_.Run();
    ArmWatcher();
  } else {
    // And now his watch is ended.
  }
}

// static
void MessagePort::State::CallOnHandleReady(uintptr_t context,
                                           MojoResult result,
                                           MojoHandleSignalsState signals_state,
                                           MojoWatcherNotificationFlags flags) {
  auto* state_ref = reinterpret_cast<scoped_refptr<State>*>(context);
  if (result == MOJO_RESULT_CANCELLED) {
    // Last notification. Delete the watch's owned State ref.
    delete state_ref;
  } else {
    (*state_ref)->OnHandleReady(result);
  }
}

}  // namespace content
