// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "nacl_io/memfs/mem_fs_node.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <algorithm>

#include "nacl_io/kernel_handle.h"
#include "nacl_io/osinttypes.h"
#include "nacl_io/osstat.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

namespace {

// The maximum size to reserve in addition to the requested size. Resize() will
// allocate twice as much as requested, up to this value.
const size_t kMaxResizeIncrement = 16 * 1024 * 1024;

}  // namespace

MemFsNode::MemFsNode(Filesystem* filesystem)
  : Node(filesystem),
    data_(NULL),
    data_capacity_(0) {
  SetType(S_IFREG);
}

MemFsNode::~MemFsNode() {
}

Error MemFsNode::Read(const HandleAttr& attr,
                      void* buf,
                      size_t count,
                      int* out_bytes) {
  *out_bytes = 0;

  AUTO_LOCK(node_lock_);
  if (count == 0)
    return 0;

  size_t size = stat_.st_size;

  if (attr.offs + count > size) {
    count = size - attr.offs;
  }

  memcpy(buf, data_ + attr.offs, count);
  *out_bytes = static_cast<int>(count);
  return 0;
}

Error MemFsNode::Write(const HandleAttr& attr,
                       const void* buf,
                       size_t count,
                       int* out_bytes) {
  *out_bytes = 0;

  if (count == 0)
    return 0;

  AUTO_LOCK(node_lock_);
  off_t new_size = attr.offs + count;
  if (new_size > stat_.st_size) {
    Error error = Resize(new_size);
    if (error) {
      LOG_ERROR("memfs: resize (%" PRIoff ") failed: %s", new_size,
          strerror(error));
      return error;
    }
  }

  memcpy(data_ + attr.offs, buf, count);
  *out_bytes = static_cast<int>(count);
  return 0;
}

Error MemFsNode::FTruncate(off_t new_size) {
  AUTO_LOCK(node_lock_);
  return Resize(new_size);
}

Error MemFsNode::Resize(off_t new_length) {
  if (new_length < 0)
    return EINVAL;
  size_t new_size = static_cast<size_t>(new_length);

  if (new_size > data_capacity_) {
    // While the node size is small, grow exponentially. When it starts to get
    // larger, grow linearly.
    size_t extra = std::min(new_size, kMaxResizeIncrement);
    data_capacity_ = new_size + extra;
  } else {
    data_capacity_ = new_size;
  }

  data_ = (char*)realloc(data_, data_capacity_);
  if (data_capacity_ != 0) {
    assert(data_ != NULL);
    if (data_ == NULL)
      return ENOMEM;
    if (new_length > stat_.st_size)
      memset(data_ + stat_.st_size, 0, new_length - stat_.st_size);
  }

  stat_.st_size = new_length;
  return 0;
}

}  // namespace nacl_io
