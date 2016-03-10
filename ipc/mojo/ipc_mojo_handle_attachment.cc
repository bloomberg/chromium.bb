// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_mojo_handle_attachment.h"

#include <utility>

#include "build/build_config.h"
#include "ipc/ipc_message_attachment_set.h"
#include "mojo/edk/embedder/embedder.h"

namespace IPC {
namespace internal {

MojoHandleAttachment::MojoHandleAttachment(mojo::ScopedHandle handle)
    : handle_(std::move(handle)) {}

MojoHandleAttachment::~MojoHandleAttachment() {
}

MessageAttachment::Type MojoHandleAttachment::GetType() const {
  return TYPE_MOJO_HANDLE;
}

#if defined(OS_POSIX)
base::PlatformFile MojoHandleAttachment::TakePlatformFile() {
  mojo::edk::ScopedPlatformHandle platform_handle;
  MojoResult unwrap_result = mojo::edk::PassWrappedPlatformHandle(
      handle_.get().value(), &platform_handle);
  handle_.reset();
  if (unwrap_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Pipe failed to covert handles. Closing: " << unwrap_result;
    return -1;
  }

  return platform_handle.release().handle;
}
#endif  // OS_POSIX

mojo::ScopedHandle MojoHandleAttachment::TakeHandle() {
  return std::move(handle_);
}

}  // namespace internal
}  // namespace IPC
