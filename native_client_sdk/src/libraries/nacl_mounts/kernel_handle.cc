/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <pthread.h>

#include "nacl_mounts/kernel_handle.h"
#include "nacl_mounts/mount.h"
#include "nacl_mounts/mount_node.h"


// It is only legal to construct a handle while the kernel lock is held.
KernelHandle::KernelHandle(Mount* mnt, MountNode* node, int mode)
    : mount_(mnt),
      node_(node),
      mode_(mode),
      offs_(0) {
  if (mode & O_APPEND) offs_ = node->GetSize();
}

