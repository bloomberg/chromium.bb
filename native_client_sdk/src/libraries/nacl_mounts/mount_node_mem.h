/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_NACL_MOUNTS_MOUNT_NODE_MEM_H_
#define LIBRARIES_NACL_MOUNTS_MOUNT_NODE_MEM_H_

#include "nacl_mounts/mount_node.h"

class MountNodeMem : public MountNode {
 public:
  MountNodeMem(Mount* mount, int ino, int dev);

protected:
  virtual ~MountNodeMem();
  virtual bool Init(int mode, short uid, short gid);

public:
  // Normal read/write operations on a file
  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Write(size_t offs, const void* buf, size_t count);
  virtual int Truncate(size_t size);

 private:
  char* data_;
  size_t capacity_;
  friend class MountMem;
};

#endif  // LIBRARIES_NACL_MOUNTS_MOUNT_NODE_MEM_H_

