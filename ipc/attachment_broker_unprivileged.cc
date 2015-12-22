// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_unprivileged.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_endpoint.h"

#if defined(OS_WIN)
#include "ipc/attachment_broker_unprivileged_win.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "ipc/attachment_broker_unprivileged_mac.h"
#endif

namespace IPC {

AttachmentBrokerUnprivileged::AttachmentBrokerUnprivileged()
    : sender_(nullptr) {
  IPC::AttachmentBroker::SetGlobal(this);
}

AttachmentBrokerUnprivileged::~AttachmentBrokerUnprivileged() {
  IPC::AttachmentBroker::SetGlobal(nullptr);
}

// static
scoped_ptr<AttachmentBrokerUnprivileged>
AttachmentBrokerUnprivileged::CreateBroker() {
#if defined(OS_WIN)
  return scoped_ptr<AttachmentBrokerUnprivileged>(
      new IPC::AttachmentBrokerUnprivilegedWin);
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  return scoped_ptr<AttachmentBrokerUnprivileged>(
      new IPC::AttachmentBrokerUnprivilegedMac);
#else
  return nullptr;
#endif
}

void AttachmentBrokerUnprivileged::DesignateBrokerCommunicationChannel(
    Endpoint* endpoint) {
  DCHECK(endpoint);
  DCHECK(!sender_);
  sender_ = endpoint;
  endpoint->SetAttachmentBrokerEndpoint(true);
}

void AttachmentBrokerUnprivileged::LogError(UMAError error) {
  UMA_HISTOGRAM_ENUMERATION(
      "IPC.AttachmentBrokerUnprivileged.BrokerAttachmentError", error,
      ERROR_MAX);
}

}  // namespace IPC
