// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_NODE_MEM_H_
#define LIBRARIES_NACL_IO_MOUNT_NODE_MEM_H_

#include "nacl_io/mount_node.h"

namespace nacl_io {

class MountNodeMem : public MountNode {
 public:
  explicit MountNodeMem(Mount* mount);

 protected:
  virtual ~MountNodeMem();

 public:
  // Normal read/write operations on a file
  virtual Error Read(size_t offs, void* buf, size_t count, int* out_bytes);
  virtual Error Write(size_t offs,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
  virtual Error FTruncate(off_t size);

 private:
  char* data_;
  size_t capacity_;
  friend class MountMem;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_NODE_MEM_H_
