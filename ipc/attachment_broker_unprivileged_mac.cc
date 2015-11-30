// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_unprivileged_mac.h"

#include <mach/mach.h>

#include "base/mac/scoped_mach_port.h"
#include "base/process/process.h"
#include "ipc/attachment_broker_messages.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_sender.h"
#include "ipc/mach_port_attachment_mac.h"

namespace {

// Struct for receiving a complex message.
struct MachReceiveComplexMessage {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t data;
  mach_msg_trailer_t trailer;
};

// Receives a Mach port from |port_to_listen_on|, which should have exactly one
// queued message. Returns |MACH_PORT_NULL| on any error.
base::mac::ScopedMachSendRight ReceiveMachPort(mach_port_t port_to_listen_on) {
  MachReceiveComplexMessage recv_msg;
  mach_msg_header_t* recv_hdr = &recv_msg.header;
  recv_hdr->msgh_local_port = port_to_listen_on;
  recv_hdr->msgh_size = sizeof(recv_msg);

  kern_return_t kr =
      mach_msg(recv_hdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0,
               recv_hdr->msgh_size, port_to_listen_on, 0, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS)
    return base::mac::ScopedMachSendRight(MACH_PORT_NULL);
  if (recv_msg.header.msgh_id != 0)
    return base::mac::ScopedMachSendRight(MACH_PORT_NULL);
  return base::mac::ScopedMachSendRight(recv_msg.data.name);
}

}  // namespace

namespace IPC {

AttachmentBrokerUnprivilegedMac::AttachmentBrokerUnprivilegedMac() {}

AttachmentBrokerUnprivilegedMac::~AttachmentBrokerUnprivilegedMac() {}

bool AttachmentBrokerUnprivilegedMac::SendAttachmentToProcess(
    const scoped_refptr<IPC::BrokerableAttachment>& attachment,
    base::ProcessId destination_process) {
  switch (attachment->GetBrokerableType()) {
    case BrokerableAttachment::MACH_PORT: {
      internal::MachPortAttachmentMac* mach_port_attachment =
          static_cast<internal::MachPortAttachmentMac*>(attachment.get());
      internal::MachPortAttachmentMac::WireFormat format =
          mach_port_attachment->GetWireFormat(destination_process);
      bool success =
          get_sender()->Send(new AttachmentBrokerMsg_DuplicateMachPort(format));
      if (success)
        mach_port_attachment->reset_mach_port_ownership();
      return success;
    }
    default:
      NOTREACHED();
      return false;
  }
  return false;
}

bool AttachmentBrokerUnprivilegedMac::OnMessageReceived(const Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AttachmentBrokerUnprivilegedMac, msg)
    IPC_MESSAGE_HANDLER(AttachmentBrokerMsg_MachPortHasBeenDuplicated,
                        OnMachPortHasBeenDuplicated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AttachmentBrokerUnprivilegedMac::OnMachPortHasBeenDuplicated(
    const IPC::internal::MachPortAttachmentMac::WireFormat& wire_format) {
  // The IPC message was intended for a different process. Ignore it.
  if (wire_format.destination_process != base::Process::Current().Pid()) {
    // If everything is functioning correctly, this path should not be taken.
    // However, it's still important to validate all fields of the IPC message.
    LogError(WRONG_DESTINATION);
    return;
  }

  base::mac::ScopedMachReceiveRight message_port(wire_format.mach_port);
  base::mac::ScopedMachSendRight memory_object(
      ReceiveMachPort(message_port.get()));

  LogError(memory_object.get() == MACH_PORT_NULL ? ERR_RECEIVE_MACH_MESSAGE
                                                 : SUCCESS);

  IPC::internal::MachPortAttachmentMac::WireFormat translated_wire_format(
      memory_object.release(), wire_format.destination_process,
      wire_format.attachment_id);

  scoped_refptr<BrokerableAttachment> attachment(
      new IPC::internal::MachPortAttachmentMac(translated_wire_format));
  HandleReceivedAttachment(attachment);
}

}  // namespace IPC
