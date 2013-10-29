// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_KERNEL_OBJECT_H_
#define LIBRARIES_NACL_IO_KERNEL_OBJECT_H_

#include <pthread.h>

#include <map>
#include <string>
#include <vector>

#include "nacl_io/error.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/path.h"

#include "sdk_util/macros.h"
#include "sdk_util/simple_lock.h"

namespace nacl_io {

// KernelObject provides basic functionality for threadsafe access to kernel
// objects such as the CWD, mount points, file descriptors and file handles.
// All Kernel locks are private to ensure the lock order.
//
// All calls are assumed to be a relative path.
class KernelObject {
 public:
  struct Descriptor_t {
    Descriptor_t() : flags(0) {}
    explicit Descriptor_t(const ScopedKernelHandle& h) : handle(h), flags(0) {}

    ScopedKernelHandle handle;
    int flags;
  };
  typedef std::vector<Descriptor_t> HandleMap_t;
  typedef std::map<std::string, ScopedMount> MountMap_t;

  KernelObject();
  virtual ~KernelObject();

  // Attach the given Mount object at the specified path.
  Error AttachMountAtPath(const ScopedMount& mnt, const std::string& path);

  // Unmap the Mount object from the specified path and release it.
  Error DetachMountAtPath(const std::string& path);

  // Find the mount for the given path, and acquires it and return a
  // path relative to the mount.
  // Assumes |out_mount| and |rel_path| are non-NULL.
  Error AcquireMountAndRelPath(const std::string& path,
                               ScopedMount* out_mount,
                               Path* rel_path);

  // Find the mount and node for the given path, acquiring/creating it as
  // specified by the |oflags|.
  // Assumes |out_mount| and |out_node| are non-NULL.
  Error AcquireMountAndNode(const std::string& path,
                            int oflags,
                            ScopedMount* out_mount,
                            ScopedMountNode* out_node);

  // Get FD-specific flags (currently only FD_CLOEXEC is supported).
  Error GetFDFlags(int fd, int* out_flags);
  // Set FD-specific flags (currently only FD_CLOEXEC is supported).
  Error SetFDFlags(int fd, int flags);

  // Convert from FD to KernelHandle, and acquire the handle.
  // Assumes |out_handle| is non-NULL.
  Error AcquireHandle(int fd, ScopedKernelHandle* out_handle);

  // Allocate a new fd and assign the handle to it, while
  // ref counting the handle and associated mount.
  // Assumes |handle| is non-NULL;
  int AllocateFD(const ScopedKernelHandle& handle);

  // Assumes |handle| is non-NULL;
  void FreeAndReassignFD(int fd, const ScopedKernelHandle& handle);
  void FreeFD(int fd);

  // Returns or sets CWD
  std::string GetCWD();
  Error SetCWD(const std::string& path);

  // Returns parts of the absolute path for the given relative path
  Path GetAbsParts(const std::string& path);

private:
  std::string cwd_;
  std::vector<int> free_fds_;
  HandleMap_t handle_map_;
  MountMap_t mounts_;

  // Lock to protect free_fds_ and handle_map_.
  sdk_util::SimpleLock handle_lock_;

  // Lock to protect handle_map_.
  sdk_util::SimpleLock mount_lock_;

  // Lock to protect cwd_.
  sdk_util::SimpleLock cwd_lock_;

  DISALLOW_COPY_AND_ASSIGN(KernelObject);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_KERNEL_OBJECT_H_
