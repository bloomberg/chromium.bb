// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/kernel_handle.h"

#include <errno.h>
#include <pthread.h>

#include "nacl_io/mount.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/osunistd.h"

#include "sdk_util/auto_lock.h"

namespace nacl_io {

// It is only legal to construct a handle while the kernel lock is held.
KernelHandle::KernelHandle()
    : mount_(NULL), node_(NULL) {}

KernelHandle::KernelHandle(const ScopedMount& mnt, const ScopedMountNode& node)
    : mount_(mnt), node_(node) {}

KernelHandle::~KernelHandle() {
  // Force release order for cases where mount_ is not ref'd by mounting.
  node_.reset(NULL);
  mount_.reset(NULL);
}

// Returns the MountNodeSocket* if this node is a socket.
MountNodeSocket* KernelHandle::socket_node() {
  if (node_.get() && node_->IsaSock())
    return reinterpret_cast<MountNodeSocket*>(node_.get());
  return NULL;
}

Error KernelHandle::Init(int open_flags) {
  handle_data_.flags = open_flags;

  if (open_flags & O_APPEND) {
    Error error = node_->GetSize(&handle_data_.offs);
    if (error)
      return error;
  }

  return 0;
}

Error KernelHandle::Seek(off_t offset, int whence, off_t* out_offset) {
  // By default, don't move the offset.
  *out_offset = offset;
  size_t base;
  size_t node_size;

  AUTO_LOCK(handle_lock_);
  Error error = node_->GetSize(&node_size);
  if (error)
    return error;

  switch (whence) {
    default:
      return -1;
    case SEEK_SET:
      base = 0;
      break;
    case SEEK_CUR:
      base = handle_data_.offs;
      break;
    case SEEK_END:
      base = node_size;
      break;
  }

  if (base + offset < 0)
    return EINVAL;

  off_t new_offset = base + offset;

  // Seeking past the end of the file will zero out the space between the old
  // end and the new end.
  if (new_offset > node_size) {
    error = node_->FTruncate(new_offset);
    if (error)
      return EINVAL;
  }

  *out_offset = handle_data_.offs = new_offset;
  return 0;
}

Error KernelHandle::Read(void* buf, size_t nbytes, int* cnt) {
  AUTO_LOCK(handle_lock_);
  Error error = node_->Read(handle_data_, buf, nbytes, cnt);
  if (0 == error)
    handle_data_.offs += *cnt;
  return error;
}

Error KernelHandle::Write(const void* buf, size_t nbytes, int* cnt) {
  AUTO_LOCK(handle_lock_);
  Error error = node_->Write(handle_data_, buf, nbytes, cnt);
  if (0 == error)
    handle_data_.offs += *cnt;
  return error;
}

Error KernelHandle::GetDents(struct dirent* pdir, size_t nbytes, int* cnt) {
  AUTO_LOCK(handle_lock_);
  Error error = node_->GetDents(handle_data_.offs, pdir, nbytes, cnt);
  if (0 == error)
    handle_data_.offs += *cnt;
  return error;
}

Error KernelHandle::Fcntl(int request, char* arg, int* result) {
  switch (request) {
    case F_GETFL: {
      *result = handle_data_.flags;
      return 0;
    }
    case F_SETFL: {
      AUTO_LOCK(handle_lock_);
      int flags = reinterpret_cast<int>(arg);
      if (!(flags & O_APPEND) && (handle_data_.flags & O_APPEND)) {
        // Attempt to clear O_APPEND.
        return EPERM;
      }
      // Only certain flags are mutable
      const int mutable_flags = O_ASYNC | O_NONBLOCK;
      flags &= mutable_flags;
      handle_data_.flags &= ~mutable_flags;
      handle_data_.flags |= flags;
      return 0;
    }
  }
  return ENOSYS;
}

}  // namespace nacl_io
