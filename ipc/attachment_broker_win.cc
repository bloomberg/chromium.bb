// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_win.h"

#include "ipc/attachment_broker_messages.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/ipc_sender.h"

namespace IPC {

AttachmentBrokerWin::AttachmentBrokerWin() {
}

AttachmentBrokerWin::~AttachmentBrokerWin() {
}

void AttachmentBrokerWin::OnReceiveDuplicatedHandle(
    HANDLE,
    BrokerableAttachment::AttachmentId id) {
  // TODO(erikchen): Implement me. http://crbug.com/493414
}

bool AttachmentBrokerWin::SendAttachmentToProcess(
    const BrokerableAttachment* attachment,
    base::ProcessId destination_process) {
  switch (attachment->GetBrokerableType()) {
    case BrokerableAttachment::WIN_HANDLE:
      const internal::HandleAttachmentWin* handle_attachment =
          static_cast<const internal::HandleAttachmentWin*>(attachment);
      internal::HandleAttachmentWin::WireFormat format =
          handle_attachment->GetWireFormat(destination_process);
      return sender_->Send(
          new AttachmentBrokerMsg_DuplicateWinHandle(format));
  }
  return false;
}

bool AttachmentBrokerWin::GetAttachmentWithId(
    BrokerableAttachment::AttachmentId id,
    BrokerableAttachment* attachment) {
  // TODO(erikchen): Implement me. http://crbug.com/493414
  return false;
}

}  // namespace IPC
