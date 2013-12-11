// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_FUSE_MOUNT_FACTORY_H_
#define LIBRARIES_NACL_IO_FUSE_MOUNT_FACTORY_H_

#include "nacl_io/mount.h"
#include "nacl_io/mount_factory.h"

struct fuse_operations;

namespace nacl_io {

class FuseMountFactory : public MountFactory {
 public:
  explicit FuseMountFactory(fuse_operations* fuse_ops);
  virtual Error CreateMount(const MountInitArgs& args,
                            ScopedMount* out_mount);

 private:
  fuse_operations* fuse_ops_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_FUSE_MOUNT_FACTORY_H_
