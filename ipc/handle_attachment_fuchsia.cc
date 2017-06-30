// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_attachment_fuchsia.h"

#include <magenta/syscalls.h>
#include <magenta/types.h>

namespace IPC {
namespace internal {

HandleAttachmentFuchsia::HandleAttachmentFuchsia(
    const mx_handle_t& handle,
    HandleFuchsia::Permissions permissions)
    : handle_(MX_HANDLE_INVALID),
      permissions_(HandleFuchsia::INVALID),
      owns_handle_(true) {
  mx_handle_t duplicated_handle;
  if (mx_handle_duplicate(handle, MX_RIGHT_SAME_RIGHTS, &duplicated_handle) ==
      MX_OK) {
    handle_ = duplicated_handle;
    permissions_ = permissions;
  }
}

HandleAttachmentFuchsia::HandleAttachmentFuchsia(const mx_handle_t& handle,
                                                 FromWire from_wire)
    : handle_(handle),
      permissions_(HandleFuchsia::INVALID),
      owns_handle_(true) {}

HandleAttachmentFuchsia::~HandleAttachmentFuchsia() {
  if (handle_ != MX_HANDLE_INVALID && owns_handle_)
    mx_handle_close(handle_);
}

MessageAttachment::Type HandleAttachmentFuchsia::GetType() const {
  return Type::FUCHSIA_HANDLE;
}

}  // namespace internal
}  // namespace IPC
