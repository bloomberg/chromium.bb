/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nacl_io/kernel_handle.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include "nacl_io/mount.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/osunistd.h"

// It is only legal to construct a handle while the kernel lock is held.
KernelHandle::KernelHandle()
    : mount_(NULL), node_(NULL), offs_(0) {}

KernelHandle::KernelHandle(const ScopedMount& mnt, const ScopedMountNode& node)
    : mount_(mnt), node_(node), offs_(0) {}

Error KernelHandle::Init(int open_mode) {
  if (open_mode & O_APPEND) {
    size_t node_size;
    Error error = node_->GetSize(&offs_);
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
      base = offs_;
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

  *out_offset = offs_ = new_offset;
  return 0;
}

