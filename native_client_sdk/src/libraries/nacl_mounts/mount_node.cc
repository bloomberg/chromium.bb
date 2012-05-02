/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>

#include "auto_lock.h"
#include "mount.h"
#include "mount_node.h"

MountNode::MountNode(Mount* mount, int ino, int dev)
    : mount_(mount) {
  memset(&stat_, 0, sizeof(stat_));
  stat_.st_ino = ino;
  stat_.st_dev = dev;
}

MountNode::~MountNode() {
}

bool MountNode::Init(int mode, short uid, short gid) {
  stat_.st_mode = mode;
  stat_.st_gid = gid;
  stat_.st_uid = uid;
  return true;
}

int MountNode::Close() {
  FSync();
  return 0;
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

int MountNode::GetLinks() const {
  return stat_.st_nlink;
}

int MountNode::GetMode() const {
  return stat_.st_mode & ~_S_IFMT;
}

size_t MountNode::GetSize() const {
  return stat_.st_size;
}

int MountNode::GetType() const {
  return stat_.st_mode & _S_IFMT;
}

bool MountNode::IsaDir() const {
  return (stat_.st_mode & _S_IFDIR) != 0;
}

bool MountNode::IsaFile() const {
  return (stat_.st_mode & _S_IFREG) != 0;
}

bool MountNode::IsaTTY() const {
  return (stat_.st_mode & _S_IFCHR) != 0;
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

