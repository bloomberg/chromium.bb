/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "nacl_mounts/kernel_handle.h"
#include "nacl_mounts/kernel_object.h"
#include "nacl_mounts/mount.h"
#include "nacl_mounts/mount_node.h"
#include "utils/auto_lock.h"

KernelObject::KernelObject() {
  pthread_mutex_init(&lock_, NULL);
}

KernelObject::~KernelObject() {
  pthread_mutex_destroy(&lock_);
}

// Uses longest prefix to find the mount for the give path, then
// acquires the mount and returns it with a relative path.
Mount* KernelObject::AcquireMountAndPath(const std::string& relpath,
                                         Path* out_path) {
  AutoLock lock(&lock_);
  Mount* mount = NULL;
  MountNode* node = NULL;

  Path abs_path = GetAbsPathLocked(relpath);

  // Find longest prefix
  size_t max = abs_path.Size();
  for (size_t len = 0; len < abs_path.Size(); len++) {
    MountMap_t::iterator it = mounts_.find(abs_path.Range(0, max - len));
    if (it != mounts_.end()) {
      out_path->Set("/");
      out_path->Append(abs_path.Range(max - len, max));
      mount = it->second;
      break;
    }
  }

  if (NULL == mount) {
    errno = ENOTDIR;
    return NULL;
  }

  // Acquire the mount while we hold the proxy lock
  mount->Acquire();
  return mount;
}

void KernelObject::ReleaseMount(Mount* mnt) {
  AutoLock lock(&lock_);
  mnt->Release();
}

KernelHandle* KernelObject::AcquireHandle(int fd) {
  AutoLock lock(&lock_);
  if (fd < 0 || fd >= static_cast<int>(handle_map_.size())) {
    errno = EBADF;
    return NULL;
  }

  KernelHandle* handle = handle_map_[fd];
  if (NULL == handle) {
    errno = EBADF;
    return NULL;
  }

  // Ref count while holding parent mutex
  handle->Acquire();

  lock.Unlock();
  if (handle->node_) handle->mount_->AcquireNode(handle->node_);

  return handle;
}

void KernelObject::ReleaseHandle(KernelHandle* handle) {
  // The handle must already be held before taking the
  // kernel lock.
  if (handle->node_) handle->mount_->ReleaseNode(handle->node_);

  AutoLock lock(&lock_);
  handle->Release();
}

// Helper function to properly sort FD order in the heap, forcing
// lower numberd FD to be available first.
static bool FdOrder(int i, int j) {
  return i > j;
}

int KernelObject::AllocateFD(KernelHandle* handle) {
  AutoLock lock(&lock_);
  int id;

  // Acquire then handle and it's mount since we are about
  // to track them with this FD.
  handle->Acquire();
  handle->mount_->Acquire();

  // If we can recycle and FD, use that first
  if (free_fds_.size()) {
    id = free_fds_.front();
    std::pop_heap(free_fds_.begin(), free_fds_.end(), FdOrder);
    free_fds_.pop_back();
    handle_map_[id] = handle;
  } else {
    id = handle_map_.size();
    handle_map_.push_back(handle);
  }
  return id;
}

void KernelObject::FreeFD(int fd) {
  AutoLock lock(&lock_);

  // Release the mount and handle since we no longer
  // track them with this FD.
  KernelHandle* handle = handle_map_[fd];
  handle->mount_->Release();
  handle->Release();

  handle_map_[fd] = NULL;
  free_fds_.push_back(fd);
  std::push_heap(free_fds_.begin(), free_fds_.end(), FdOrder);
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

