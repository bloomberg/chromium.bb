// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel.h"

namespace IPC {

static Channel::MessageVerifier g_message_verifier = nullptr;

// static
scoped_ptr<Channel> Channel::CreateClient(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener,
    AttachmentBroker* broker) {
  return Channel::Create(channel_handle, Channel::MODE_CLIENT, listener,
                         broker);
}

// static
scoped_ptr<Channel> Channel::CreateNamedServer(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener,
    AttachmentBroker* broker) {
  return Channel::Create(channel_handle, Channel::MODE_NAMED_SERVER, listener,
                         broker);
}

// static
scoped_ptr<Channel> Channel::CreateNamedClient(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener,
    AttachmentBroker* broker) {
  return Channel::Create(channel_handle, Channel::MODE_NAMED_CLIENT, listener,
                         broker);
}

#if defined(OS_POSIX)
// static
scoped_ptr<Channel> Channel::CreateOpenNamedServer(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener,
    AttachmentBroker* broker) {
  return Channel::Create(channel_handle, Channel::MODE_OPEN_NAMED_SERVER,
                         listener, broker);
}
#endif

// static
scoped_ptr<Channel> Channel::CreateServer(
    const IPC::ChannelHandle& channel_handle,
    Listener* listener,
    AttachmentBroker* broker) {
  return Channel::Create(channel_handle, Channel::MODE_SERVER, listener,
                         broker);
}

Channel::~Channel() {
}

// static
void Channel::SetMessageVerifier(MessageVerifier verifier) {
  g_message_verifier = verifier;
}

// static
Channel::MessageVerifier Channel::GetMessageVerifier() {
  return g_message_verifier;
}

bool Channel::IsSendThreadSafe() const {
  return false;
}

}  // namespace IPC

