// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_attachment_win.h"

#include <windows.h>

#include "crypto/random.h"

namespace IPC {
namespace internal {

namespace {

// In order to prevent mutually untrusted processes from stealing resources from
// one another, the nonce must be secret. This generates a 128-bit,
// cryptographicaly-strong random number.
BrokerableAttachment::AttachmentId GenerateAttachementId() {
  BrokerableAttachment::AttachmentId result;
  crypto::RandBytes(result.nonce, BrokerableAttachment::kNonceSize);
  return result;
}

}  // namespace

HandleAttachmentWin::HandleAttachmentWin(const HANDLE& handle,
                                         HandleWin::Permissions permissions)
    : BrokerableAttachment(GenerateAttachementId(), true),
      handle_(handle),
      permissions_(permissions) {}

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
