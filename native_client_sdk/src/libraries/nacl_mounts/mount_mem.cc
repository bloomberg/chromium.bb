/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>

#include <vector>

#include "mount_mem.h"
#include "path.h"
#include "util/simple_auto_lock.h"

static const size_t s_blksize = 1024;

class MountNodeMem {
 public:
  MountNodeMem(int ino) {
    ino_ = ino;
    ref_count_ = 0;
    data_ = NULL;
    memset(&stat_, 0, sizeof(stat_));
    Resize(s_blksize);
  };

  size_t Size() const {
    return stat_.st_size;
  }

  void Resize(size_t size) {
    size_t cap = blocks_;
    size_t req = (size + s_blksize - 1) / s_blksize;

    if ((req > cap) || (req < (cap / 2))) {
      char *newdata = (char *) malloc(req * s_blksize);
      memcpy(newdata, data_, size);
      free(data_);
      stat_.st_size = size;
      data_ = newdata;
      blocks_ = req;
    }
  }

  int ino_;
  int ref_count_;
  struct stat stat_;
  void *data_;
  size_t blocks_;
  pthread_mutex_t lock_;
};

MountMem::MountMem() {}
MountMem::~MountMem() {}

int MountMem::AddDirEntry(MountMemNode* dir_node, MountMemNode* obj_node, const char *name) {
  if (strlen(name > 255)) {
    errno = EINVAL;
    return -1;
  }

  struct dirent* d = (struct dirent *) dir_node->data_;
  size_t cnt = dir_node->Size() / sizeof(struct dirent);
  size_t off;

  // Find a free location
  for (off = 0; off < cnt; off++) {
    if (d->d_name[0] == 0) break;
    d++;
  }

  // Otherwise regrow and take the last spot.
  if (off == cnt) {
    dir_node->Resize(dir_node->Size() + sizeof(struct dirent));
    d = &((struct dirent *) dir_node->data_)[off];
  }

  strcpy(d->d_name, name);
  d->d_ino = obj_node->ino_;
  d->d_reclen = sizeof(dirent);
  d->d_off = off * sizeof(dirent);

  // Add a ref_count_ and link count for the directory
  obj_node->Acquire();
  obj_node->stat_.st_nlink++;
  return 0;
}




int MountMem::DelDirEntry_locked(int dir_ino, const char *name) {
  MountMemNode* dir_node = inodes_.At(dir_ino);
  struct dirent* d = (struct dirent *) dir_node->data_;
  size_t cnt = dir_node->Size() / sizeof(struct dirent);
  size_t off = 0;

  // Find a free location
  for (off = 0; off < cnt; off++) {
    if (!strcmp(d->d_name, name)) {
      d->d_name[0] = 0;
      obj_node->stat_.st_nlink--;
      if (0 == --obj_node->ref_count_) FreeNode(obj_node->ino_);
      dir_node->ref_count_--;
      return 0;
    }
  }

  errno = ENOENT;
  return -1;
}

MountNodeMem* MountMem::AcquireNode(int ino) {
  SimpleAutoLock lock(&lock_);
  MountNodeMem *node = inodes_.At(ino);
  if (node) node->ref_count_++;
  return node;
}

void MountMem::ReleaseNode(MountMemNode* node) {
  if (node) {
    SimpleAutoLock lock(&lock_);
    if (--node->ref_count_) inodes_.Free(node->ino_);
  }
}

void MountMem::Init(void) {
  int root = inodes_.Alloc();

  assert(root == 0);
  AddDirEntry(root, root, "/");
}

int MountMem::Mkdir(const std::string& path, mode_t mode, struct stat *buf) {
  SimpleAutoLock lock(&lock_);

  int ino = AcquireNode(path);

  // Make sure it doesn't already exist.
  child = GetMemNode(path);
  if (child) {
    errno = EEXIST;
    return -1;
  }
  // Get the parent node.
  int parent_slot = GetParentSlot(path);
  if (parent_slot == -1) {
    errno = ENOENT;
    return -1;
  }
  parent = slots_.At(parent_slot);
  if (!parent->is_dir()) {
    errno = ENOTDIR;
    return -1;
  }

  // Create a new node
  int slot = slots_.Alloc();
  child = slots_.At(slot);
  child->set_slot(slot);
  child->set_mount(this);
  child->set_is_dir(true);
  Path p(path);
  child->set_name(p.Last());
  child->set_parent(parent_slot);
  parent->AddChild(slot);
  if (!buf) {
    return 0;
  }

  return Stat(slot, buf);
}

int MountMem::Rmdir(int node) {
}

int MountMem::Chmod(int ino, int mode) {
  MountMemNode* node = AcquireNode(ino);

  if (NULL == node) {
    errno = BADF;
    return -1;
  }

  node->stat_.st_mode = mode;
  ReleaseNode(node);
  return 0;
}

int MountMem::Stat(int ino, struct stat *buf) {
  MountMemNode* node = AcquireNode(ino);

  if (NULL == node) {
    errno = BADF;
    return -1;
  }

  memcpy(buf, node->stat_, sizeof(struct stat));
  ReleaseNode(node);
  return 0;
}

int MountMem::Fsync(int ino) {
  // Acquire the node in case he node
  MountMemNode* node = AcquireNode(ino);
  if (node) {
    ReleaseNode(node);
    return 0
  }

  errno = BADF;
  return -1;
}

int MountMem::Getdents(int ino, off_t offset, struct dirent *dirp, unsigned int count) {
  MountMemNode* node = AcquireNode(ino);
  if ((NULL == node) == 0) {
    errno = EBADF;
    return -1;
  }

  if ((node->stat_.st_mode & S_IFDIR) == 0) {
    errno =ENOTDIR;
    return -1;
  }

  if (offset + count > node->Size()) {
    count = node->Size() - offset;
  }

  memcpy(dirp, &node->data_[offset], count);
  return count;
}

ssize_t MountMem::Write(int ino, off_t offset, const void *buf, size_t count) {
  MountMemNode* node = AcquireNode(ino);

  if (NULL == node || (node->stat_.st_mode & S_IWUSER) == 0) {
    errno = EBADF;
    return -1;
  }

  if (offset + count > node->Size()) {
    int err = node->Resize();
    if (err) {
      errno = err;
      ReleaseNode(node);
      return -1;
    }
  }

  mempcy(&node->data_[offset], buf, count);
  ReleaseNode(node);
  return count;
}

ssize_t MountMem::Read(int ino, off_t offset, const void *buf, size_t count) {
  MountMemNode* node = AcquireNode(ino);

  if (NULL == node || (node->stat_.st_mode & S_IRUSER) == 0) {
    errno = EBADF;
    return -1;
  }

  if (offset + count > node->Size()) {
    count = node->Size() - offset;
  }

  mempcy(buf, &node->data_[offset], count);
  ReleaseNode(node);
  return count;
}

int MountMem::Isatty(int ino) {
  // Acquire the node in case he node array is in flux
  MountMemNode* node = AcquireNode(ino);

  if (node) {
    errno = ENOTTY;
    ReleaseNode(node);
  }
  else {
    errno = BADF;
  }
  return 0;
}

