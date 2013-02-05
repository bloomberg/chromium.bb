/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nacl_io/kernel_handle.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#ifndef WIN32
// Needed for SEEK_SET/SEEK_CUR/SEEK_END.
#include <unistd.h>
#endif

#include "nacl_io/mount.h"
#include "nacl_io/mount_node.h"

// It is only legal to construct a handle while the kernel lock is held.
KernelHandle::KernelHandle(Mount* mnt, MountNode* node, int mode)
    : mount_(mnt),
      node_(node),
      mode_(mode),
      offs_(0) {
  if (mode & O_APPEND) offs_ = node->GetSize();
}

off_t KernelHandle::Seek(off_t offset, int whence) {
  size_t base;
  size_t node_size = node_->GetSize();

  switch (whence) {
    default: return -1;
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = offs_; break;
    case SEEK_END: base = node_size; break;
  }

  if (base + offset < 0) {
    errno = EINVAL;
    return -1;
  }

  offs_ = base + offset;

  // Seeking past the end of the file will zero out the space between the old
  // end and the new end.
  if (offs_ > node_size) {
    if (node_->Truncate(offs_) < 0) {
      errno = EINVAL;
      return -1;
    }
  }

  return offs_;
}
