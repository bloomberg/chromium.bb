/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nacl_io/mount_mem.h"

#include <errno.h>
#include <fcntl.h>
#include <string>

#include "nacl_io/mount.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/mount_node_dir.h"
#include "nacl_io/mount_node_mem.h"
#include "nacl_io/osstat.h"
#include "nacl_io/path.h"
#include "utils/auto_lock.h"
#include "utils/ref_object.h"

// TODO(noelallen) : Grab/Redefine these in the kernel object once available.
#define USR_ID 1002
#define GRP_ID 1003

MountMem::MountMem()
    : root_(NULL),
      max_ino_(0) {
}

bool MountMem::Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
  Mount::Init(dev, args, ppapi);
  root_ = AllocatePath(S_IREAD | S_IWRITE);
  return (bool) (root_ != NULL);
}

void MountMem::Destroy() {
  if (root_)
    root_->Release();
  root_ = NULL;
}

MountNode* MountMem::AllocatePath(int mode) {
  MountNode *ptr = new MountNodeDir(this);
  if (!ptr->Init(mode)) {
    ptr->Release();
    return NULL;
  }
  return ptr;
}

MountNode* MountMem::AllocateData(int mode) {
  MountNode* ptr = new MountNodeMem(this);
  if (!ptr->Init(mode)) {
    ptr->Release();
    return NULL;
  }
  return ptr;
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
  if ((type & S_IFDIR) && !node->IsaDir()) {
    errno = ENOTDIR;
    return NULL;
  }

  // If a file is expected, but it's not a file, then fail.
  if ((type & S_IFREG) && node->IsaDir()) {
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
    MountNode* parent = FindNode(path.Parent(), S_IFDIR);
    if (NULL == parent) return NULL;

    // If the node does not exist and we can't create it, fail
    if ((mode & O_CREAT) == 0) return NULL;

    // Otherwise, create it with a single reference
    mode = OpenModeToPermission(mode);
    node = AllocateData(mode);
    if (NULL == node) return NULL;

    if (parent->AddChild(path.Basename(), node) == -1) {
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

  // Verify we got the requested permissions.
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

  MountNode* parent = FindNode(path.Parent(), S_IFDIR);
  MountNode* node;

  // If we failed to find the parent, the error code is already set.
  if (NULL == parent) return -1;

  node = parent->FindChild(path.Basename());
  if (NULL != node) {
    errno = EEXIST;
    return -1;
  }

  // Otherwise, create a new node and attempt to add it
  mode = OpenModeToPermission(mode);

  // Allocate a node, with a RefCount of 1.  If added to the parent
  // it will get ref counted again.  In either case, release the
  // recount we have on exit.
  node = AllocatePath(S_IREAD | S_IWRITE);
  if (NULL == node) return -1;

  if (parent->AddChild(path.Basename(), node) == -1) {
    node->Release();
    return -1;
  }

  node->Release();
  return 0;
}

int MountMem::Unlink(const Path& path) {
  return RemoveInternal(path, REMOVE_FILE);
}

int MountMem::Rmdir(const Path& path) {
  return RemoveInternal(path, REMOVE_DIR);
}

int MountMem::Remove(const Path& path) {
  return RemoveInternal(path, REMOVE_ALL);
}

int MountMem::RemoveInternal(const Path& path, int remove_type) {
  AutoLock lock(&lock_);
  bool dir_only = remove_type == REMOVE_DIR;
  bool file_only = remove_type == REMOVE_FILE;
  bool remove_dir = (remove_type & REMOVE_DIR) != 0;

  if (dir_only) {
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
  }

  MountNode* parent = FindNode(path.Parent(), S_IFDIR);

  // If we failed to find the parent, the error code is already set.
  if (NULL == parent) return -1;

  // Verify we find a child which is a directory.
  MountNode* child = parent->FindChild(path.Basename());
  if (NULL == child) {
    errno = ENOENT;
    return -1;
  }
  if (dir_only && !child->IsaDir()) {
    errno = ENOTDIR;
    return -1;
  }
  if (file_only && child->IsaDir()) {
    errno = EISDIR;
    return -1;
  }
  if (remove_dir && child->ChildCount() > 0) {
    errno = ENOTEMPTY;
    return -1;
  }
  return parent->RemoveChild(path.Basename());
}
