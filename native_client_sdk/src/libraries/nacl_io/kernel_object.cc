/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nacl_io/kernel_object.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "nacl_io/kernel_handle.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_node.h"

#include "sdk_util/auto_lock.h"
#include "sdk_util/ref_object.h"
#include "sdk_util/scoped_ref.h"

KernelObject::KernelObject() {
  pthread_mutex_init(&kernel_lock_, NULL);
  pthread_mutex_init(&process_lock_, NULL);
}

KernelObject::~KernelObject() {
  pthread_mutex_destroy(&process_lock_);
  pthread_mutex_destroy(&kernel_lock_);
}

// Uses longest prefix to find the mount for the give path, then
// acquires the mount and returns it with a relative path.
Error KernelObject::AcquireMountAndPath(const std::string& relpath,
                                        ScopedMount* out_mount,
                                        Path* out_path) {
  out_mount->reset(NULL);

  *out_path = Path();
  Path abs_path;
  {
    AutoLock lock(&process_lock_);
    abs_path = GetAbsPathLocked(relpath);
  }

  AutoLock lock(&kernel_lock_);

  // Find longest prefix
  size_t max = abs_path.Size();
  for (size_t len = 0; len < abs_path.Size(); len++) {
    MountMap_t::iterator it = mounts_.find(abs_path.Range(0, max - len));
    if (it != mounts_.end()) {
      out_path->Set("/");
      out_path->Append(abs_path.Range(max - len, max));

      *out_mount = it->second;
      return 0;
    }
  }

  return ENOTDIR;
}

Error KernelObject::AcquireHandle(int fd, ScopedKernelHandle* out_handle) {
  out_handle->reset(NULL);

  AutoLock lock(&process_lock_);
  if (fd < 0 || fd >= static_cast<int>(handle_map_.size()))
    return EBADF;

  *out_handle = handle_map_[fd];
  if (out_handle) return 0;

  return EBADF;
}

int KernelObject::AllocateFD(const ScopedKernelHandle& handle) {
  AutoLock lock(&process_lock_);
  int id;

  // If we can recycle and FD, use that first
  if (free_fds_.size()) {
    id = free_fds_.front();
    // Force lower numbered FD to be available first.
    std::pop_heap(free_fds_.begin(), free_fds_.end(), std::greater<int>());
    free_fds_.pop_back();
    handle_map_[id] = handle;
  } else {
    id = handle_map_.size();
    handle_map_.push_back(handle);
  }
  return id;
}

void KernelObject::FreeAndReassignFD(int fd, const ScopedKernelHandle& handle) {
  if (NULL == handle) {
    FreeFD(fd);
  } else {
    AutoLock lock(&process_lock_);

    // If the required FD is larger than the current set, grow the set
    if (fd >= handle_map_.size())
      handle_map_.resize(fd + 1, ScopedRef<KernelHandle>());

    handle_map_[fd] = handle;
  }
}

void KernelObject::FreeFD(int fd) {
  AutoLock lock(&process_lock_);

  handle_map_[fd].reset(NULL);
  free_fds_.push_back(fd);

  // Force lower numbered FD to be available first.
  std::push_heap(free_fds_.begin(), free_fds_.end(), std::greater<int>());
}

Path KernelObject::GetAbsPathLocked(const std::string& path) {
  // Generate absolute path
  Path abs_path(cwd_);

  if (path[0] == '/') {
    abs_path = path;
  } else {
    abs_path = cwd_;
    abs_path.Append(path);
  }

  return abs_path;
}

