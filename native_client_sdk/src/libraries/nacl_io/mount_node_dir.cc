// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_node_dir.h"

#include <errno.h>
#include <string.h>

#include "nacl_io/osdirent.h"
#include "nacl_io/osstat.h"
#include "sdk_util/auto_lock.h"
#include "sdk_util/macros.h"

namespace nacl_io {

MountNodeDir::MountNodeDir(Mount* mount) : MountNode(mount), cache_(NULL) {
  stat_.st_mode |= S_IFDIR;
}

MountNodeDir::~MountNodeDir() {
  for (MountNodeMap_t::iterator it = map_.begin(); it != map_.end(); ++it) {
    it->second->Unlink();
  }
  free(cache_);
}

Error MountNodeDir::Read(size_t offs, void* buf, size_t count, int* out_bytes) {
  *out_bytes = 0;
  return EISDIR;
}

Error MountNodeDir::FTruncate(off_t size) { return EISDIR; }

Error MountNodeDir::Write(size_t offs,
                          const void* buf,
                          size_t count,
                          int* out_bytes) {
  *out_bytes = 0;
  return EISDIR;
}

Error MountNodeDir::GetDents(size_t offs,
                             struct dirent* pdir,
                             size_t size,
                             int* out_bytes) {
  *out_bytes = 0;

  AUTO_LOCK(node_lock_);

  // If the buffer pointer is invalid, fail
  if (NULL == pdir)
    return EINVAL;

  // If the buffer is too small, fail
  if (size < sizeof(struct dirent))
    return EINVAL;

  // Force size to a multiple of dirent
  size -= size % sizeof(struct dirent);
  size_t max = map_.size() * sizeof(struct dirent);
  if (cache_ == NULL)
    BuildCache();

  if (offs >= max) {
    // OK, trying to read past the end.
    return 0;
  }

  if (offs + size >= max)
    size = max - offs;

  memcpy(pdir, ((char*)cache_) + offs, size);
  *out_bytes = size;
  return 0;
}

Error MountNodeDir::AddChild(const std::string& name,
                             const ScopedMountNode& node) {
  AUTO_LOCK(node_lock_);

  if (name.empty())
    return ENOENT;

  if (name.length() >= MEMBER_SIZE(struct dirent, d_name))
    return ENAMETOOLONG;

  MountNodeMap_t::iterator it = map_.find(name);
  if (it != map_.end())
    return EEXIST;

  node->Link();
  map_[name] = node;
  ClearCache();
  return 0;
}

Error MountNodeDir::RemoveChild(const std::string& name) {
  AUTO_LOCK(node_lock_);
  MountNodeMap_t::iterator it = map_.find(name);
  if (it != map_.end()) {
    it->second->Unlink();
    map_.erase(it);
    ClearCache();
    return 0;
  }
  return ENOENT;
}

Error MountNodeDir::FindChild(const std::string& name,
                              ScopedMountNode* out_node) {
  out_node->reset(NULL);

  AUTO_LOCK(node_lock_);
  MountNodeMap_t::iterator it = map_.find(name);
  if (it == map_.end())
    return ENOENT;

  *out_node = it->second;
  return 0;
}

int MountNodeDir::ChildCount() {
  AUTO_LOCK(node_lock_);
  return map_.size();
}

void MountNodeDir::ClearCache() {
  free(cache_);
  cache_ = NULL;
}

void MountNodeDir::BuildCache() {
  if (map_.size()) {
    cache_ = (struct dirent*)malloc(sizeof(struct dirent) * map_.size());
    MountNodeMap_t::iterator it = map_.begin();
    for (size_t index = 0; it != map_.end(); it++, index++) {
      size_t len = it->first.length();
      cache_[index].d_ino = it->second->stat_.st_ino;
      cache_[index].d_off = sizeof(struct dirent);
      cache_[index].d_reclen = sizeof(struct dirent);
      cache_[index].d_name[len] = 0;
      strncpy(cache_[index].d_name, &it->first[0], len);
    }
  }
}

}  // namespace nacl_io

