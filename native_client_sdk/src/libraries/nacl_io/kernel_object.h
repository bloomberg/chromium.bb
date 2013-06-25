/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef LIBRARIES_NACL_IO_KERNEL_OBJECT_H_
#define LIBRARIES_NACL_IO_KERNEL_OBJECT_H_

#include <pthread.h>

#include <map>
#include <string>
#include <vector>

#include "nacl_io/error.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/mount.h"
#include "nacl_io/path.h"


// KernelObject provides basic functionality for threadsafe
// access to kernel objects such as file descriptors and
// file handles.  It also provides access to the CWD for
// path resolution.
class KernelObject {
 public:
  typedef std::vector<ScopedKernelHandle> HandleMap_t;
  typedef std::map<std::string, ScopedMount> MountMap_t;

  KernelObject();
  virtual ~KernelObject();

  // Find the mount for the given path, and acquires it.
  // Assumes |out_mount| and |out_path| are non-NULL.
  Error AcquireMountAndPath(const std::string& relpath,
                            ScopedMount* out_mount,
                            Path* out_path);

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

 protected:
  Path GetAbsPathLocked(const std::string& path);

  std::vector<int> free_fds_;
  std::string cwd_;

  HandleMap_t handle_map_;
  MountMap_t mounts_;

  // Kernel lock protects kernel wide resources such as the mount table...
  pthread_mutex_t kernel_lock_;

  // Process lock protects process wide resources such as CWD, file handles...
  pthread_mutex_t process_lock_;

  DISALLOW_COPY_AND_ASSIGN(KernelObject);
};

#endif  // LIBRARIES_NACL_IO_KERNEL_OBJECT_H_
