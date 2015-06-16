// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_WIN_H_
#define IPC_ATTACHMENT_BROKER_WIN_H_

#include "ipc/attachment_broker.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_sender.h"

namespace IPC {

// This class is an implementation of AttachmentBroker for the Windows platform.
class IPC_EXPORT AttachmentBrokerWin : public IPC::AttachmentBroker {
 public:
  AttachmentBrokerWin();
  virtual ~AttachmentBrokerWin();

  // In a non-broker process, the single instance of this class listens for
  // an IPC from the broker process indicating that a new attachment has been
  // duplicated.
  void OnReceiveDuplicatedHandle(HANDLE, BrokerableAttachment::AttachmentId id);

  // IPC::AttachmentBroker overrides.
  void SendAttachmentToProcess(BrokerableAttachment* attachment,
                               base::ProcessId destination_process) override;
  bool GetAttachmentWithId(BrokerableAttachment::AttachmentId id,
                           BrokerableAttachment* attachment) override;
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_WIN_H_
