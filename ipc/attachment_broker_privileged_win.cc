// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/attachment_broker_privileged_win.h"

#include <windows.h>

#include "base/process/process.h"
#include "ipc/attachment_broker_messages.h"
#include "ipc/brokerable_attachment.h"
#include "ipc/handle_attachment_win.h"
#include "ipc/ipc_channel.h"

namespace IPC {

AttachmentBrokerPrivilegedWin::AttachmentBrokerPrivilegedWin() {}

AttachmentBrokerPrivilegedWin::~AttachmentBrokerPrivilegedWin() {}

bool AttachmentBrokerPrivilegedWin::SendAttachmentToProcess(
    const BrokerableAttachment* attachment,
    base::ProcessId destination_process) {
  switch (attachment->GetBrokerableType()) {
    case BrokerableAttachment::WIN_HANDLE:
      const internal::HandleAttachmentWin* handle_attachment =
          static_cast<const internal::HandleAttachmentWin*>(attachment);
      HandleWireFormat wire_format =
          handle_attachment->GetWireFormat(destination_process);
      HandleWireFormat new_wire_format =
          DuplicateWinHandle(wire_format, base::Process::Current().Pid());
      if (new_wire_format.handle == 0)
        return false;
      RouteDuplicatedHandle(new_wire_format);
      return true;
  }
  return false;
}

bool AttachmentBrokerPrivilegedWin::OnMessageReceived(const Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AttachmentBrokerPrivilegedWin, msg)
    IPC_MESSAGE_HANDLER(AttachmentBrokerMsg_DuplicateWinHandle,
                        OnDuplicateWinHandle)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AttachmentBrokerPrivilegedWin::RegisterCommunicationChannel(
    Channel* channel) {
  auto it = std::find(channels_.begin(), channels_.end(), channel);
  DCHECK(channels_.end() == it);
  channels_.push_back(channel);
}

void AttachmentBrokerPrivilegedWin::DeregisterCommunicationChannel(
    Channel* channel) {
  auto it = std::find(channels_.begin(), channels_.end(), channel);
  DCHECK(it != channels_.end());
  channels_.erase(it);
}

void AttachmentBrokerPrivilegedWin::OnDuplicateWinHandle(
    const HandleWireFormat& wire_format,
    base::ProcessId source_pid) {
  if (wire_format.destination_process == base::kNullProcessId)
    return;

  HandleWireFormat new_wire_format =
      DuplicateWinHandle(wire_format, source_pid);
  RouteDuplicatedHandle(new_wire_format);
}

void AttachmentBrokerPrivilegedWin::RouteDuplicatedHandle(
    const HandleWireFormat& wire_format) {
  // This process is the destination.
  if (wire_format.destination_process == base::Process::Current().Pid()) {
    scoped_refptr<BrokerableAttachment> attachment(
        new internal::HandleAttachmentWin(wire_format));
    HandleReceivedAttachment(attachment);
    return;
  }

  // Another process is the destination.
  base::ProcessId dest = wire_format.destination_process;
  auto it =
      std::find_if(channels_.begin(), channels_.end(),
                   [dest](Channel* c) { return c->GetPeerPID() == dest; });
  if (it == channels_.end()) {
    // Assuming that this message was not sent from a malicious process, the
    // channel endpoint that would have received this message will block
    // forever.
    LOG(ERROR) << "Failed to deliver brokerable attachment to process with id: "
               << dest;
    return;
  }

  (*it)->Send(new AttachmentBrokerMsg_WinHandleHasBeenDuplicated(wire_format));
}

AttachmentBrokerPrivilegedWin::HandleWireFormat
AttachmentBrokerPrivilegedWin::DuplicateWinHandle(
    const HandleWireFormat& wire_format,
    base::ProcessId source_pid) {
  HandleWireFormat new_wire_format;
  new_wire_format.destination_process = wire_format.destination_process;
  new_wire_format.attachment_id = wire_format.attachment_id;

  HANDLE original_handle = LongToHandle(wire_format.handle);

  base::Process source_process =
      base::Process::OpenWithExtraPrivileges(source_pid);
  base::Process dest_process =
      base::Process::OpenWithExtraPrivileges(wire_format.destination_process);
  if (source_process.Handle() && dest_process.Handle()) {
    HANDLE new_handle;
    DWORD result = ::DuplicateHandle(source_process.Handle(), original_handle,
                                     dest_process.Handle(), &new_handle, 0,
                                     FALSE, DUPLICATE_SAME_ACCESS);

    new_wire_format.handle = (result != 0) ? HandleToLong(new_handle) : 0;
  } else {
    new_wire_format.handle = 0;
  }
  return new_wire_format;
}

}  // namespace IPC
