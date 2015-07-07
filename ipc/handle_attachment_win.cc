// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_attachment_win.h"

#include <windows.h>

namespace IPC {
namespace internal {

HandleAttachmentWin::HandleAttachmentWin(const HANDLE& handle)
    : handle_(handle) {
}

HandleAttachmentWin::~HandleAttachmentWin() {
}

HandleAttachmentWin::BrokerableType HandleAttachmentWin::GetBrokerableType()
    const {
  return WIN_HANDLE;
}

HandleAttachmentWin::WireFormat HandleAttachmentWin::GetWireFormat(
    const base::ProcessId& destination) const {
  WireFormat format;
  format.handle = HandleToLong(handle_);
  format.attachment_id = GetIdentifier();
  format.destination_process = destination;
  return format;
}

}  // namespace internal
}  // namespace IPC
