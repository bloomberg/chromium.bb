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

namespace {

// TODO(binji): For now, just use a dummy value for the parent ino.
const ino_t kParentDirIno = -1;

}

MountNodeDir::MountNodeDir(Mount* mount)
    : MountNode(mount),
      cache_(stat_.st_ino, kParentDirIno),
      cache_built_(false) {
  SetType(S_IFDIR);
  // Directories are raadable, writable and executable by default.
  stat_.st_mode |= S_IRALL | S_IWALL | S_IXALL;
}

MountNodeDir::~MountNodeDir() {
  for (MountNodeMap_t::iterator it = map_.begin(); it != map_.end(); ++it) {
    it->second->Unlink();
  }
}

Error MountNodeDir::Read(const HandleAttr& attr,
                         void* buf,
                         size_t count,
                         int* out_bytes) {
  *out_bytes = 0;
  return EISDIR;
}

Error MountNodeDir::FTruncate(off_t size) { return EISDIR; }

Error MountNodeDir::Write(const HandleAttr& attr,
                          const void* buf,
                          size_t count,
                          int* out_bytes) {
  *out_bytes = 0;
  return EISDIR;
}

Error MountNodeDir::GetDents(size_t offs,
                             dirent* pdir,
                             size_t size,
                             int* out_bytes) {
  AUTO_LOCK(node_lock_);
  BuildCache_Locked();
  return cache_.GetDents(offs, pdir, size, out_bytes);
}

Error MountNodeDir::AddChild(const std::string& name,
                             const ScopedMountNode& node) {
  AUTO_LOCK(node_lock_);

  if (name.empty())
    return ENOENT;

  if (name.length() >= MEMBER_SIZE(dirent, d_name))
    return ENAMETOOLONG;

  MountNodeMap_t::iterator it = map_.find(name);
  if (it != map_.end())
    return EEXIST;

  node->Link();
  map_[name] = node;
  ClearCache_Locked();
  return 0;
}

Error MountNodeDir::RemoveChild(const std::string& name) {
  AUTO_LOCK(node_lock_);
  MountNodeMap_t::iterator it = map_.find(name);
  if (it != map_.end()) {
    it->second->Unlink();
    map_.erase(it);
    ClearCache_Locked();
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

void MountNodeDir::BuildCache_Locked() {
  if (cache_built_)
    return;

  for (MountNodeMap_t::iterator it = map_.begin(), end = map_.end();
       it != end; ++it) {
    const std::string& name = it->first;
    ino_t ino = it->second->stat_.st_ino;
    cache_.AddDirent(ino, name.c_str(), name.length());
  }

  cache_built_ = true;
}

void MountNodeDir::ClearCache_Locked() {
  cache_built_ = false;
  cache_.Reset();
}

}  // namespace nacl_io
