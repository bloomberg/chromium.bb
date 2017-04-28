// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_handle.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace base {

SharedMemoryHandle::SharedMemoryHandle() = default;

SharedMemoryHandle::SharedMemoryHandle(
    const base::FileDescriptor& file_descriptor)
    : file_descriptor_(file_descriptor) {}

// static
SharedMemoryHandle SharedMemoryHandle::ImportHandle(int fd) {
  SharedMemoryHandle handle;
  handle.file_descriptor_.fd = fd;
  handle.file_descriptor_.auto_close = false;
  return handle;
}

int SharedMemoryHandle::GetHandle() const {
  return file_descriptor_.fd;
}

void SharedMemoryHandle::SetHandle(int handle) {
  file_descriptor_.fd = handle;
}

bool SharedMemoryHandle::IsValid() const {
  return file_descriptor_.fd >= 0;
}

void SharedMemoryHandle::Close() const {
  if (IGNORE_EINTR(close(file_descriptor_.fd)) < 0)
    PLOG(ERROR) << "close";
}

int SharedMemoryHandle::Release() {
  int old_fd = file_descriptor_.fd;
  file_descriptor_.fd = -1;
  return old_fd;
}

SharedMemoryHandle SharedMemoryHandle::Duplicate() const {
  if (!IsValid())
    return SharedMemoryHandle();

  int duped_handle = HANDLE_EINTR(dup(file_descriptor_.fd));
  if (duped_handle < 0)
    return SharedMemoryHandle();
  return SharedMemoryHandle(FileDescriptor(duped_handle, true));
}

void SharedMemoryHandle::SetOwnershipPassesToIPC(bool ownership_passes) {
  file_descriptor_.auto_close = ownership_passes;
}

bool SharedMemoryHandle::OwnershipPassesToIPC() const {
  return file_descriptor_.auto_close;
}

}  // namespace base
