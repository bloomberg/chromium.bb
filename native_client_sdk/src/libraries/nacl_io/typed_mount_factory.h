// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_TYPED_MOUNT_FACTORY_H_
#define LIBRARIES_NACL_IO_TYPED_MOUNT_FACTORY_H_

#include "nacl_io/mount.h"
#include "nacl_io/mount_factory.h"

namespace nacl_io {

template <typename T>
class TypedMountFactory : public MountFactory {
 public:
  virtual Error CreateMount(int dev,
                            StringMap_t& args,
                            PepperInterface* ppapi,
                            ScopedMount* out_mount) {
    sdk_util::ScopedRef<T> mnt(new T());
    Error error = mnt->Init(dev, args, ppapi);
    if (error)
      return error;

    *out_mount = mnt;
    return 0;
  }
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_TYPED_MOUNT_FACTORY_H_
