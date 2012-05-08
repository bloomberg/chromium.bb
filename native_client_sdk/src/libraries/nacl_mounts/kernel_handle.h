/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_NACL_MOUNTS_KERNEL_HANDLE_H_
#define LIBRARIES_NACL_MOUNTS_KERNEL_HANDLE_H_

#include <pthread.h>

#include "utils/macros.h"
#include "utils/ref_object.h"

class Mount;
class MountNode;

// KernelHandle provides a reference counted container for the open
// file information, such as it's mount, node, access type and offset.
// KernelHandle can only be referenced when the KernelProxy lock is held.
class KernelHandle : public RefObject {
 public:
  KernelHandle(Mount* mnt, MountNode* node, int oflags);

  Mount* mount_;
  MountNode* node_;
  int mode_;
  size_t offs_;

 private:
  // May only be called by the KernelProxy when the Kernel's
  // lock is held.
  friend class KernelObject;
  friend class KernelProxy;
  void Acquire() { RefObject::Acquire(); }
  bool Release() { return RefObject::Release(); }
  DISALLOW_COPY_AND_ASSIGN(KernelHandle);
};

#endif  // LIBRARIES_NACL_MOUNTS_KERNEL_HANDLE_H_
