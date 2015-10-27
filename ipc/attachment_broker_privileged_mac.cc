// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_privileged_mac.h"

#include "base/mac/scoped_mach_port.h"
#include "base/memory/shared_memory.h"
#include "base/process/port_provider_mac.h"
#include "base/process/process.h"
#include "ipc/attachment_broker_messages.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/ipc_channel.h"
#include "ipc/mach_port_attachment_mac.h"

namespace {

// Struct for sending a complex Mach message.
struct MachSendComplexMessage {
  mach_msg_header_t header;
  mach_msg_body_t body;
  mach_msg_port_descriptor_t data;
};

// Sends a Mach port to |endpoint|. Assumes that |endpoint| is a send once
// right. Takes ownership of |endpoint|.
kern_return_t SendMachPort(mach_port_t endpoint,
                           mach_port_t port_to_send,
                           int disposition) {
  MachSendComplexMessage send_msg;
  send_msg.header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0) | MACH_MSGH_BITS_COMPLEX;
  send_msg.header.msgh_size = sizeof(send_msg);
  send_msg.header.msgh_remote_port = endpoint;
  send_msg.header.msgh_local_port = MACH_PORT_NULL;
  send_msg.header.msgh_reserved = 0;
  send_msg.header.msgh_id = 0;
  send_msg.body.msgh_descriptor_count = 1;
  send_msg.data.name = port_to_send;
  send_msg.data.disposition = disposition;
  send_msg.data.type = MACH_MSG_PORT_DESCRIPTOR;

  return mach_msg(&send_msg.header,
                  MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                  send_msg.header.msgh_size,
                  0,                // receive limit
                  MACH_PORT_NULL,   // receive name
                  0,                // timeout
                  MACH_PORT_NULL);  // notification port
}

}  // namespace

namespace IPC {

AttachmentBrokerPrivilegedMac::AttachmentBrokerPrivilegedMac(
    base::PortProvider* port_provider)
    : port_provider_(port_provider) {}

AttachmentBrokerPrivilegedMac::~AttachmentBrokerPrivilegedMac() {}

bool AttachmentBrokerPrivilegedMac::SendAttachmentToProcess(
    BrokerableAttachment* attachment,
    base::ProcessId destination_process) {
  switch (attachment->GetBrokerableType()) {
    case BrokerableAttachment::MACH_PORT: {
      internal::MachPortAttachmentMac* mach_port_attachment =
          static_cast<internal::MachPortAttachmentMac*>(attachment);
      MachPortWireFormat wire_format =
          mach_port_attachment->GetWireFormat(destination_process);

      if (destination_process == base::Process::Current().Pid()) {
        RouteWireFormatToSelf(wire_format);
        mach_port_attachment->reset_mach_port_ownership();
        return true;
      }

      mach_port_name_t intermediate_port = CreateIntermediateMachPort(
          wire_format.destination_process,
          base::mac::ScopedMachSendRight(wire_format.mach_port));
      mach_port_attachment->reset_mach_port_ownership();
      if (intermediate_port == MACH_PORT_NULL) {
        LogError(ERROR_MAKE_INTERMEDIATE);
        return false;
      }

      MachPortWireFormat intermediate_wire_format =
          CopyWireFormat(wire_format, intermediate_port);
      RouteWireFormatToAnother(intermediate_wire_format);
      return true;
    }
    default:
      NOTREACHED();
      return false;
  }
  return false;
}

bool AttachmentBrokerPrivilegedMac::OnMessageReceived(const Message& msg) {
  bool handled = true;
  switch (msg.type()) {
    IPC_MESSAGE_HANDLER_GENERIC(AttachmentBrokerMsg_DuplicateMachPort,
                                OnDuplicateMachPort(msg))
    IPC_MESSAGE_UNHANDLED(handled = false)
  }
  return handled;
}

void AttachmentBrokerPrivilegedMac::OnDuplicateMachPort(
    const IPC::Message& message) {
  AttachmentBrokerMsg_DuplicateMachPort::Param param;
  if (!AttachmentBrokerMsg_DuplicateMachPort::Read(&message, &param)) {
    LogError(ERROR_PARSE_DUPLICATE_MACH_PORT_MESSAGE);
    return;
  }
  IPC::internal::MachPortAttachmentMac::WireFormat wire_format =
      base::get<0>(param);

  if (wire_format.destination_process == base::kNullProcessId) {
    LogError(NO_DESTINATION);
    return;
  }

  // Acquire a send right to the Mach port.
  base::ProcessId sender_pid = message.get_sender_pid();
  DCHECK_NE(sender_pid, base::GetCurrentProcId());
  base::mac::ScopedMachSendRight send_right(
      AcquireSendRight(sender_pid, wire_format.mach_port));

  if (wire_format.destination_process == base::GetCurrentProcId()) {
    // Intentionally leak the reference, as the consumer of the Chrome IPC
    // message will take ownership.
    mach_port_t final_mach_port = send_right.release();
    MachPortWireFormat final_wire_format(
        CopyWireFormat(wire_format, final_mach_port));
    RouteWireFormatToSelf(final_wire_format);
    return;
  }

  mach_port_name_t intermediate_mach_port = CreateIntermediateMachPort(
      wire_format.destination_process,
      base::mac::ScopedMachSendRight(send_right.release()));
  RouteWireFormatToAnother(CopyWireFormat(wire_format, intermediate_mach_port));
}

void AttachmentBrokerPrivilegedMac::RouteWireFormatToSelf(
    const MachPortWireFormat& wire_format) {
  DCHECK_EQ(wire_format.destination_process, base::Process::Current().Pid());
  scoped_refptr<BrokerableAttachment> attachment(
      new internal::MachPortAttachmentMac(wire_format));
  HandleReceivedAttachment(attachment);
}

void AttachmentBrokerPrivilegedMac::RouteWireFormatToAnother(
    const MachPortWireFormat& wire_format) {
  DCHECK_NE(wire_format.destination_process, base::Process::Current().Pid());

  // Another process is the destination.
  base::ProcessId dest = wire_format.destination_process;
  Sender* sender = GetSenderWithProcessId(dest);
  if (!sender) {
    // Assuming that this message was not sent from a malicious process, the
    // channel endpoint that would have received this message will block
    // forever.
    LOG(ERROR) << "Failed to deliver brokerable attachment to process with id: "
               << dest;
    LogError(DESTINATION_NOT_FOUND);
    return;
  }

  LogError(DESTINATION_FOUND);
  sender->Send(new AttachmentBrokerMsg_MachPortHasBeenDuplicated(wire_format));
}

mach_port_name_t AttachmentBrokerPrivilegedMac::CreateIntermediateMachPort(
    base::ProcessId pid,
    base::mac::ScopedMachSendRight port_to_insert) {
  DCHECK_NE(pid, base::GetCurrentProcId());
  mach_port_t task_port = port_provider_->TaskForPid(pid);
  if (task_port == MACH_PORT_NULL) {
    LogError(ERROR_TASK_FOR_PID);
    return MACH_PORT_NULL;
  }
  return CreateIntermediateMachPort(
      task_port, base::mac::ScopedMachSendRight(port_to_insert.release()));
}

mach_port_name_t AttachmentBrokerPrivilegedMac::CreateIntermediateMachPort(
    mach_port_t task_port,
    base::mac::ScopedMachSendRight port_to_insert) {
  DCHECK_NE(mach_task_self(), task_port);
  DCHECK_NE(static_cast<mach_port_name_t>(MACH_PORT_NULL), task_port);

  // Make a port with receive rights in the destination task.
  mach_port_name_t endpoint;
  kern_return_t kr =
      mach_port_allocate(task_port, MACH_PORT_RIGHT_RECEIVE, &endpoint);
  if (kr != KERN_SUCCESS) {
    LogError(ERROR_MAKE_RECEIVE_PORT);
    return MACH_PORT_NULL;
  }

  // Change its message queue limit so that it accepts one message.
  mach_port_limits limits = {};
  limits.mpl_qlimit = 1;
  kr = mach_port_set_attributes(task_port, endpoint, MACH_PORT_LIMITS_INFO,
                                reinterpret_cast<mach_port_info_t>(&limits),
                                MACH_PORT_LIMITS_INFO_COUNT);
  if (kr != KERN_SUCCESS) {
    LogError(ERROR_SET_ATTRIBUTES);
    mach_port_deallocate(task_port, endpoint);
    return MACH_PORT_NULL;
  }

  // Get a send right.
  mach_port_t send_once_right;
  mach_msg_type_name_t send_right_type;
  kr =
      mach_port_extract_right(task_port, endpoint, MACH_MSG_TYPE_MAKE_SEND_ONCE,
                              &send_once_right, &send_right_type);
  if (kr != KERN_SUCCESS) {
    LogError(ERROR_EXTRACT_RIGHT);
    mach_port_deallocate(task_port, endpoint);
    return MACH_PORT_NULL;
  }
  DCHECK_EQ(static_cast<mach_msg_type_name_t>(MACH_MSG_TYPE_PORT_SEND_ONCE),
            send_right_type);

  // This call takes ownership of |send_once_right|.
  kr = SendMachPort(
      send_once_right, port_to_insert.get(), MACH_MSG_TYPE_COPY_SEND);
  if (kr != KERN_SUCCESS) {
    LogError(ERROR_SEND_MACH_PORT);
    mach_port_deallocate(task_port, endpoint);
    return MACH_PORT_NULL;
  }

  // Endpoint is intentionally leaked into the destination task. An IPC must be
  // sent to the destination task so that it can clean up this port.
  return endpoint;
}

base::mac::ScopedMachSendRight AttachmentBrokerPrivilegedMac::AcquireSendRight(
    base::ProcessId pid,
    mach_port_name_t named_right) {
  if (pid == base::GetCurrentProcId()) {
    kern_return_t kr = mach_port_mod_refs(mach_task_self(), named_right,
                                          MACH_PORT_RIGHT_SEND, 1);
    if (kr != KERN_SUCCESS)
      return base::mac::ScopedMachSendRight(MACH_PORT_NULL);
    return base::mac::ScopedMachSendRight(named_right);
  }

  mach_port_t task_port = port_provider_->TaskForPid(pid);
  return ExtractNamedRight(task_port, named_right);
}

base::mac::ScopedMachSendRight AttachmentBrokerPrivilegedMac::ExtractNamedRight(
    mach_port_t task_port,
    mach_port_name_t named_right) {
  mach_port_t extracted_right = MACH_PORT_NULL;
  mach_msg_type_name_t extracted_right_type;
  kern_return_t kr =
      mach_port_extract_right(task_port, named_right, MACH_MSG_TYPE_COPY_SEND,
                              &extracted_right, &extracted_right_type);
  if (kr != KERN_SUCCESS)
    return base::mac::ScopedMachSendRight(MACH_PORT_NULL);

  DCHECK_EQ(static_cast<mach_msg_type_name_t>(MACH_MSG_TYPE_PORT_SEND),
            extracted_right_type);

  // Decrement the reference count of the send right from the source process.
  kr = mach_port_mod_refs(task_port, named_right, MACH_PORT_RIGHT_SEND, -1);
  if (kr != KERN_SUCCESS) {
    LogError(ERROR_DECREASE_REF);
    // Failure does not actually affect attachment brokering, so there's no need
    // to return |MACH_PORT_NULL|.
  }

  return base::mac::ScopedMachSendRight(extracted_right);
}

AttachmentBrokerPrivilegedMac::MachPortWireFormat
AttachmentBrokerPrivilegedMac::CopyWireFormat(
    const MachPortWireFormat& wire_format,
    uint32_t mach_port) {
  return MachPortWireFormat(mach_port, wire_format.destination_process,
                            wire_format.attachment_id);
}

}  // namespace IPC
