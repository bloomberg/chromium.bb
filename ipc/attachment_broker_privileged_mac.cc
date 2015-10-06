// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_privileged_mac.h"

#include "base/mac/scoped_mach_port.h"
#include "base/memory/shared_memory.h"
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

AttachmentBrokerPrivilegedMac::AttachmentBrokerPrivilegedMac()
    : port_provider_(nullptr) {}

AttachmentBrokerPrivilegedMac::~AttachmentBrokerPrivilegedMac() {}

void AttachmentBrokerPrivilegedMac::SetPortProvider(
    base::PortProvider* port_provider) {
  CHECK(!port_provider_);
  port_provider_ = port_provider;
}

bool AttachmentBrokerPrivilegedMac::SendAttachmentToProcess(
    const BrokerableAttachment* attachment,
    base::ProcessId destination_process) {
  switch (attachment->GetBrokerableType()) {
    case BrokerableAttachment::MACH_PORT: {
      const internal::MachPortAttachmentMac* mach_port_attachment =
          static_cast<const internal::MachPortAttachmentMac*>(attachment);
      MachPortWireFormat wire_format =
          mach_port_attachment->GetWireFormat(destination_process);
      MachPortWireFormat new_wire_format =
          DuplicateMachPort(wire_format, base::Process::Current().Pid());
      if (new_wire_format.mach_port == 0)
        return false;
      RouteDuplicatedMachPort(new_wire_format);
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
  if (!AttachmentBrokerMsg_DuplicateMachPort::Read(&message, &param))
    return;
  IPC::internal::MachPortAttachmentMac::WireFormat wire_format =
      base::get<0>(param);

  if (wire_format.destination_process == base::kNullProcessId) {
    LogError(NO_DESTINATION);
    return;
  }

  MachPortWireFormat new_wire_format =
      DuplicateMachPort(wire_format, message.get_sender_pid());
  RouteDuplicatedMachPort(new_wire_format);
}

void AttachmentBrokerPrivilegedMac::RouteDuplicatedMachPort(
    const MachPortWireFormat& wire_format) {
  // This process is the destination.
  if (wire_format.destination_process == base::Process::Current().Pid()) {
    scoped_refptr<BrokerableAttachment> attachment(
        new internal::MachPortAttachmentMac(wire_format));
    HandleReceivedAttachment(attachment);
    return;
  }

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

AttachmentBrokerPrivilegedMac::MachPortWireFormat
AttachmentBrokerPrivilegedMac::DuplicateMachPort(
    const MachPortWireFormat& wire_format,
    base::ProcessId source_pid) {
  // If the source is the destination, just increment the ref count.
  if (source_pid == wire_format.destination_process) {
    mach_port_t task_port =
        port_provider_->TaskForPid(wire_format.destination_process);
    kern_return_t kr = mach_port_mod_refs(task_port, wire_format.mach_port,
                                          MACH_PORT_RIGHT_SEND, 1);
    if (kr != KERN_SUCCESS) {
      // TODO(erikchen): UMA metric.
      return CopyWireFormat(wire_format, MACH_PORT_NULL);
    }
    return wire_format;
  }

  // Acquire a send right to the memory object.
  base::mac::ScopedMachSendRight memory_object(
      AcquireSendRight(source_pid, wire_format.mach_port));
  if (!memory_object)
    return CopyWireFormat(wire_format, MACH_PORT_NULL);

  mach_port_t task_port =
      port_provider_->TaskForPid(wire_format.destination_process);
  mach_port_name_t inserted_memory_object =
      InsertIndirectMachPort(task_port, memory_object);
  return CopyWireFormat(wire_format, inserted_memory_object);
}

mach_port_name_t AttachmentBrokerPrivilegedMac::InsertIndirectMachPort(
    mach_port_t task_port,
    mach_port_t port_to_insert) {
  DCHECK_NE(mach_task_self(), task_port);

  // Make a port with receive rights in the destination task.
  mach_port_name_t endpoint;
  kern_return_t kr =
      mach_port_allocate(task_port, MACH_PORT_RIGHT_RECEIVE, &endpoint);
  if (kr != KERN_SUCCESS) {
    // TODO(erikchen): UMA metric.
    return MACH_PORT_NULL;
  }

  // Change its message queue limit so that it accepts one message.
  mach_port_limits limits = {};
  limits.mpl_qlimit = 1;
  kr = mach_port_set_attributes(task_port, endpoint, MACH_PORT_LIMITS_INFO,
                                reinterpret_cast<mach_port_info_t>(&limits),
                                MACH_PORT_LIMITS_INFO_COUNT);
  if (kr != KERN_SUCCESS) {
    // TODO(erikchen): UMA metric.
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
    // TODO(erikchen): UMA metric.
    mach_port_deallocate(task_port, endpoint);
    return MACH_PORT_NULL;
  }
  DCHECK_EQ(static_cast<mach_msg_type_name_t>(MACH_MSG_TYPE_PORT_SEND_ONCE),
            send_right_type);

  // This call takes ownership of |send_once_right|.
  kr = SendMachPort(send_once_right, port_to_insert, MACH_MSG_TYPE_COPY_SEND);
  if (kr != KERN_SUCCESS) {
    // TODO(erikchen): UMA metric.
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
    // TODO(erikchen): UMA metric.
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
