/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <sys/stat.h>

#include "auto_lock.h"
#include "mount_node_mem.h"

#define BLOCK_SIZE (1 << 16)
#define BLOCK_MASK (BLOCK_SIZE - 1)

MountNodeMem::MountNodeMem(Mount *mount, int ino, int dev)
    : MountNode(mount, ino, dev),
      data_(NULL),
      capacity_(0) {
}

MountNodeMem::~MountNodeMem() {
  free(data_);
}

bool MountNodeMem::Init(int mode, short uid, short gid) {
  bool ok = MountNode::Init(mode, uid, gid);
  stat_.st_mode |= _S_IFREG;
  return ok;
}

int MountNodeMem::Read(size_t offs, void *buf, size_t count) {
  AutoLock lock(&lock_);
  if (count == 0) return 0;
  if (offs + count > GetSize()) {
    count = GetSize() - offs;
  }

  memcpy(buf, &data_[offs], count);
  return static_cast<int>(count);
}

int MountNodeMem::Write(size_t offs, const void *buf, size_t count) {
  AutoLock lock(&lock_);

  if (count == 0) return 0;

  if (count + offs > GetSize()) {
    Truncate(count + offs);
    count = GetSize() - offs;
  }

  memcpy(&data_[offs], buf, count);
  return static_cast<int>(count);
}

int MountNodeMem::Truncate(size_t size) {
  size_t need = (size + BLOCK_MASK) & ~BLOCK_MASK;

  // If the current capacity is correct, just adjust and return
  if (need == capacity_) {
    stat_.st_size = static_cast<off_t>(size);
    0;
  }

  // Attempt to realloc the block
  char *newdata = static_cast<char *>(realloc(data_, need));
  if (newdata != NULL) {
    data_ = newdata;
    capacity_ = need;
    stat_.st_size = static_cast<off_t>(size);
    return 0;
  }

  // If we failed, then adjust size according to what we keep
  if (size > capacity_) size = capacity_;

  // Update the size and return the new size
  stat_.st_size = static_cast<off_t>(size);
  errno = EIO;
  return -1;
}
