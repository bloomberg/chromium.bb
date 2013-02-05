/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nacl_io/mount_node_dir.h"

#include <errno.h>
#include <string.h>

#include "nacl_io/osdirent.h"
#include "nacl_io/osstat.h"
#include "utils/macros.h"
#include "utils/auto_lock.h"

MountNodeDir::MountNodeDir(Mount* mount)
    : MountNode(mount),
      cache_(NULL) {
  stat_.st_mode |= S_IFDIR;
}

MountNodeDir::~MountNodeDir() {
  free(cache_);
}

int MountNodeDir::Read(size_t offs, void *buf, size_t count) {
  errno = EISDIR;
  return -1;
}

int MountNodeDir::Truncate(size_t size) {
  errno = EISDIR;
  return -1;
}

int MountNodeDir::Write(size_t offs, void *buf, size_t count) {
  errno = EISDIR;
  return -1;
}

int MountNodeDir::GetDents(size_t offs, struct dirent* pdir, size_t size) {
  AutoLock lock(&lock_);

  // If the buffer pointer is invalid, fail
  if (NULL == pdir) {
    errno = EINVAL;
    return -1;
  }

  // If the buffer is too small, fail
  if (size < sizeof(struct dirent)) {
    errno = EINVAL;
    return -1;
  }

  // Force size to a multiple of dirent
  size -= size % sizeof(struct dirent);
  size_t max = map_.size() * sizeof(struct dirent);
  if (cache_ == NULL) BuildCache();

  if (offs >= max) return 0;
  if (offs + size >= max) size = max - offs;

  memcpy(pdir, ((char *) cache_) + offs, size);
  return size;
}

int MountNodeDir::AddChild(const std::string& name, MountNode* node) {
  AutoLock lock(&lock_);

  if (name.empty()) {
    errno = ENOENT;
    return -1;
  }
  if (name.length() >= MEMBER_SIZE(struct dirent, d_name)) {
    errno = ENAMETOOLONG;
    return -1;
  }

  MountNodeMap_t::iterator it = map_.find(name);
  if (it != map_.end()) {
    errno = EEXIST;
    return -1;
  }

  node->Link();
  map_[name] = node;
  ClearCache();
  return 0;
}

int MountNodeDir::RemoveChild(const std::string& name) {
  AutoLock lock(&lock_);
  MountNodeMap_t::iterator it = map_.find(name);
  if (it != map_.end()) {
    it->second->Unlink();
    map_.erase(it);
    ClearCache();
    return 0;
  }
  errno = ENOENT;
  return -1;
}

MountNode* MountNodeDir::FindChild(const std::string& name) {
  AutoLock lock(&lock_);
  MountNodeMap_t::iterator it = map_.find(name);
  if (it != map_.end()) {
    return it->second;
  }
  errno = ENOENT;
  return NULL;
}

int MountNodeDir::ChildCount() {
  AutoLock lock(&lock_);
  return map_.size();
}

void MountNodeDir::ClearCache() {
  free(cache_);
  cache_ = NULL;
}

void MountNodeDir::BuildCache() {
  if (map_.size()) {
    cache_ = (struct dirent *) malloc(sizeof(struct dirent) * map_.size());
    MountNodeMap_t::iterator it = map_.begin();
    for (size_t index = 0; it != map_.end(); it++, index++) {
      MountNode* node = it->second;
      size_t len = it->first.length();
      cache_[index].d_ino = node->stat_.st_ino;
      cache_[index].d_reclen = sizeof(struct dirent);
      cache_[index].d_name[len] = 0;
      strncpy(cache_[index].d_name, &it->first[0], len);
    }
  }
}
