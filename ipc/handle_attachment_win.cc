// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_attachment_win.h"

#include <windows.h>

namespace IPC {
namespace internal {

HandleAttachmentWin::HandleAttachmentWin(const HANDLE& handle,
                                         HandleWin::Permissions permissions)
    : handle_(handle), permissions_(permissions) {}

HandleAttachmentWin::HandleAttachmentWin(const WireFormat& wire_format)
    : BrokerableAttachment(wire_format.attachment_id, false),
      handle_(LongToHandle(wire_format.handle)),
      permissions_(wire_format.permissions) {}

HandleAttachmentWin::HandleAttachmentWin(
    const BrokerableAttachment::AttachmentId& id)
    : BrokerableAttachment(id, true),
      handle_(INVALID_HANDLE_VALUE),
      permissions_(HandleWin::INVALID) {}

HandleAttachmentWin::~HandleAttachmentWin() {
}

HandleAttachmentWin::BrokerableType HandleAttachmentWin::GetBrokerableType()
    const {
  return WIN_HANDLE;
}

void HandleAttachmentWin::PopulateWithAttachment(
    const BrokerableAttachment* attachment) {
  DCHECK(NeedsBrokering());
  DCHECK(!attachment->NeedsBrokering());
  DCHECK(GetIdentifier() == attachment->GetIdentifier());
  DCHECK_EQ(GetBrokerableType(), attachment->GetBrokerableType());

  const HandleAttachmentWin* handle_attachment =
      static_cast<const HandleAttachmentWin*>(attachment);
  handle_ = handle_attachment->handle_;
  SetNeedsBrokering(false);
}

HandleAttachmentWin::WireFormat HandleAttachmentWin::GetWireFormat(
    const base::ProcessId& destination) const {
  WireFormat format;
  format.handle = HandleToLong(handle_);
  format.attachment_id = GetIdentifier();
  format.destination_process = destination;
  format.permissions = permissions_;
  return format;
}

}  // namespace internal
}  // namespace IPC
