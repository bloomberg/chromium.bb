// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_KERNEL_HANDLE_H_
#define LIBRARIES_NACL_IO_KERNEL_HANDLE_H_

#include <pthread.h>

#include "nacl_io/error.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/ostypes.h"

#include "sdk_util/macros.h"
#include "sdk_util/ref_object.h"
#include "sdk_util/scoped_ref.h"
#include "sdk_util/simple_lock.h"

namespace nacl_io {

// KernelHandle provides a reference counted container for the open
// file information, such as it's mount, node, access type and offset.
// KernelHandle can only be referenced when the KernelProxy lock is held.
class KernelHandle : public sdk_util::RefObject {
 public:
  KernelHandle();
  KernelHandle(const ScopedMount& mnt, const ScopedMountNode& node);
  ~KernelHandle();

  Error Init(int open_flags);

  // Assumes |out_offset| is non-NULL.
  Error Seek(off_t offset, int whence, off_t* out_offset);

  // Dispatches Read, Write, GetDents to atomically update offs_.
  Error Read(void* buf, size_t nbytes, int* bytes_read);
  Error Write(const void* buf, size_t nbytes, int* bytes_written);
  Error GetDents(struct dirent* pdir, size_t count, int* bytes_written);

  const ScopedMountNode& node() { return node_; }
  const ScopedMount& mount() { return mount_; }

private:
  ScopedMount mount_;
  ScopedMountNode node_;
  sdk_util::SimpleLock offs_lock_;
  size_t offs_;

  friend class KernelProxy;
  DISALLOW_COPY_AND_ASSIGN(KernelHandle);
};

typedef sdk_util::ScopedRef<KernelHandle> ScopedKernelHandle;

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_KERNEL_HANDLE_H_
