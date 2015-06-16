// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_win.h"

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

void AttachmentBrokerWin::SendAttachmentToProcess(
    BrokerableAttachment* attachment,
    base::ProcessId destination_process) {
  // TODO(erikchen): Implement me. http://crbug.com/493414
}

bool AttachmentBrokerWin::GetAttachmentWithId(
    BrokerableAttachment::AttachmentId id,
    BrokerableAttachment* attachment) {
  // TODO(erikchen): Implement me. http://crbug.com/493414
  return false;
}

}  // namespace IPC
