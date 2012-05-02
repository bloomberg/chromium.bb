/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>
#include <unistd.h>

#include "mount.h"
#include "mount_mem.h"
#include "mount_node.h"
#include "mount_node_dir.h"
#include "mount_node_mem.h"
#include "path.h"

#include "auto_lock.h"
#include "ref_object.h"

// TODO(noelallen) : Grab/Redefine these in the kernel object once available.
#define USR_ID 1002
#define GRP_ID 1003

MountMem::MountMem()
    : MountFactory(),
      root_(NULL),
      max_ino_(0) {
}

bool MountMem::Init(int dev, StringMap_t& args) { 
  dev_ = dev;
  root_ = AllocatePath(_S_IREAD | _S_IWRITE);
  return (bool) (root_ != NULL);
}

void MountMem::Destroy() {
  if (root_)
    root_->Release();
  root_ = NULL;
}

MountNode* MountMem::AllocatePath(int mode) {
  ino_t ino = AllocateINO();

  MountNode *ptr = new MountNodeDir(this, ino, dev_);
  if (!ptr->Init(mode, 1002, 1003)) { 
    ptr->Release();
    FreeINO(ino);
    return NULL;
  }
  return ptr;
}

MountNode* MountMem::AllocateData(int mode) {
  ino_t ino = AllocateINO();

  MountNode* ptr = new MountNodeMem(this, ino, dev_);
  if (!ptr->Init(mode, getuid(), getgid())) { 
    ptr->Release();
    FreeINO(ino);
    return NULL;
  }
  return ptr;
}

void MountMem::ReleaseNode(MountNode* node) {
  node->Release();
}

int MountMem::AllocateINO() {
  const int INO_CNT = 8;

  // If we run out of INO numbers, then allocate 8 more
  if (inos_.size() == 0) {
    max_ino_ += INO_CNT;
    // Add eight more to the stack in reverse order, offset by 1
    // since '0' refers to no INO.
    for (int a = 0; a < INO_CNT; a++) {
      inos_.push_back(max_ino_ - a);
    }
  }

  // Return the INO at the top of the stack.
  int val = inos_.back();
  inos_.pop_back();
  return val;
}

void MountMem::FreeINO(int ino) {
  inos_.push_back(ino);
}

MountNode* MountMem::FindNode(const Path& path, int type) {
  MountNode* node = root_;

  // If there is no root there, we have an error.
  if (node == NULL) {
    errno = ENOTDIR;
    return NULL;
  }

  // We are expecting an "absolute" path from this mount point.
  if (!path.IsAbsolute()) {
    errno = EINVAL;
    return NULL;
  }

  // Starting at the root, traverse the path parts.
  for (size_t index = 1; node && index < path.Size(); index++) {
    // If not a directory, then we have an error so return.
    if (!node->IsaDir()) {
      errno = ENOTDIR;
      return NULL;
    }

    // Find the child node
    node = node->FindChild(path.Part(index));
  }

  // node should be root, a found child, or a failed 'FindChild' 
  // which already has the correct errno set.
  if (NULL == node) return NULL;

  // If a directory is expected, but it's not a directory, then fail.
  if ((type & _S_IFDIR) && !node->IsaDir()) {
    errno = ENOTDIR;
    return NULL;
  }

  // If a file is expected, but it's not a file, then fail.
  if ((type & _S_IFREG) && node->IsaDir()) {
    errno = EISDIR;
    return NULL;
  }

  // We now have a valid object of the expected type, so return it.
  return node;
}

MountNode* MountMem::Open(const Path& path, int mode) {
  AutoLock lock(&lock_);
  MountNode* node = FindNode(path);

  if (NULL == node) {
    // Now first find the parent directory to see if we can add it
    MountNode* parent = FindNode(path.Parent(), _S_IFDIR);
    if (NULL == parent) return NULL;

    // If the node does not exist and we can't create it, fail
    if ((mode & O_CREAT) == 0) return NULL;

    // Otherwise, create it with a single refernece
    mode = OpenModeToPermission(mode);
    node = AllocateData(mode);
    if (NULL == node) return NULL;

    if (parent->AddChild(path.Filename(), node) == -1) {
      // Or if it fails, release it
      node->Release();
      return NULL;
    }
    return node;
  }

  // If we were expected to create it exclusively, fail
  if (mode & O_EXCL) {
    errno = EEXIST;
    return NULL;
  }

  // Verify we got the requested permisions.
  int req_mode = OpenModeToPermission(mode);
  int obj_mode = node->GetMode() & OpenModeToPermission(O_RDWR);
  if ((obj_mode & req_mode) != req_mode) {
    errno = EACCES;
    return NULL;
  }

  // We opened it, so ref count it before passing it back.
  node->Acquire();
  return node;
}

int MountMem::Close(MountNode* node) {
  AutoLock lock(&lock_);
  node->Close();
  ReleaseNode(node);
  return 0;
}

int MountMem::Unlink(const Path& path) {
  AutoLock lock(&lock_);
  MountNode* parent = FindNode(path.Parent(), _S_IFDIR);

  if (NULL == parent) return -1;

  MountNode* child = parent->FindChild(path.Filename());
  if (NULL == child) {
    errno = ENOENT;
    return -1;
  }
  if (child->IsaDir()) {
    errno = EISDIR;
    return -1;
  }
  return parent->RemoveChild(path.Filename());
}

int MountMem::Mkdir(const Path& path, int mode) {
  AutoLock lock(&lock_);

  // We expect a Mount "absolute" path
  if (!path.IsAbsolute()) {
    errno = ENOENT;
    return -1;
  }

  // The root of the mount is already created by the mount
  if (path.Size() == 1) {
    errno = EEXIST;
    return -1;
  }

  MountNode* parent = FindNode(path.Parent(), _S_IFDIR);
  MountNode* node;

  // If we failed to find the parent, the error code is already set.
  if (NULL == parent) return -1;

  node = parent->FindChild(path.Filename());
  if (NULL != node) {
    errno = EEXIST;
    return -1;
  }

  // Otherwise, create a new node and attempt to add it
  mode = OpenModeToPermission(mode);

  // Allocate a node, with a RefCount of 1.  If added to the parent
  // it will get ref counted again.  In either case, release the
  // recount we have on exit.
  node = AllocatePath(_S_IREAD | _S_IWRITE);
  if (NULL == node) return -1;

  if (parent->AddChild(path.Filename(), node) == -1) {
    node->Release();
    return -1;
  }

  node->Release();
  return 0;
}

int MountMem::Rmdir(const Path& path) {
  AutoLock lock(&lock_);

  // We expect a Mount "absolute" path
  if (!path.IsAbsolute()) {
    errno = ENOENT;
    return -1;
  }

  // The root of the mount is already created by the mount
  if (path.Size() == 1) {
    errno = EEXIST;
    return -1;
  }

  MountNode* parent = FindNode(path.Parent(), _S_IFDIR);
  MountNode* node;

  // If we failed to find the parent, the error code is already set.
  if (NULL == parent) return -1;

  // Verify we find a child which is also a directory
  node = parent->FindChild(path.Filename());
  if (NULL == node) {
    errno = ENOENT;
    return -1;
  }
  if (!node->IsaDir()) {
    errno = ENOTDIR;
    return -1;
  }
  if (node->ChildCount() > 0) {
    errno = ENOTEMPTY;
    return -1;
  }
  return parent->RemoveChild(path.Filename());
}
