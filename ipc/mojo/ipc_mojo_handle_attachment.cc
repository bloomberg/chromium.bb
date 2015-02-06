// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_mojo_handle_attachment.h"

#include "ipc/ipc_message_attachment_set.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"

namespace IPC {
namespace internal {

MojoHandleAttachment::MojoHandleAttachment(mojo::ScopedHandle handle)
    : handle_(handle.Pass()) {
}

MojoHandleAttachment::~MojoHandleAttachment() {
}

MessageAttachment::Type MojoHandleAttachment::GetType() const {
  return TYPE_MOJO_HANDLE;
}

#if defined(OS_POSIX)
base::PlatformFile MojoHandleAttachment::TakePlatformFile() {
  mojo::embedder::ScopedPlatformHandle platform_handle;
  MojoResult unwrap_result = mojo::embedder::PassWrappedPlatformHandle(
      handle_.release().value(), &platform_handle);
  if (unwrap_result != MOJO_RESULT_OK) {
    DLOG(ERROR) << "Pipe failed to covert handles. Closing: " << unwrap_result;
    return -1;
  }

  return platform_handle.release().fd;
}
#endif  // OS_POSIX

mojo::ScopedHandle MojoHandleAttachment::TakeHandle() {
  return handle_.Pass();
}

}  // namespace internal
}  // namespace IPC
