/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#if defined(WIN32)
#define _CRT_RAND_S
#endif

#include "nacl_mounts/mount_dev.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "nacl_mounts/mount_node.h"
#include "nacl_mounts/mount_node_dir.h"
#include "utils/auto_lock.h"

#if defined(__native_client__)
#  include <irt.h>
#elif defined(WIN32)
#  include <stdlib.h>
#endif

namespace {

void ReleaseAndNullNode(MountNode** node) {
  if (*node)
    (*node)->Release();
  *node = NULL;
}

class NullNode : public MountNode {
 public:
  NullNode(Mount* mount, int ino, int dev);

  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Write(size_t offs, const void* buf, size_t count);
};

class ZeroNode : public MountNode {
 public:
  ZeroNode(Mount* mount, int ino, int dev);

  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Write(size_t offs, const void* buf, size_t count);
};

class UrandomNode : public MountNode {
 public:
  UrandomNode(Mount* mount, int ino, int dev);

  virtual int Read(size_t offs, void* buf, size_t count);
  virtual int Write(size_t offs, const void* buf, size_t count);

 private:
#if defined(__native_client__)
  nacl_irt_random random_interface_;
  bool interface_ok_;
#endif
};

NullNode::NullNode(Mount* mount, int ino, int dev)
    : MountNode(mount, ino, dev) {
}

int NullNode::Read(size_t offs, void* buf, size_t count) {
  return 0;
}

int NullNode::Write(size_t offs, const void* buf, size_t count) {
  return count;
}

ZeroNode::ZeroNode(Mount* mount, int ino, int dev)
    : MountNode(mount, ino, dev) {
}

int ZeroNode::Read(size_t offs, void* buf, size_t count) {
  memset(buf, 0, count);
  return count;
}

int ZeroNode::Write(size_t offs, const void* buf, size_t count) {
  return count;
}

UrandomNode::UrandomNode(Mount* mount, int ino, int dev)
    : MountNode(mount, ino, dev) {
#if defined(__native_client__)
  size_t result = nacl_interface_query(NACL_IRT_RANDOM_v0_1, &random_interface_,
                                       sizeof(random_interface_));
  interface_ok_ = result != 0;
#endif
}

int UrandomNode::Read(size_t offs, void* buf, size_t count) {
#if defined(__native_client__)
  if (interface_ok_) {
    size_t nread;
    int result = (*random_interface_.get_random_bytes)(buf, count, &nread);
    if (result != 0) {
      errno = result;
      return 0;
    }

    return count;
  }

  errno = EBADF;
  return 0;
#elif defined(WIN32)
  char* out = static_cast<char*>(buf);
  size_t bytes_left = count;
  while (bytes_left) {
    unsigned int random_int;
    errno_t err = rand_s(&random_int);
    if (err) {
      errno = err;
      return count - bytes_left;
    }

    int bytes_to_copy = std::min(bytes_left, sizeof(random_int));
    memcpy(out, &random_int, bytes_to_copy);
    out += bytes_to_copy;
    bytes_left -= bytes_to_copy;
  }

  return count;
#endif
}

int UrandomNode::Write(size_t offs, const void* buf, size_t count) {
  return count;
}

}  // namespace

MountNode *MountDev::Open(const Path& path, int mode) {
  AutoLock lock(&lock_);

  // Don't allow creating any files.
  if (mode & O_CREAT)
    return NULL;

  MountNode* node = root_->FindChild(path.Join());
  if (node)
    node->Acquire();
  return node;
}

int MountDev::Close(MountNode* node) {
  AutoLock lock(&lock_);
  node->Close();
  node->Release();
  return 0;
}

int MountDev::Unlink(const Path& path) {
  errno = EINVAL;
  return -1;
}

int MountDev::Mkdir(const Path& path, int permissions) {
  errno = EINVAL;
  return -1;
}

int MountDev::Rmdir(const Path& path) {
  errno = EINVAL;
  return -1;
}

int MountDev::Remove(const Path& path) {
  errno = EINVAL;
  return -1;
}

MountDev::MountDev()
    : null_node_(NULL),
      zero_node_(NULL),
      random_node_(NULL) {
}

bool MountDev::Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
  if (!Mount::Init(dev, args, ppapi))
    return false;

  root_ = new MountNodeDir(this, 1, dev_);
  null_node_ = new NullNode(this, 2, dev_);
  root_->AddChild("/null", null_node_);
  zero_node_ = new ZeroNode(this, 3, dev_);
  root_->AddChild("/zero", zero_node_);
  random_node_ = new UrandomNode(this, 4, dev_);
  root_->AddChild("/urandom", random_node_);
  return true;
}

void MountDev::Destroy() {
  ReleaseAndNullNode(&random_node_);
  ReleaseAndNullNode(&zero_node_);
  ReleaseAndNullNode(&null_node_);
  ReleaseAndNullNode(&root_);
}
