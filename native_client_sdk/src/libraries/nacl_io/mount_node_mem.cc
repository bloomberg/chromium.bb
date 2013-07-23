// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_node_mem.h"

#include <errno.h>
#include <string.h>

#include "nacl_io/osstat.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

#define BLOCK_SIZE (1 << 16)
#define BLOCK_MASK (BLOCK_SIZE - 1)

MountNodeMem::MountNodeMem(Mount* mount)
    : MountNode(mount), data_(NULL), capacity_(0) {
  stat_.st_mode |= S_IFREG;
}

MountNodeMem::~MountNodeMem() { free(data_); }

Error MountNodeMem::Read(size_t offs, void* buf, size_t count, int* out_bytes) {
  *out_bytes = 0;

  AUTO_LOCK(node_lock_);
  if (count == 0)
    return 0;

  size_t size = stat_.st_size;

  if (offs + count > size) {
    count = size - offs;
  }

  memcpy(buf, &data_[offs], count);
  *out_bytes = static_cast<int>(count);
  return 0;
}

Error MountNodeMem::Write(size_t offs,
                          const void* buf,
                          size_t count,
                          int* out_bytes) {
  *out_bytes = 0;
  AUTO_LOCK(node_lock_);

  if (count == 0)
    return 0;

  if (count + offs > stat_.st_size) {
    Error error = FTruncate(count + offs);
    if (error)
      return error;

    count = stat_.st_size - offs;
  }

  memcpy(&data_[offs], buf, count);
  *out_bytes = static_cast<int>(count);
  return 0;
}

Error MountNodeMem::FTruncate(off_t new_size) {
  size_t need = (new_size + BLOCK_MASK) & ~BLOCK_MASK;
  size_t old_size = stat_.st_size;

  // If the current capacity is correct, just adjust and return
  if (need == capacity_) {
    stat_.st_size = static_cast<off_t>(new_size);
    return 0;
  }

  // Attempt to realloc the block
  char* newdata = static_cast<char*>(realloc(data_, need));
  if (newdata != NULL) {
    // Zero out new space.
    if (new_size > old_size)
      memset(newdata + old_size, 0, need - old_size);

    data_ = newdata;
    capacity_ = need;
    stat_.st_size = static_cast<off_t>(new_size);
    return 0;
  }

  // If we failed, then adjust size according to what we keep
  if (new_size > capacity_)
    new_size = capacity_;

  // Update the size and return the new size
  stat_.st_size = static_cast<off_t>(new_size);
  return EIO;
}

}  // namespace nacl_io

