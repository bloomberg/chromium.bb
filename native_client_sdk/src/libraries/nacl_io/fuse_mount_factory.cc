// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/fuse_mount_factory.h"

#include "nacl_io/mount_fuse.h"

namespace nacl_io {

FuseMountFactory::FuseMountFactory(fuse_operations* fuse_ops)
    : fuse_ops_(fuse_ops) {}

Error FuseMountFactory::CreateMount(const MountInitArgs& args,
                                    ScopedMount* out_mount) {
  MountInitArgs args_copy(args);
  args_copy.fuse_ops = fuse_ops_;

  sdk_util::ScopedRef<MountFuse> mnt(new MountFuse());
  Error error = mnt->Init(args_copy);
  if (error)
    return error;

  *out_mount = mnt;
  return 0;
}

}  // namespace nacl_io
