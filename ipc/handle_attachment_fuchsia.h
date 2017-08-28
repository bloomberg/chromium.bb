// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_HANDLE_ATTACHMENT_FUCHSIA_H_
#define IPC_HANDLE_ATTACHMENT_FUCHSIA_H_

#include <stdint.h>

#include "base/fuchsia/scoped_mx_handle.h"
#include "ipc/handle_fuchsia.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_message_attachment.h"

namespace IPC {
namespace internal {

// This class represents a Fuchsia mx_handle_t attached to a Chrome IPC message.
class IPC_EXPORT HandleAttachmentFuchsia : public MessageAttachment {
 public:
  // This constructor makes a copy of |handle| and takes ownership of the
  // result. Should only be called by the sender of a Chrome IPC message.
  explicit HandleAttachmentFuchsia(const mx_handle_t& handle);

  // This constructor takes ownership of |handle|. Should only be called by the
  // receiver of a Chrome IPC message.
  explicit HandleAttachmentFuchsia(base::ScopedMxHandle handle);

  Type GetType() const override;

  mx_handle_t Take() { return handle_.release(); }

 private:
  ~HandleAttachmentFuchsia() override;

  base::ScopedMxHandle handle_;
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_HANDLE_ATTACHMENT_FUCHSIA_H_
