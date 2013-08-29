// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_node_mem.h"

#include <errno.h>
#include <string.h>

#include <algorithm>

#include "nacl_io/osstat.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

namespace {

// The maximum size to reserve in addition to the requested size. Resize() will
// allocate twice as much as requested, up to this value.
const size_t kMaxResizeIncrement = 16 * 1024 * 1024;

}  // namespace

MountNodeMem::MountNodeMem(Mount* mount)
    : MountNode(mount) {
  stat_.st_mode |= S_IFREG;
}

MountNodeMem::~MountNodeMem() {}

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
    Resize(count + offs);
    count = stat_.st_size - offs;
  }

  memcpy(&data_[offs], buf, count);
  *out_bytes = static_cast<int>(count);
  return 0;
}

Error MountNodeMem::FTruncate(off_t new_size) {
  AUTO_LOCK(node_lock_);
  Resize(new_size);
  return 0;
}

void MountNodeMem::Resize(off_t new_size) {
  if (new_size > data_.capacity()) {
    // While the node size is small, grow exponentially. When it starts to get
    // larger, grow linearly.
    size_t extra = std::min<size_t>(new_size, kMaxResizeIncrement);
    data_.reserve(new_size + extra);
  } else if (new_size < stat_.st_size) {
    // Shrink to fit. std::vector usually doesn't reduce allocation size, so
    // use the swap trick.
    std::vector<char>(data_).swap(data_);
  }
  data_.resize(new_size);
  stat_.st_size = new_size;
}

}  // namespace nacl_io
