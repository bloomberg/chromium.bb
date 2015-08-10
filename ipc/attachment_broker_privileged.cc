// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_privileged.h"

#include <algorithm>

#include "ipc/ipc_endpoint.h"

namespace IPC {

AttachmentBrokerPrivileged::AttachmentBrokerPrivileged() {}

AttachmentBrokerPrivileged::~AttachmentBrokerPrivileged() {}

void AttachmentBrokerPrivileged::RegisterCommunicationChannel(
    Endpoint* endpoint) {
  endpoint->SetAttachmentBrokerEndpoint(true);
  auto it = std::find(endpoints_.begin(), endpoints_.end(), endpoint);
  DCHECK(endpoints_.end() == it);
  endpoints_.push_back(endpoint);
}

void AttachmentBrokerPrivileged::DeregisterCommunicationChannel(
    Endpoint* endpoint) {
  auto it = std::find(endpoints_.begin(), endpoints_.end(), endpoint);
  if (it != endpoints_.end())
    endpoints_.erase(it);
}

Sender* AttachmentBrokerPrivileged::GetSenderWithProcessId(base::ProcessId id) {
  auto it = std::find_if(endpoints_.begin(), endpoints_.end(),
                         [id](Endpoint* c) { return c->GetPeerPID() == id; });
  if (it == endpoints_.end())
    return nullptr;
  return *it;
}

}  // namespace IPC
