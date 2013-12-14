// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

namespace nacl_io {

KernelObject::KernelObject() {
  cwd_ = "/";
}

KernelObject::~KernelObject() {};

Error KernelObject::AttachMountAtPath(const ScopedMount& mnt,
                                const std::string& path) {
  std::string abs_path = GetAbsParts(path).Join();

  AUTO_LOCK(mount_lock_);
  if (mounts_.find(abs_path) != mounts_.end())
    return EBUSY;

  mounts_[abs_path] = mnt;
  return 0;
}

Error KernelObject::DetachMountAtPath(const std::string& path) {
  std::string abs_path = GetAbsParts(path).Join();

  AUTO_LOCK(mount_lock_);
  MountMap_t::iterator it = mounts_.find(abs_path);
  if (mounts_.end() == it)
    return EINVAL;

  // It is only legal to unmount if there are no open references
  if (it->second->RefCount() != 1)
    return EBUSY;

  mounts_.erase(it);
  return 0;
}

// Uses longest prefix to find the mount for the give path, then
// acquires the mount and returns it with a relative path.
Error KernelObject::AcquireMountAndRelPath(const std::string& path,
                                           ScopedMount* out_mount,
                                           Path* rel_parts) {
  Path abs_parts = GetAbsParts(path);

  out_mount->reset(NULL);
  *rel_parts = Path();

  AUTO_LOCK(mount_lock_);

  // Find longest prefix
  size_t max = abs_parts.Size();
  for (size_t len = 0; len < abs_parts.Size(); len++) {
    MountMap_t::iterator it = mounts_.find(abs_parts.Range(0, max - len));
    if (it != mounts_.end()) {
      rel_parts->Set("/");
      rel_parts->Append(abs_parts.Range(max - len, max));

      *out_mount = it->second;
      return 0;
    }
  }

  return ENOTDIR;
}

// Given a path, acquire the associated mount and node, creating the
// node if needed based on the provided flags.
Error KernelObject::AcquireMountAndNode(const std::string& path,
                                        int oflags,
                                        ScopedMount* out_mount,
                                        ScopedMountNode* out_node) {
  Path rel_parts;
  out_mount->reset(NULL);
  out_node->reset(NULL);
  Error error = AcquireMountAndRelPath(path, out_mount, &rel_parts);
  if (error)
    return error;

  error = (*out_mount)->Open(rel_parts, oflags, out_node);
  if (error)
    return error;

  return 0;
}

Path KernelObject::GetAbsParts(const std::string& path) {
  AUTO_LOCK(cwd_lock_);

  Path abs_parts(cwd_);
  if (path[0] == '/') {
    abs_parts = path;
  } else {
    abs_parts = cwd_;
    abs_parts.Append(path);
  }

  return abs_parts;
}

std::string KernelObject::GetCWD() {
  AUTO_LOCK(cwd_lock_);
  std::string out = cwd_;

  return out;
}

Error KernelObject::SetCWD(const std::string& path) {
  std::string abs_path = GetAbsParts(path).Join();

  ScopedMount mnt;
  ScopedMountNode node;

  Error error = AcquireMountAndNode(abs_path, O_RDONLY, &mnt, &node);
  if (error)
    return error;

  if ((node->GetType() & S_IFDIR) == 0)
    return ENOTDIR;

  AUTO_LOCK(cwd_lock_);
  cwd_ = abs_path;
  return 0;
}

Error KernelObject::GetFDFlags(int fd, int* out_flags) {
  AUTO_LOCK(handle_lock_);
  if (fd < 0 || fd >= static_cast<int>(handle_map_.size()))
    return EBADF;

  *out_flags = handle_map_[fd].flags;
  return 0;
}

Error KernelObject::SetFDFlags(int fd, int flags) {
  AUTO_LOCK(handle_lock_);
  if (fd < 0 || fd >= static_cast<int>(handle_map_.size()))
    return EBADF;

  // Only setting of FD_CLOEXEC is supported.
  if (flags & ~FD_CLOEXEC)
    return EINVAL;

  handle_map_[fd].flags = flags;
  return 0;
}

Error KernelObject::AcquireHandle(int fd, ScopedKernelHandle* out_handle) {
  out_handle->reset(NULL);

  AUTO_LOCK(handle_lock_);
  if (fd < 0 || fd >= static_cast<int>(handle_map_.size()))
    return EBADF;

  *out_handle = handle_map_[fd].handle;
  if (out_handle) return 0;

  return EBADF;
}

int KernelObject::AllocateFD(const ScopedKernelHandle& handle) {
  AUTO_LOCK(handle_lock_);
  int id;

  Descriptor_t descriptor(handle);

  // If we can recycle and FD, use that first
  if (free_fds_.size()) {
    id = free_fds_.front();
    // Force lower numbered FD to be available first.
    std::pop_heap(free_fds_.begin(), free_fds_.end(), std::greater<int>());
    free_fds_.pop_back();
    handle_map_[id] = descriptor;
  } else {
    id = handle_map_.size();
    handle_map_.push_back(descriptor);
  }
  return id;
}

void KernelObject::FreeAndReassignFD(int fd, const ScopedKernelHandle& handle) {
  if (NULL == handle) {
    FreeFD(fd);
  } else {
    AUTO_LOCK(handle_lock_);

    // If the required FD is larger than the current set, grow the set
    if (fd >= (int)handle_map_.size())
      handle_map_.resize(fd + 1);

    handle_map_[fd] = Descriptor_t(handle);
  }
}

void KernelObject::FreeFD(int fd) {
  AUTO_LOCK(handle_lock_);

  handle_map_[fd].handle.reset(NULL);
  free_fds_.push_back(fd);

  // Force lower numbered FD to be available first.
  std::push_heap(free_fds_.begin(), free_fds_.end(), std::greater<int>());
}

}  // namespace nacl_io
