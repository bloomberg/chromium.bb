// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_MOUNT_FACTORY_H_
#define LIBRARIES_NACL_IO_MOUNT_FACTORY_H_

#include <errno.h>

#include "nacl_io/error.h"
#include "sdk_util/scoped_ref.h"

namespace nacl_io {

class PepperInterface;
class Mount;
struct MountInitArgs;

class MountFactory {
 public:
  virtual ~MountFactory() {}
  virtual Error CreateMount(const MountInitArgs& args,
                            sdk_util::ScopedRef<Mount>* out_mount) = 0;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_MOUNT_FACTORY_H_

