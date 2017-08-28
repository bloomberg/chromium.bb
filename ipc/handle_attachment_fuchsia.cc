// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_attachment_fuchsia.h"

#include <magenta/syscalls.h>
#include <magenta/types.h>

namespace IPC {
namespace internal {

HandleAttachmentFuchsia::HandleAttachmentFuchsia(const mx_handle_t& handle) {
  mx_status_t result =
      mx_handle_duplicate(handle, MX_RIGHT_SAME_RIGHTS, handle_.receive());
  DLOG_IF(ERROR, result != MX_OK)
      << "mx_handle_duplicate: " << mx_status_get_string(result);
}

HandleAttachmentFuchsia::HandleAttachmentFuchsia(base::ScopedMxHandle handle)
    : handle_(std::move(handle)) {}

HandleAttachmentFuchsia::~HandleAttachmentFuchsia() {}

MessageAttachment::Type HandleAttachmentFuchsia::GetType() const {
  return Type::FUCHSIA_HANDLE;
}

}  // namespace internal
}  // namespace IPC
