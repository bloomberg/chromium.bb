/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_NACL_MOUNTS_KERNEL_OBJECT_H_
#define LIBRARIES_NACL_MOUNTS_KERNEL_OBJECT_H_

#include <pthread.h>

#include <map>
#include <string>
#include <vector>

#include "nacl_mounts/path.h"

class KernelHandle;
class Mount;

// KernelObject provides basic functionality for threadsafe
// acces to kernel objects such as file descriptors and
// file handles.  It also provides access to the CWD for
// path resolution.
class KernelObject {
 public:
  typedef std::vector<KernelHandle*> HandleMap_t;
  typedef std::map<std::string, Mount*> MountMap_t;

  KernelObject();
  virtual ~KernelObject();

  // Find the mount for the given path, and acquires it
  Mount* AcquireMountAndPath(const std::string& relpath, Path *pobj);
  void ReleaseMount(Mount* mnt);

  // Convert from FD to KernelHandle, and aquire the handle.
  KernelHandle* AcquireHandle(int fd);
  void ReleaseHandle(KernelHandle* handle);

  // Allocate a new fd and assign the handle to it, while
  // ref counting the handle and associated mount.
  int AllocateFD(KernelHandle* handle);
  void FreeFD(int fd);

 protected:
  Path GetAbsPathLocked(const std::string& path);

  std::vector<int> free_fds_;
  std::string cwd_;

  HandleMap_t handle_map_;
  MountMap_t mounts_;
  pthread_mutex_t lock_;

  DISALLOW_COPY_AND_ASSIGN(KernelObject);
};

#endif  // LIBRARIES_NACL_MOUNTS_KERNEL_OBJECT_H_

