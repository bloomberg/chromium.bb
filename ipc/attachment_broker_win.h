// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_WIN_H_
#define IPC_ATTACHMENT_BROKER_WIN_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "ipc/attachment_broker.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/ipc_export.h"

namespace IPC {

class Sender;

// This class is an implementation of AttachmentBroker for the Windows platform
// for non-privileged processes.
class IPC_EXPORT AttachmentBrokerWin : public IPC::AttachmentBroker {
 public:
  AttachmentBrokerWin();
  ~AttachmentBrokerWin() override;

  // IPC::AttachmentBroker overrides.
  bool SendAttachmentToProcess(const BrokerableAttachment* attachment,
                               base::ProcessId destination_process) override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const Message& message) override;

  // |sender_| is used to send Messages to the broker. |sender_| must live at
  // least as long as this instance.
  void set_sender(IPC::Sender* sender) { sender_ = sender; }

 private:
  // IPC message handlers.
  void OnWinHandleHasBeenDuplicated(
      const IPC::internal::HandleAttachmentWin::WireFormat& wire_format);

  // |sender_| is used to send Messages to the broker. |sender_| must live at
  // least as long as this instance.
  IPC::Sender* sender_;
  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerWin);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_WIN_H_
