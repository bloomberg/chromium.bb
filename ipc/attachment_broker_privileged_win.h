// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_PRIVILEGED_WIN_H_
#define IPC_ATTACHMENT_BROKER_PRIVILEGED_WIN_H_

#include "base/macros.h"
#include "ipc/attachment_broker_privileged.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/ipc_export.h"

namespace IPC {

// This class is a concrete subclass of AttachmentBrokerPrivileged for the
// Windows platform.
class IPC_EXPORT AttachmentBrokerPrivilegedWin
    : public AttachmentBrokerPrivileged {
 public:
  AttachmentBrokerPrivilegedWin();
  ~AttachmentBrokerPrivilegedWin() override;

  // IPC::AttachmentBroker overrides.
  bool SendAttachmentToProcess(
      const scoped_refptr<IPC::BrokerableAttachment>& attachment,
      base::ProcessId destination_process) override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const Message& message) override;

 private:
  using HandleWireFormat = internal::HandleAttachmentWin::WireFormat;
  // IPC message handlers.
  void OnDuplicateWinHandle(const Message& message);

  // Duplicates |wire_Format| from |source_process| into its destination
  // process. Closes the original HANDLE.
  HandleWireFormat DuplicateWinHandle(const HandleWireFormat& wire_format,
                                      base::ProcessId source_process);

  // Copies an existing |wire_format|, but substitutes in a different handle.
  HandleWireFormat CopyWireFormat(const HandleWireFormat& wire_format,
                                  int handle);

  // If the HANDLE's destination is this process, queue it and notify the
  // observers. Otherwise, send it in an IPC to its destination.
  void RouteDuplicatedHandle(const HandleWireFormat& wire_format);

  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerPrivilegedWin);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_PRIVILEGED_WIN_H_
