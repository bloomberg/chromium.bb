// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MOJO_IPC_MOJO_HANDLE_ATTACHMENT_H_
#define IPC_MOJO_IPC_MOJO_HANDLE_ATTACHMENT_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ipc/ipc_export.h"
#include "ipc/ipc_message_attachment.h"
#include "mojo/public/cpp/system/handle.h"

namespace IPC {

namespace internal {

// A MessageAttachment that holds a MojoHandle.
// * On the sending side, every Mojo handle is a MessagePipe. This is because
//   any platform files are wrapped by PlatformFileAttachment.
// * On the receiving side, the handle can be either MessagePipe or wrapped
//   platform file: All files, not only MessagePipes are wrapped as a
//   MojoHandle. The message deserializer should know which type of the object
//   the handle wraps.
class IPC_MOJO_EXPORT MojoHandleAttachment : public MessageAttachment {
 public:
  explicit MojoHandleAttachment(mojo::ScopedHandle handle);

  Type GetType() const override;

#if defined(OS_POSIX)
  // Returns wrapped file if it wraps a file, or
  // an invalid fd otherwise. The ownership of handle
  // is passed to the caller.
  base::PlatformFile TakePlatformFile() override;
#endif  // OS_POSIX

  // Returns the owning handle transferring the ownership.
  mojo::ScopedHandle TakeHandle();

 private:
  ~MojoHandleAttachment() override;
  mojo::ScopedHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(MojoHandleAttachment);
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_MOJO_IPC_MOJO_HANDLE_ATTACHMENT_H_
