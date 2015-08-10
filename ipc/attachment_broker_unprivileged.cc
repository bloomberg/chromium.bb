// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_unprivileged.h"

#include "ipc/ipc_channel.h"
#include "ipc/ipc_endpoint.h"

namespace IPC {

AttachmentBrokerUnprivileged::AttachmentBrokerUnprivileged()
    : sender_(nullptr) {}

AttachmentBrokerUnprivileged::~AttachmentBrokerUnprivileged() {}

void AttachmentBrokerUnprivileged::DesignateBrokerCommunicationChannel(
    Endpoint* endpoint) {
  DCHECK(endpoint);
  DCHECK(!sender_);
  sender_ = endpoint;
  endpoint->SetAttachmentBrokerEndpoint(true);
}

}  // namespace IPC
