/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "nacl_io/mount_node.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <string>

#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/mount.h"
#include "nacl_io/osmman.h"
#include "utils/auto_lock.h"

static const int USR_ID = 1001;
static const int GRP_ID = 1002;

MountNode::MountNode(Mount* mount)
    : mount_(mount) {
  memset(&stat_, 0, sizeof(stat_));
  stat_.st_gid = GRP_ID;
  stat_.st_uid = USR_ID;

  // Mount should normally never be NULL, but may be null in tests.
  if (mount_)
    mount_->OnNodeCreated(this);
}

MountNode::~MountNode() {
}

bool MountNode::Init(int perm) {
  stat_.st_mode |= perm;
  return true;
}

void MountNode::Destroy() {
  if (mount_) {
    mount_->OnNodeDestroyed(this);
  }
}

int MountNode::FSync() {
  return 0;
}

int MountNode::GetDents(size_t offs, struct dirent* pdir, size_t count) {
  errno = ENOTDIR;
  return -1;
}

int MountNode::GetStat(struct stat* pstat) {
  AutoLock lock(&lock_);
  memcpy(pstat, &stat_, sizeof(stat_));
  return 0;
}

int MountNode::Ioctl(int request, char* arg) {
  errno = EINVAL;
  return -1;
}

int MountNode::Read(size_t offs, void* buf, size_t count) {
  errno = EINVAL;
  return -1;
}

int MountNode::Truncate(size_t size) {
  errno = EINVAL;
  return -1;
}

int MountNode::Write(size_t offs, const void* buf, size_t count) {
  errno = EINVAL;
  return -1;
}

void* MountNode::MMap(void* addr, size_t length, int prot, int flags,
                      size_t offset) {
  // This default mmap support is just enough to make dlopen work.
  // This implementation just reads from the mount into the mmap'd memory area.
  void* new_addr = addr;
  int err = _real_mmap(&new_addr, length, prot | PROT_WRITE, flags |
                       MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED) {
    _real_munmap(addr, length);
    errno = err;
    return MAP_FAILED;
  }

  ssize_t cnt = Read(offset, addr, length);
  if (cnt == -1) {
    _real_munmap(addr, length);
    errno = ENOSYS;
    return MAP_FAILED;
  }

  return new_addr;
}

int MountNode::Munmap(void* addr, size_t length) {
  return _real_munmap(addr, length);
}

int MountNode::GetLinks() {
  return stat_.st_nlink;
}

int MountNode::GetMode() {
  return stat_.st_mode & ~S_IFMT;
}

size_t MountNode::GetSize() {
  return stat_.st_size;
}

int MountNode::GetType() {
  return stat_.st_mode & S_IFMT;
}

bool MountNode::IsaDir() {
  return (stat_.st_mode & S_IFDIR) != 0;
}

bool MountNode::IsaFile() {
  return (stat_.st_mode & S_IFREG) != 0;
}

bool MountNode::IsaTTY() {
  return (stat_.st_mode & S_IFCHR) != 0;
}


int MountNode:: AddChild(const std::string& name, MountNode* node) {
  errno = ENOTDIR;
  return -1;
}

int MountNode::RemoveChild(const std::string& name) {
  errno = ENOTDIR;
  return -1;
}

MountNode* MountNode::FindChild(const std::string& name) {
  errno = ENOTDIR;
  return NULL;
}

int MountNode::ChildCount() {
  errno = ENOTDIR;
  return -1;
}

void MountNode::Link() {
  Acquire();
  stat_.st_nlink++;
}

void MountNode::Unlink() {
  stat_.st_nlink--;
  Release();
}
