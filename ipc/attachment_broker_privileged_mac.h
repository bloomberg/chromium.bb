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
//
// An example of the typical process by which a Mach port gets brokered.
// Definitions:
//   1. Let there be three processes P1, U2, U3. P1 is privileged.
//   2. U2 wants to send a Mach port M2 to U3. If this port is inserted into P1,
//   it will be called M1. If it is inserted into U3, it will be called M3.
//   3. name() returns a serializable representation of a Mach port that can be
//   passed over chrome IPC.
//   4. pid() returns the process id of a process.
//
// Process:
//   1. U2 sends a AttachmentBrokerMsg_DuplicateMachPort message to P1. The
//   message contains name(M2), and pid(U3).
//   2. P1 extracts M2 into its own namespace, making M1.
//   3. P1 makes a new Mach port R in U3.
//   4. P1 sends a mach_msg with M1 to R.
//   5. P1 sends name(R) to U3.
//   6. U3 retrieves the queued message from R. The kernel automatically
//   translates M1 into the namespace of U3, making M3.
//
// The logic of this class is a little bit more complex becauese any or all of
// P1, U2 and U3 may be the same, and depending on the exact configuration,
// the creation of R may not be necessary.
//
// For the rest of this file, and the corresponding implementation file, R will
// be called the "intermediate Mach port" and M3 the "final Mach port".
class IPC_EXPORT AttachmentBrokerPrivilegedMac
    : public AttachmentBrokerPrivileged {
 public:
  AttachmentBrokerPrivilegedMac();
  ~AttachmentBrokerPrivilegedMac() override;

  // IPC::AttachmentBroker overrides.
  bool SendAttachmentToProcess(BrokerableAttachment* attachment,
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

  // |pid| must be another process.
  // |port_to_insert| must be a send right in the current task's name space.
  // Creates an intermediate Mach port in |pid| and sends |port_to_insert| as a
  // mach_msg to the intermediate Mach port.
  // On success, returns the name of the intermediate Mach port.
  // On failure, returns |MACH_PORT_NULL|.
  // This method takes ownership of |port_to_insert|. On success, ownership is
  // passed to the intermediate Mach port.
  mach_port_name_t CreateIntermediateMachPort(
      base::ProcessId pid,
      base::mac::ScopedMachSendRight port_to_insert);

  // Same as the above method, where |task_port| is the task port of |pid|.
  mach_port_name_t CreateIntermediateMachPort(
      mach_port_t task_port,
      base::mac::ScopedMachSendRight port_to_insert);

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

  // |wire_format.destination_process| must be this process.
  // |wire_format.mach_port| must be the final Mach port.
  // Consumes a reference to |wire_format.mach_port|, as ownership is implicitly
  // passed to the consumer of the Chrome IPC message.
  // Makes an attachment, queues it, and notifies the observers.
  void RouteWireFormatToSelf(const MachPortWireFormat& wire_format);

  // |wire_format.destination_process| must be another process.
  // |wire_format.mach_port| must be the intermediate Mach port.
  // Ownership of |wire_format.mach_port| is implicitly passed to the process
  // that receives the Chrome IPC message.
  void RouteWireFormatToAnother(const MachPortWireFormat& wire_format);

  DISALLOW_COPY_AND_ASSIGN(AttachmentBrokerPrivilegedMac);
};

}  // namespace IPC

#endif  // IPC_ATTACHMENT_BROKER_PRIVILEGED_MAC_H_
