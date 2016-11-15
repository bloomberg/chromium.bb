// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_HANDLE_ATTACHMENT_WIN_H_
#define IPC_HANDLE_ATTACHMENT_WIN_H_

#include <stdint.h>

#include "base/process/process_handle.h"
#include "ipc/handle_win.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_message_attachment.h"

namespace IPC {
namespace internal {

// This class represents a Windows HANDLE attached to a Chrome IPC message.
class IPC_EXPORT HandleAttachmentWin : public MessageAttachment {
 public:
  // This constructor makes a copy of |handle| and takes ownership of the
  // result. Should only be called by the sender of a Chrome IPC message.
  HandleAttachmentWin(const HANDLE& handle, HandleWin::Permissions permissions);

  enum FromWire {
    FROM_WIRE,
  };
  // This constructor takes ownership of |handle|. Should only be called by the
  // receiver of a Chrome IPC message.
  HandleAttachmentWin(const HANDLE& handle, FromWire from_wire);

  Type GetType() const override;

  HANDLE get_handle() const { return handle_; }

  // The caller of this method has taken ownership of |handle_|.
  void reset_handle_ownership() {
    owns_handle_ = false;
    handle_ = INVALID_HANDLE_VALUE;
  }

 private:
  ~HandleAttachmentWin() override;
  HANDLE handle_;
  HandleWin::Permissions permissions_;

  // In the sender process, the attachment owns the HANDLE of a newly created
  // message. The attachment broker will eventually take ownership, and set
  // this member to |false|.
  // In the destination process, the attachment owns the Mach port until a call
  // to ParamTraits<HandleWin>::Read() takes ownership.
  bool owns_handle_;
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_HANDLE_ATTACHMENT_WIN_H_
