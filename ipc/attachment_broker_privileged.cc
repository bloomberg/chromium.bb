// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_privileged.h"

#include <algorithm>

#include "ipc/ipc_channel.h"

namespace IPC {

AttachmentBrokerPrivileged::AttachmentBrokerPrivileged() {}

AttachmentBrokerPrivileged::~AttachmentBrokerPrivileged() {}

void AttachmentBrokerPrivileged::RegisterCommunicationChannel(
    Channel* channel) {
  auto it = std::find(channels_.begin(), channels_.end(), channel);
  DCHECK(channels_.end() == it);
  channels_.push_back(channel);
}

void AttachmentBrokerPrivileged::DeregisterCommunicationChannel(
    Channel* channel) {
  auto it = std::find(channels_.begin(), channels_.end(), channel);
  if (it != channels_.end())
    channels_.erase(it);
}

Channel* AttachmentBrokerPrivileged::GetChannelWithProcessId(
    base::ProcessId id) {
  auto it = std::find_if(channels_.begin(), channels_.end(),
                         [id](Channel* c) { return c->GetPeerPID() == id; });
  if (it == channels_.end())
    return nullptr;
  return *it;
}

}  // namespace IPC
