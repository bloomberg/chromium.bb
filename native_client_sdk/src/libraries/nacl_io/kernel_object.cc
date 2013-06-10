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
                                        Mount** out_mount,
                                        Path* out_path) {
  *out_mount = NULL;
  *out_path = Path();

  Path abs_path;
  {
    AutoLock lock(&process_lock_);
    abs_path = GetAbsPathLocked(relpath);
  }

  AutoLock lock(&kernel_lock_);
  Mount* mount = NULL;

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

  if (NULL == mount)
    return ENOTDIR;

  // Acquire the mount while we hold the proxy lock
  mount->Acquire();
  *out_mount = mount;
  return 0;
}

void KernelObject::ReleaseMount(Mount* mnt) {
  AutoLock lock(&kernel_lock_);
  mnt->Release();
}

Error KernelObject::AcquireHandle(int fd, KernelHandle** out_handle) {
  *out_handle = NULL;

  AutoLock lock(&process_lock_);
  if (fd < 0 || fd >= static_cast<int>(handle_map_.size()))
    return EBADF;

  KernelHandle* handle = handle_map_[fd];
  if (NULL == handle)
    return EBADF;

  // Ref count while holding parent mutex
  handle->Acquire();

  lock.Unlock();
  if (handle->node_)
    handle->mount_->AcquireNode(handle->node_);

  *out_handle = handle;
  return 0;
}

void KernelObject::ReleaseHandle(KernelHandle* handle) {
  // The handle must already be held before taking the
  // kernel lock.
  if (handle->node_)
    handle->mount_->ReleaseNode(handle->node_);

  AutoLock lock(&process_lock_);
  handle->Release();
}

int KernelObject::AllocateFD(KernelHandle* handle) {
  AutoLock lock(&process_lock_);
  int id;

  // Acquire the handle and its mount since we are about to track it with
  // this FD.
  handle->Acquire();
  handle->mount_->Acquire();

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

void KernelObject::FreeAndReassignFD(int fd, KernelHandle* handle) {
  if (NULL == handle) {
    FreeFD(fd);
  } else {
    AutoLock lock(&process_lock_);

    // Acquire the new handle first in case they are the same.
    if (handle) {
      handle->Acquire();
      handle->mount_->Acquire();
    }

    // If the required FD is larger than the current set, grow the set
    if (fd >= handle_map_.size()) {
      handle_map_.resize(fd + 1);
    } else {
      KernelHandle* old_handle = handle_map_[fd];
      if (NULL != old_handle) {
        old_handle->mount_->Release();
        old_handle->Release();
      }
    }
    handle_map_[fd] = handle;
  }
}

void KernelObject::FreeFD(int fd) {
  AutoLock lock(&process_lock_);

  // Release the mount and handle since we no longer
  // track them with this FD.
  KernelHandle* handle = handle_map_[fd];
  handle->Release();
  handle->mount_->Release();

  handle_map_[fd] = NULL;
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
