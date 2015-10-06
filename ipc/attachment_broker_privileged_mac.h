// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_ATTACHMENT_BROKER_PRIVILEGED_MAC_H_
#define IPC_ATTACHMENT_BROKER_PRIVILEGED_MAC_H_

#include <mach/mach.h>

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/process/port_provider_mac.h"
#include "ipc/attachment_broker_privileged.h"
#include "ipc/ipc_export.h"
#include "ipc/mach_port_attachment_mac.h"

namespace IPC {

// This class is a concrete subclass of AttachmentBrokerPrivileged for the
// OSX platform.
class IPC_EXPORT AttachmentBrokerPrivilegedMac
    : public AttachmentBrokerPrivileged {
 public:
  AttachmentBrokerPrivilegedMac();
  ~AttachmentBrokerPrivilegedMac() override;

  // The port provider must live as long as the AttachmentBrokerPrivilegedMac. A
  // port provider must be set before any attachment brokering occurs.
  void SetPortProvider(base::PortProvider* port_provider);

  // IPC::AttachmentBroker overrides.
  bool SendAttachmentToProcess(const BrokerableAttachment* attachment,
                               base::ProcessId destination_process) override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const Message& message) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AttachmentBrokerPrivilegedMacMultiProcessTest,
                           InsertRight);
  FRIEND_TEST_ALL_PREFIXES(AttachmentBrokerPrivilegedMacMultiProcessTest,
                           InsertSameRightTwice);
  FRIEND_TEST_ALL_PREFIXES(AttachmentBrokerPrivilegedMacMultiProcessTest,
                           InsertTwoRights);
  using MachPortWireFormat = internal::MachPortAttachmentMac::WireFormat;
  // IPC message handlers.
  void OnDuplicateMachPort(const Message& message);

  // Duplicates the Mach port referenced from |wire_format| from
  // |source_process| into |wire_format|'s destination process.
  MachPortWireFormat DuplicateMachPort(const MachPortWireFormat& wire_format,
                                       base::ProcessId source_process);

  // Returns the name of the inserted right of a port, which contains a queued
  // message with |port_to_insert|. Returns |MACH_PORT_NULL| on failure.
  // |task_port| must be for a different task, and |port_to_insert| is a port
  // right in the current task.
  mach_port_name_t InsertIndirectMachPort(mach_port_t task_port,
                                          mach_port_t port_to_insert);

  // Acquire a send right to a named right in |pid|.
  // Returns MACH_PORT_NULL on error.
  base::mac::ScopedMachSendRight AcquireSendRight(base::ProcessId pid,
                                                  mach_port_name_t named_right);

  // Extracts a copy of the send right to |named_right| from |task_port|.
  // Returns MACH_PORT_NULL on error.
  base::mac::ScopedMachSendRight ExtractNamedRight(
      mach_port_t task_port,
      mach_port_name_t named_right);

  // Copies an existing |wire_format|, but substitutes in a different mach port.
  MachPortWireFormat CopyWireFormat(const MachPortWireFormat& wire_format,
                                    uint32_t mach_port);

  // If the mach port's destination is this process, queue it and notify the
  // observers. Otherwise, send it in an IPC to its destination.
  void RouteDuplicatedMachPort(const MachPortWireFormat& wire_format);

  base::PortProvider* port_provider_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerPrivilegedMac);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_PRIVILEGED_MAC_H_
