// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mach_port_attachment_mac.h"

namespace IPC {
namespace internal {

MachPortAttachmentMac::MachPortAttachmentMac(mach_port_t mach_port)
    : mach_port_(mach_port) {}

MachPortAttachmentMac::MachPortAttachmentMac(const WireFormat& wire_format)
    : BrokerableAttachment(wire_format.attachment_id),
      mach_port_(static_cast<mach_port_t>(wire_format.mach_port)) {}

MachPortAttachmentMac::MachPortAttachmentMac(
    const BrokerableAttachment::AttachmentId& id)
    : BrokerableAttachment(id), mach_port_(MACH_PORT_NULL) {}

MachPortAttachmentMac::~MachPortAttachmentMac() {}

MachPortAttachmentMac::BrokerableType MachPortAttachmentMac::GetBrokerableType()
    const {
  return MACH_PORT;
}

MachPortAttachmentMac::WireFormat MachPortAttachmentMac::GetWireFormat(
    const base::ProcessId& destination) const {
  return WireFormat(static_cast<uint32_t>(mach_port_), destination,
                    GetIdentifier());
}

}  // namespace internal
}  // namespace IPC
