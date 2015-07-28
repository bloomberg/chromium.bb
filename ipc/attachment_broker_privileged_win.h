// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_PRIVILEGED_WIN_H_
#define IPC_ATTACHMENT_BROKER_PRIVILEGED_WIN_H_

#include <vector>

#include "ipc/attachment_broker.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/ipc_export.h"

namespace IPC {

class Channel;

// This class is an implementation of AttachmentBroker for a privileged process
// on the Windows platform. When unprivileged processes want to send
// attachments, the attachments get routed through the privileged process, and
// more specifically, an instance of this class.
class IPC_EXPORT AttachmentBrokerPrivilegedWin : public AttachmentBroker {
 public:
  AttachmentBrokerPrivilegedWin();
  ~AttachmentBrokerPrivilegedWin() override;

  // IPC::AttachmentBroker overrides.
  bool SendAttachmentToProcess(const BrokerableAttachment* attachment,
                               base::ProcessId destination_process) override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const Message& message) override;

  // Each unprivileged process should have one IPC channel on which it
  // communicates attachment information with this privileged process. These
  // channels must be registered and deregistered with the Attachment Broker as
  // they are created and destroyed.
  void RegisterCommunicationChannel(Channel* channel);
  void DeregisterCommunicationChannel(Channel* channel);

 private:
  using HandleWireFormat = internal::HandleAttachmentWin::WireFormat;
  // IPC message handlers.
  void OnDuplicateWinHandle(const IPC::Message& message);

  // Duplicates |wire_Format| from |source_process| into its destination
  // process.
  HandleWireFormat DuplicateWinHandle(const HandleWireFormat& wire_format,
                                      base::ProcessId source_process);

  // If the HANDLE's destination is this process, queue it and notify the
  // observers. Otherwise, send it in an IPC to its destination.
  void RouteDuplicatedHandle(const HandleWireFormat& wire_format);

  std::vector<Channel*> channels_;
  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerPrivilegedWin);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_PRIVILEGED_WIN_H_
